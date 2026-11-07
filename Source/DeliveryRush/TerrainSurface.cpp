#include "TerrainSurface.h"
#include <algorithm>
#include <cmath>
namespace Delivery
{
namespace
{
double VertexHeight(double x,double y)
{
    double height=0;
    for(const auto& hill:TerrainHills)
    {
        const double dx=(x-hill.x)/hill.rx,dy=(y-hill.y)/hill.ry;
        const double influence=std::max(0.0,1-dx*dx-dy*dy);
        height+=hill.height*influence*influence*influence;
    }
    return height;
}
}
double GroundHeight(double x,double y)
{
    constexpr int Side=int(2*TerrainExtent/TerrainGridStep)+1;
    static const auto heights=[]
    {
        std::array<double,Side*Side> result{};
        for(int ix=0;ix<Side;++ix) for(int iy=0;iy<Side;++iy)
            result[ix*Side+iy]=VertexHeight(-TerrainExtent+ix*TerrainGridStep,-TerrainExtent+iy*TerrainGridStep);
        return result;
    }();
    x=std::clamp(x,-TerrainExtent,TerrainExtent);y=std::clamp(y,-TerrainExtent,TerrainExtent);
    const double gx=std::min(std::floor((x+TerrainExtent)/TerrainGridStep),2*TerrainExtent/TerrainGridStep-1);
    const double gy=std::min(std::floor((y+TerrainExtent)/TerrainGridStep),2*TerrainExtent/TerrainGridStep-1);
    const double x0=-TerrainExtent+gx*TerrainGridStep,y0=-TerrainExtent+gy*TerrainGridStep;
    const double u=(x-x0)/TerrainGridStep,v=(y-y0)/TerrainGridStep;
    const int index=int(gx)*Side+int(gy);
    const double h00=heights[index],h10=heights[index+Side];
    const double h01=heights[index+1],h11=heights[index+Side+1];
    // Shared diagonal from (0,0) to (1,1), matching the generated mesh quads.
    return u>=v?h00*(1-u)+h10*(u-v)+h11*v:h00*(1-v)+h01*(v-u)+h11*u;
}
GroundPose SurfacePose(double x,double y,double heading,double halfLength,double halfWidth)
{
    const double fx=std::cos(heading),fy=std::sin(heading);
    const double front=GroundHeight(x+fx*halfLength,y+fy*halfLength);
    const double rear=GroundHeight(x-fx*halfLength,y-fy*halfLength);
    const double right=GroundHeight(x-fy*halfWidth,y+fx*halfWidth);
    const double left=GroundHeight(x+fy*halfWidth,y-fx*halfWidth);
    return {(front+rear)*.5,std::atan2(front-rear,2*halfLength),std::atan2(right-left,2*halfWidth)};
}
}
