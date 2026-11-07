#include "LevelLayout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace Delivery
{
namespace
{
Vec2 Local(Vec2 p,const Building& b)
{
    const double c=std::cos(b.yaw),s=std::sin(b.yaw);
    p=p-Vec2{b.x,b.y};return {c*p.x+s*p.y,-s*p.x+c*p.y};
}
Vec2 Rotate(Vec2 p,double angle)
{ return {std::cos(angle)*p.x-std::sin(angle)*p.y,std::sin(angle)*p.x+std::cos(angle)*p.y}; }
double RoadDot(Vec2 a,Vec2 b) { return a.x*b.x+a.y*b.y; }
double RoadDistance(Vec2 point) { return std::hypot(point.x,point.y); }
bool IsInRoad(const LevelLayout& level,Vec2 point)
{
    for(const auto& road:level.paths)
    {
        const Vec2 start{road.ax,road.ay},delta=Vec2{road.bx,road.by}-start;
        const double lengthSquared=RoadDot(delta,delta);
        if(lengthSquared<.01) continue;
        const double progress=std::clamp(RoadDot(point-start,delta)/lengthSquared,0.,1.);
        const Vec2 offset=point-(start+delta*progress);
        const double clearance=road.width*.5+.4;
        if(RoadDot(offset,offset)<clearance*clearance) return true;
    }
    return false;
}
}
Vec2 ObstaclePush(Vec2 p,const Building& b,double radius)
{
    p=Local(p,b);
    const Vec2 nearest{std::clamp(p.x,-b.hx,b.hx),std::clamp(p.y,-b.hy,b.hy)};
    const Vec2 delta=p-nearest;const double d=std::hypot(delta.x,delta.y);
    if(d>=radius) return {};
    Vec2 push;
    if(d>1e-8) push=delta*((radius-d)/d);
    else
    {
        const double dx=b.hx+radius-std::abs(p.x),dy=b.hy+radius-std::abs(p.y);
        push=dx<dy?Vec2{p.x>=0?dx:-dx,0}:Vec2{0,p.y>=0?dy:-dy};
    }
    return Rotate(push,b.yaw);
}
bool LevelLayout::Blocked(Vec2 p,double radius) const
{
    for(const auto& b:obstacles)
    { const Vec2 push=ObstaclePush(p,b,radius);if(std::hypot(push.x,push.y)>1e-7) return true; }
    return false;
}
bool LevelLayout::Clear(Vec2 a,Vec2 b,double padding) const
{
    for(const auto& box:obstacles)
    {
        const Vec2 origin=Local(a,box),end=Local(b,box),d=end-origin;
        double lo=0,hi=1;bool intersects=true;
        for(int axis=0;axis<2;++axis)
        {
            const double o=axis==0?origin.x:origin.y,delta=axis==0?d.x:d.y;
            const double half=(axis==0?box.hx:box.hy)+padding;
            if(std::abs(delta)<1e-9) { if(std::abs(o)>half) { intersects=false;break; } }
            else
            {
                double t0=(-half-o)/delta,t1=(half-o)/delta;
                if(t0>t1) std::swap(t0,t1);
                lo=std::max(lo,t0);hi=std::min(hi,t1);
                if(lo>hi) { intersects=false;break; }
            }
        }
        if(intersects) return false;
    }
    return true;
}
std::vector<WalkRoute> RoadsideWalks(const LevelLayout& level,Vec2 junction,unsigned seed)
{
    std::mt19937 random(seed);
    std::vector<WalkRoute> walks;
    const std::array<Vec2,3> deliveryRoute{level.start,junction,level.goal};
    for(std::size_t leg=0;leg+1<deliveryRoute.size();++leg)
    {
        const Vec2 from=deliveryRoute[leg], delta=deliveryRoute[leg+1]-from;
        const double length=RoadDistance(delta);
        if(length<30) continue;
        const Vec2 along=delta*(1/length),side{-along.y,along.x};
        for(double edge:{-1.,1.})
        {
            std::vector<Vec2> candidates;
            for(const auto& road:level.paths)
            {
                const Vec2 roadStart{road.ax,road.ay},roadEnd{road.bx,road.by};
                if(std::abs(RoadDot(roadStart-from,side))>1||std::abs(RoadDot(roadEnd-from,side))>1) continue;
                const Vec2 middle=(roadStart+roadEnd)*.5;
                const double distance=RoadDot(middle-from,along);
                if(distance<14||distance>length-14) continue;
                const Vec2 curb=middle+side*(edge*(road.width*.5+1.3));
                const Vec2 a=curb-along*6,b=curb+along*6;
                if(std::abs(a.x)>level.limit-1||std::abs(a.y)>level.limit-1||
                    std::abs(b.x)>level.limit-1||std::abs(b.y)>level.limit-1||!level.Clear(a,b,.65)) continue;
                bool crossesRoad=false;
                for(int step=0;step<=12&&!crossesRoad;++step) crossesRoad=IsInRoad(level,a+along*step);
                if(!crossesRoad) candidates.push_back(curb);
            }
            std::shuffle(candidates.begin(),candidates.end(),random);
            const std::size_t maxWalks=walks.size()+3;
            for(Vec2 curb:candidates)
            {
                if(std::any_of(walks.begin(),walks.end(),[&](const WalkRoute& walk)
                    { return RoadDistance(curb-(walk.start+walk.end)*.5)<15; })) continue;
                walks.push_back({curb-along*6,curb+along*6});
                if(walks.size()==maxWalks) break;
            }
        }
    }
    return walks;
}
void LevelLayout::BuildNavigation(bool addObstacleCorners)
{
    if(addObstacleCorners) for(const auto& b:obstacles)
        for(double x:{-1.,1.}) for(double y:{-1.,1.})
            nodes.push_back(Vec2{b.x,b.y}+Rotate({x*(b.hx+3),y*(b.hy+3)},b.yaw));
    std::vector<Vec2> valid;
    for(Vec2 p:nodes)
    {
        if(std::abs(p.x)>limit-2.5||std::abs(p.y)>limit-2.5||Blocked(p,2.2)) continue;
        bool duplicate=false;for(Vec2 q:valid) if(std::hypot(p.x-q.x,p.y-q.y)<.5) { duplicate=true;break; }
        if(!duplicate) valid.push_back(p);
    }
    nodes=std::move(valid);
    edges.assign(nodes.size(),std::vector<double>(nodes.size(),1e12));
    for(std::size_t i=0;i<nodes.size();++i) for(std::size_t j=i+1;j<nodes.size();++j)
        if(Clear(nodes[i],nodes[j],2.2)) edges[i][j]=edges[j][i]=std::hypot(nodes[i].x-nodes[j].x,nodes[i].y-nodes[j].y);
}
std::shared_ptr<const LevelLayout> DefaultLevelLayout()
{
    static const auto layout=[]
    {
        auto level=std::make_shared<LevelLayout>();
        level->obstacles.assign(CityObstacles.begin(),CityObstacles.end());
        for(const auto& n:NavigationNodes) level->nodes.push_back({n.x,n.y});
        for(int road=-180;road<=180;road+=90) for(int along=-190;along<=190;along+=10)
            for(double lane:{-3.6,3.6})
            { level->spawnPoints.push_back({double(along),double(road)+lane});level->spawnPoints.push_back({double(road)+lane,double(along)}); }
        level->paths.assign(ParkPathSegments.begin(),ParkPathSegments.end());
        for(double alley:{-135.,-45.}) level->paths.push_back({alley,-180,alley,180,8.5});
        level->BuildNavigation(false);return level;
    }();return layout;
}
GroundPose PoseOnLevel(const LevelLayout& level,double x,double y,double heading,double halfLength,double halfWidth)
{
    const double fx=std::cos(heading),fy=std::sin(heading);
    const double front=level.height(x+fx*halfLength,y+fy*halfLength),rear=level.height(x-fx*halfLength,y-fy*halfLength);
    const double right=level.height(x-fy*halfWidth,y+fx*halfWidth),left=level.height(x+fy*halfWidth,y-fx*halfWidth);
    return {(front+rear)*.5,std::atan2(front-rear,2*halfLength),std::atan2(right-left,2*halfWidth)};
}
}
