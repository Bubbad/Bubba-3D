#ifndef __COLLIDER_H__
#define __COLLIDER_H__

#include "Scene.h"

bool octreeOctreeIntersection(Octree *object1Octree, float4x4 *object1ModelMatrix, Octree *object2Octree,
							  float4x4 *object2ModelMatrix);
bool AabbAabbintersection(AABB *aabb1, AABB *aabb2);
bool sphereIntersection(Sphere sphere1,Sphere sphere2);
AABB multiplyAABBWithModelMatrix(AABB *aabb, float4x4 modelMatrix);
bool triangleTriangleIntersection(Triangle *t1, Triangle *t2);
bool isTrianglesIntersecting(Octree *object1Octree, float4x4 *object1ModelMatrix, Octree *object2Octree,
							 float4x4 *object2ModelMatrix);


class Collider
{
public:
	virtual void updateCollision(Scene *scene) = 0;

private:

};

#endif //__COLLIDER_H__