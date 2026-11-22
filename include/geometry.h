#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <sys/types.h>
#include <libgte.h>
#include "collision_scratch.h"
#include "world_vector.h"

int BuildTriangleFrame(CollisionWork *frame);
int TestSphereVsAlignedSegment(int reusePoint, CollisionWork *work, WorldVector *center, int radius, int radiusSquared);
int SegmentIntersectsCircleXY(int radius, int radiusSquared, WorldVector *start, WorldVector *end);
int TestSphereVsAlignedTriangle(int reusePoint, CollisionWork *work, WorldVector *center,
                                int radius, int radiusSquared);
int BuildNonDegenerateTriangleFrame(CollisionWork *work);
int TestTriangleIntersectsAlignedTriangle(CollisionWork *work, WorldVector *vertexA,
                                          WorldVector *vertexB, WorldVector *vertexC);
int IsPointInTriangleXZ(WorldVector *vertexA, WorldVector *vertexB, WorldVector *vertexC, WorldVector *probe);
void BuildSegmentFrame(CollisionWork *work);
int TestSphereVsSegmentFrame(CollisionWork *work, WorldVector *center, int radius, int radiusSquared);
int GetTriangleNormalHeading(VECTOR *origin, VECTOR *vertexA, VECTOR *vertexB);
struct Fighter;
int IsFighterSupine(struct Fighter *fighter);
int IsWeaponSwingPlaneVertical(struct Fighter *fighter);

#endif
