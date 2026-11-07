#pragma once
#include "TerrainSurface.h"
#include <functional>
#include <memory>
#include <vector>

namespace Delivery
{
struct Vec2
{
    double x = 0, y = 0;
    Vec2 operator+(Vec2 v) const { return {x+v.x,y+v.y}; }
    Vec2 operator-(Vec2 v) const { return {x-v.x,y-v.y}; }
    Vec2 operator*(double s) const { return {x*s,y*s}; }
};
struct LevelLayout
{
    std::vector<Building> obstacles;
    std::vector<Vec2> nodes, spawnPoints;
    std::vector<Vec2> bridgeCheckpoints;
    std::vector<PathSegment> paths;
    std::vector<std::vector<double>> edges;
    Vec2 start{StartX,StartY}, goal{DestinationX,DestinationY};
    double startHeading=0, limit=WorldLimit, timeLimit=DeliveryTime;
    std::function<double(double,double)> height=GroundHeight;
    bool Blocked(Vec2 point,double radius=.55) const;
    bool Clear(Vec2 a,Vec2 b,double padding=.6) const;
    void BuildNavigation(bool addObstacleCorners=true);
};
std::shared_ptr<const LevelLayout> DefaultLevelLayout();
struct WalkRoute { Vec2 start, end; };
std::vector<WalkRoute> RoadsideWalks(const LevelLayout& level,Vec2 junction,unsigned seed);
Vec2 ObstaclePush(Vec2 point,const Building& box,double radius);
GroundPose PoseOnLevel(const LevelLayout& level,double x,double y,double heading,double halfLength,double halfWidth);
}
