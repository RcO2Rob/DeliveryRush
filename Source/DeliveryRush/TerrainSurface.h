#pragma once
#include "CityLayoutData.h"

namespace Delivery
{
// The mesh and every moving actor sample this same triangulated height field.
double GroundHeight(double x,double y);
struct GroundPose { double height=0, pitch=0, bank=0; };
GroundPose SurfacePose(double x,double y,double heading,double halfLength,double halfWidth);
}
