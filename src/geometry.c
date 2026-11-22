/* GTE-accelerated 3D collision geometry: triangle/segment alignment frames, sphere and triangle
 * intersection tests on the combat scratchpad, and fighter torso/weapon-plane orientation queries. */

#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>

#include "fighter.h"
#include "collision_scratch.h"
#include "cloth.h"
#include "geometry.h"

#define SQRT_LUT_SIZE               1024  /* gSqrtLutQ3 entry count: floor(sqrt(i) * 8) */
#define NORMAL_GTE_SAFE_MAX         16383 /* max component for unscaled GTE sqr0 without saturation */
#define NORMAL_DEGENERATE_LEN_MIN   16384 /* below this |normal|, BuildTriangleFrame falls back to edgeA */
#define NORMAL_SCALE_LIMIT          16384 /* halve normal while any component exceeds this so |n|^2 fits in int */
#define SEGMENT_PARAM_ONE_Q8        256   /* 1.0 in Q8 segment parameter t */
#define VERTICAL_SWING_NORMAL_Y_MAX 2896  /* floor(4096 * cos(45 deg)): swing plane within 45 deg of vertical */

static void SetIdentityRotMatrix(MATRIX *rotation)
{
    rotation->m[0][0] = ONE;
    rotation->m[0][1] = 0;
    rotation->m[0][2] = 0;
    rotation->m[1][0] = 0;
    rotation->m[1][1] = ONE;
    rotation->m[1][2] = 0;
    rotation->m[2][0] = 0;
    rotation->m[2][1] = 0;
    rotation->m[2][2] = ONE;
}

/* Build a GTE rotation frame aligning a triangle into XY (return 1), or falling back to edgeA along Z
 * when |normal| is too small (return 0). */
int BuildTriangleFrame(CollisionWork *frame)
{
    int normalSquared, edgeSquared, scaledRootInput, length, scaledHorizontal, edgeYaw, scaledYaw;
    int normalLeading, edgeLeading, scaledLeading;
    MATRIX *rotation, *normalRotation;
    WorldVector *edgeA, *edgeB;

    frame->edgeA.x = frame->primaryVerts[1]->x - frame->primaryVerts[0]->x;
    frame->edgeA.y = frame->primaryVerts[1]->y - frame->primaryVerts[0]->y;
    frame->edgeA.z = frame->primaryVerts[1]->z - frame->primaryVerts[0]->z;
    frame->edgeB.x = frame->primaryVerts[2]->x - frame->primaryVerts[0]->x;
    frame->edgeB.y = frame->primaryVerts[2]->y - frame->primaryVerts[0]->y;
    frame->edgeB.z = frame->primaryVerts[2]->z - frame->primaryVerts[0]->z;
    gte_ldopv1(&frame->edgeA);
    gte_ldopv2(&frame->edgeB);
    gte_op0();
    gte_stlvnl(&frame->normalOrTarget);
    if (frame->normalOrTarget.x >= -NORMAL_GTE_SAFE_MAX && frame->normalOrTarget.x <= NORMAL_GTE_SAFE_MAX &&
        frame->normalOrTarget.y >= -NORMAL_GTE_SAFE_MAX && frame->normalOrTarget.y <= NORMAL_GTE_SAFE_MAX &&
        frame->normalOrTarget.z >= -NORMAL_GTE_SAFE_MAX && frame->normalOrTarget.z <= NORMAL_GTE_SAFE_MAX) {
        gte_sqr0();
        gte_stlvnl(&frame->localPoint);
        normalSquared = frame->localPoint.vx + frame->localPoint.vy + frame->localPoint.vz;
        if ((unsigned int)normalSquared < SQRT_LUT_SIZE) {
            length = gSqrtLutQ3[normalSquared] >> 3;
        } else {
            int leading = 0;
            unsigned int shift;
            if (normalSquared >= 0) {
                gte_ldlzc(normalSquared);
                gte_nop();
                gte_nop();
                gte_stlzc(&normalLeading);
                leading = normalLeading;
            }
            shift = 22 - (leading & ~1);
            normalSquared = gSqrtLutQ3[(unsigned int)normalSquared >> shift];
            length = ((unsigned int)normalSquared << 16) >> (19 - (shift >> 1));
        }
        if ((unsigned int)length < NORMAL_DEGENERATE_LEN_MIN) {
            edgeYaw = ratan2(frame->edgeA.x, frame->edgeA.z);
            edgeSquared = frame->edgeA.x * frame->edgeA.x + frame->edgeA.z * frame->edgeA.z;
            frame->anglesOrOffset.vy = ANGLE_HALF - edgeYaw;
            if ((unsigned int)edgeSquared < SQRT_LUT_SIZE) {
                length = gSqrtLutQ3[edgeSquared] >> 3;
            } else {
                int leading = 0;
                unsigned int shift;
                if (edgeSquared >= 0) {
                    gte_ldlzc(edgeSquared);
                    gte_nop();
                    gte_nop();
                    gte_stlzc(&edgeLeading);
                    leading = edgeLeading;
                }
                shift = 22 - (leading & ~1);
                edgeSquared = gSqrtLutQ3[(unsigned int)edgeSquared >> shift];
                length = ((unsigned int)edgeSquared << 16) >> (19 - (shift >> 1));
            }
            rotation = &frame->rotation;
            frame->anglesOrOffset.vx = ANGLE_HALF - ratan2(frame->edgeA.y, length);
            SetIdentityRotMatrix(rotation);
            RotMatrixY(frame->anglesOrOffset.vy, rotation);
            RotMatrixX(frame->anglesOrOffset.vx, rotation);
            gte_SetRotMatrix(rotation);
            edgeA = &frame->edgeA;
            gte_ldlv0(edgeA);
            gte_rtv0();
            gte_stlvnl(edgeA);
            edgeB = &frame->edgeB;
            /* gte_ldv0 on three words: the GTE gets x's low and high halves and y's low half, not edgeB. */
            gte_ldv0(edgeB);
            gte_rtv0();
            gte_stlvnl(edgeB);
            return 0;
        }
    }

    scaledYaw = ratan2(frame->normalOrTarget.x, frame->normalOrTarget.z);
    frame->normalOrTarget.x >>= 6;
    frame->normalOrTarget.y >>= 6;
    frame->normalOrTarget.z >>= 6;
    scaledRootInput = frame->normalOrTarget.x * frame->normalOrTarget.x +
                      frame->normalOrTarget.z * frame->normalOrTarget.z;
    frame->anglesOrOffset.vy = ANGLE_HALF - scaledYaw;
    if ((unsigned int)scaledRootInput < SQRT_LUT_SIZE) {
        scaledHorizontal = gSqrtLutQ3[scaledRootInput] >> 3;
    } else {
        int leading = 0;
        unsigned int shift;
        if (scaledRootInput >= 0) {
            gte_ldlzc(scaledRootInput);
            gte_nop();
            gte_nop();
            gte_stlzc(&scaledLeading);
            leading = scaledLeading;
        }
        shift = 22 - (leading & ~1);
        scaledRootInput = gSqrtLutQ3[(unsigned int)scaledRootInput >> shift];
        scaledHorizontal = ((unsigned int)scaledRootInput << 16) >> (19 - (shift >> 1));
    }
    normalRotation = &frame->rotation;
    frame->anglesOrOffset.vx = ANGLE_HALF - ratan2(frame->normalOrTarget.y, scaledHorizontal);
    SetIdentityRotMatrix(normalRotation);
    RotMatrixY(frame->anglesOrOffset.vy, normalRotation);
    RotMatrixX(frame->anglesOrOffset.vx, normalRotation);
    gte_SetRotMatrix(normalRotation);
    edgeA = &frame->edgeA;
    gte_ldlv0(edgeA);
    gte_rtv0();
    gte_stlvnl(edgeA);
    edgeB = &frame->edgeB;
    gte_ldlv0(edgeB);
    gte_rtv0();
    gte_stlvnl(edgeB);
    return 1;
}

int TestSphereVsAlignedSegment(int reusePoint, CollisionWork *work, WorldVector *center, int radius, int radiusSquared)
{
    int localX, localY, localZ, distanceSquared, span;
    int low;
    int high;
    int endpoint;
    int measuredLeading;

    if (!reusePoint) {
        work->anglesOrOffset.vx = center->x - work->primaryVerts[0]->x;
        work->anglesOrOffset.vy = center->y - work->primaryVerts[0]->y;
        work->anglesOrOffset.vz = center->z - work->primaryVerts[0]->z;
        gte_ldv0(&work->anglesOrOffset);
        gte_rtv0();
        gte_stlvnl(&work->localPoint);
    }
    localX = work->localPoint.vx;
    if (localX < -radius || localX > radius)
        return 0;
    localY = work->localPoint.vy;
    if (localY < -radius || localY > radius)
        return 0;
    distanceSquared = localX * localX + localY * localY;
    if (distanceSquared > radiusSquared)
        return 0;

    distanceSquared = radiusSquared - distanceSquared;
    if ((unsigned int)distanceSquared < SQRT_LUT_SIZE) {
        span = gSqrtLutQ3[distanceSquared] >> 3;
    } else {
        int leading = 0;
        unsigned int shift;
        if (distanceSquared >= 0) {
            gte_ldlzc(distanceSquared);
            gte_nop();
            gte_nop();
            gte_stlzc(&measuredLeading);
            leading = measuredLeading;
        }
        shift = 22 - (leading & ~1);
        distanceSquared = gSqrtLutQ3[(unsigned int)distanceSquared >> shift];
        span = ((unsigned int)distanceSquared << 16) >> (19 - (shift >> 1));
    }
    high = 0;
    low = 0;
    endpoint = work->edgeA.z;
    if (endpoint < low)
        low = endpoint;
    else if (endpoint > high)
        high = endpoint;
    endpoint = work->edgeB.z;
    if (endpoint < low)
        low = endpoint;
    else if (endpoint > high)
        high = endpoint;
    localZ = work->localPoint.vz;
    if (high < localZ - span || low > localZ + span)
        return 0;
    return 1;
}

int SegmentIntersectsCircleXY(int radius, int radiusSquared, WorldVector *start, WorldVector *end)
{
    int startX, startY, endX, endY;
    int deltaX, deltaY, edgeX2, edgeY2;
    int edgeSquared, twiceDot, constantScaled, dotScaled;
    int discriminant, upper, lower;
    int measuredLeading;

    if (start->x < end->x) {
        if (end->x < -radius || radius < start->x)
            return 0;
    } else {
        if (start->x < -radius || radius < end->x)
            return 0;
    }
    if (start->y < end->y) {
        if (end->y < -radius || radius < start->y)
            return 0;
    } else {
        if (start->y < -radius || radius < end->y)
            return 0;
    }

    endX = end->x;
    startX = start->x;
    deltaX = endX - startX;
    endY = end->y;
    startY = start->y;
    deltaY = endY - startY;
    edgeX2 = deltaX * deltaX;
    edgeY2 = deltaY * deltaY;
    twiceDot = (deltaX * startX + deltaY * startY) << 1;
    dotScaled = twiceDot >> 9;
    constantScaled = (startX * startX + startY * startY - radiusSquared) >> 9;
    edgeSquared = edgeX2 + edgeY2;
    discriminant = dotScaled * dotScaled - (edgeSquared >> 9) * (constantScaled << 2);
    if (discriminant < 0)
        return 0;
    {
        int root = discriminant;
        if ((unsigned int)discriminant < SQRT_LUT_SIZE) {
            discriminant = gSqrtLutQ3[discriminant] >> 3;
        } else {
            int leading;
            unsigned int shift;
            if (root < 0) {
                leading = 0;
            } else {
                gte_ldlzc(root);
                gte_nop();
                gte_nop();
                gte_stlzc(&measuredLeading);
                leading = measuredLeading;
            }
            shift = 22 - (leading & ~1);
            root = gSqrtLutQ3[(unsigned int)root >> shift];
            discriminant = ((unsigned int)root << 16) >> (19 - (shift >> 1));
        }
    }
    discriminant <<= 9;
    upper = ((-twiceDot + discriminant) << 8) / (edgeSquared << 1);
    lower = ((-twiceDot - discriminant) << 8) / (edgeSquared << 1);
    return upper >= 0 && lower <= SEGMENT_PARAM_ONE_Q8;
}

int TestSphereVsAlignedTriangle(int reusePoint, CollisionWork *work, WorldVector *center, int radius, int radiusSquared)
{
    int centroidProduct, pointSide;
    int localZ, edgeAX, edgeAY, edgeBX, edgeBY, centerX, centerY, localX, localY, deltaX, deltaY;
    int sectionSquared, sectionRadius;
    int measuredLeading;
    WorldVector *edgeA, *edgeB, *base;

    if (!reusePoint) {
        work->anglesOrOffset.vx = center->x - work->primaryVerts[0]->x;
        work->anglesOrOffset.vy = center->y - work->primaryVerts[0]->y;
        work->anglesOrOffset.vz = center->z - work->primaryVerts[0]->z;
        gte_ldv0(&work->anglesOrOffset);
        gte_rtv0();
        gte_stlvnl(&work->localPoint);
    }
    localZ = work->localPoint.vz;
    if (localZ < -radius || localZ > radius)
        return 0;

    edgeAX = work->edgeA.x;
    edgeBX = work->edgeB.x;
    edgeAY = work->edgeA.y;
    edgeBY = work->edgeB.y;
    centerX = (edgeAX + edgeBX) / 3;
    centerY = (edgeAY + edgeBY) / 3;
    localX = work->localPoint.vx;
    localY = work->localPoint.vy;
    centroidProduct = edgeAY * centerX;
    pointSide = edgeAY * localX - edgeAX * localY;
    if (((centroidProduct - edgeAX * centerY) ^ pointSide) >= 0 &&
        ((edgeBY * centerX - edgeBX * centerY) ^ (edgeBY * localX - edgeBX * localY)) >= 0) {
        deltaY = edgeBY - edgeAY;
        deltaX = edgeBX - edgeAX;
        if (((deltaY * (centerX - edgeAX) - deltaX * (centerY - edgeAY)) ^
             (deltaY * (localX - edgeAX) - deltaX * (localY - edgeAY))) >= 0)
            return 1;
    }

    sectionSquared = radiusSquared - work->localPoint.vz * work->localPoint.vz;
    {
        int root = sectionSquared;
        if ((unsigned int)sectionSquared < SQRT_LUT_SIZE) {
            sectionRadius = gSqrtLutQ3[sectionSquared] >> 3;
        } else {
            int leading;
            unsigned int shift;
            if (root < 0) {
                leading = 0;
            } else {
                gte_ldlzc(root);
                gte_nop();
                gte_nop();
                gte_stlzc(&measuredLeading);
                leading = measuredLeading;
            }
            shift = 22 - (leading & ~1);
            root = gSqrtLutQ3[(unsigned int)root >> shift];
            sectionRadius = ((unsigned int)root << 16) >> (19 - (shift >> 1));
        }
    }
    edgeA = &work->edgeA;
    edgeB = &work->edgeB;
    edgeA->x -= work->localPoint.vx;
    edgeA->y -= work->localPoint.vy;
    edgeB->x -= work->localPoint.vx;
    edgeB->y -= work->localPoint.vy;
    if (SegmentIntersectsCircleXY(sectionRadius, sectionSquared, edgeA, edgeB))
        return 1;
    base = &work->base;
    base->x = -work->localPoint.vx;
    base->y = -work->localPoint.vy;
    if (SegmentIntersectsCircleXY(sectionRadius, sectionSquared, base, edgeA))
        return 1;
    if (SegmentIntersectsCircleXY(sectionRadius, sectionSquared, base, edgeB))
        return 1;
    return 0;
}

int BuildNonDegenerateTriangleFrame(CollisionWork *work)
{
    int yaw;
    int horizontalSquared;
    int horizontal;
    int measuredLeading;
    MATRIX *rotation;

    work->edgeA.x = work->primaryVerts[1]->x - work->primaryVerts[0]->x;
    work->edgeA.y = work->primaryVerts[1]->y - work->primaryVerts[0]->y;
    work->edgeA.z = work->primaryVerts[1]->z - work->primaryVerts[0]->z;
    work->edgeB.x = work->primaryVerts[2]->x - work->primaryVerts[0]->x;
    work->edgeB.y = work->primaryVerts[2]->y - work->primaryVerts[0]->y;
    work->edgeB.z = work->primaryVerts[2]->z - work->primaryVerts[0]->z;
    gte_ldopv1(&work->edgeA);
    gte_ldopv2(&work->edgeB);
    gte_op0();
    gte_stlvnl(&work->normalOrTarget);
    if (work->normalOrTarget.x >= -NORMAL_GTE_SAFE_MAX && work->normalOrTarget.x <= NORMAL_GTE_SAFE_MAX &&
        work->normalOrTarget.y >= -NORMAL_GTE_SAFE_MAX && work->normalOrTarget.y <= NORMAL_GTE_SAFE_MAX &&
        work->normalOrTarget.z >= -NORMAL_GTE_SAFE_MAX && work->normalOrTarget.z <= NORMAL_GTE_SAFE_MAX) {
        return 0;
    }
    yaw = ratan2(work->normalOrTarget.x, work->normalOrTarget.z);
    work->normalOrTarget.x >>= 6;
    work->normalOrTarget.y >>= 6;
    work->normalOrTarget.z >>= 6;
    horizontalSquared = work->normalOrTarget.x * work->normalOrTarget.x +
                        work->normalOrTarget.z * work->normalOrTarget.z;
    work->anglesOrOffset.vy = ANGLE_HALF - yaw;
    if ((unsigned int)horizontalSquared < SQRT_LUT_SIZE) {
        horizontal = gSqrtLutQ3[horizontalSquared] >> 3;
    } else {
        int leading = 0;
        unsigned int shift;
        if (horizontalSquared >= 0) {
            gte_ldlzc(horizontalSquared);
            gte_nop();
            gte_nop();
            gte_stlzc(&measuredLeading);
            leading = measuredLeading;
        }
        shift = 22 - (leading & ~1);
        horizontalSquared = gSqrtLutQ3[(unsigned int)horizontalSquared >> shift];
        horizontal = ((unsigned int)horizontalSquared << 16) >> (19 - (shift >> 1));
    }
    rotation = &work->rotation;
    work->anglesOrOffset.vx = ANGLE_HALF - ratan2(work->normalOrTarget.y, horizontal);
    SetIdentityRotMatrix(rotation);
    RotMatrixY(work->anglesOrOffset.vy, rotation);
    RotMatrixX(work->anglesOrOffset.vx, rotation);
    gte_SetRotMatrix(rotation);
    gte_ldlv0(&work->edgeA);
    gte_rtv0();
    gte_stlvnl(&work->edgeA);
    gte_ldlv0(&work->edgeB);
    gte_rtv0();
    gte_stlvnl(&work->edgeB);
    return 1;
}

/* Counted up by TestTriangleIntersectsAlignedTriangle at each zero-span slice; nothing else reads it. */
int gTriSliceZeroSpanCount = 0;
/* 4 bytes; nothing reads or writes them. */
int gGeometryUnused = 0;

#define SLICE_CROSS(dx, dy, x, y) ((dy) * (x) - (dx) * (y))

int TestTriangleIntersectsAlignedTriangle(CollisionWork *work, WorldVector *vertexA,
                                          WorldVector *vertexB, WorldVector *vertexC)
{
    SVECTOR *offset = &work->anglesOrOffset;
    int hiIndex, loIndex, i;
    int maxZ, minZ, vertexZ;
    int midIndex;
    int span, firstX, firstY, secondX, secondY;
    int centerX, centerY;
    int firstSide, secondSide;

    /* Prepare the next input while the GTE is transforming the previous vertex. */
    offset->vx = vertexA->x - work->primaryVerts[0]->x;
    offset->vy = vertexA->y - work->primaryVerts[0]->y;
    offset->vz = vertexA->z - work->primaryVerts[0]->z;
    gte_ldv0(offset);
    gte_rtv0();
    offset->vx = vertexB->x - work->primaryVerts[0]->x;
    offset->vy = vertexB->y - work->primaryVerts[0]->y;
    offset->vz = vertexB->z - work->primaryVerts[0]->z;
    gte_stlvnl(&work->projected[0]);
    gte_ldv0(offset);
    gte_rtv0();
    offset->vx = vertexC->x - work->primaryVerts[0]->x;
    offset->vy = vertexC->y - work->primaryVerts[0]->y;
    offset->vz = vertexC->z - work->primaryVerts[0]->z;
    gte_stlvnl(&work->projected[1]);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&work->projected[2]);

    hiIndex = 0;
    loIndex = 0;
    maxZ = work->projected[0].vz;
    minZ = maxZ;
    for (i = 1; i < 3; ++i) {
        vertexZ = work->projected[i].vz;
        if (vertexZ < minZ) {
            loIndex = i;
            minZ = vertexZ;
        } else if (maxZ < vertexZ) {
            hiIndex = i;
            maxZ = vertexZ;
        }
    }
    if (minZ > 0 || maxZ < 0)
        return 0;
    if (minZ == 0 && maxZ == 0) {
        loIndex = 0;
        hiIndex = 1;
        work->projected[0].vz--;
        work->projected[1].vz++;
    }

    /* Interpolate the endpoints of the opposing triangle's slice at Z = 0. */
    span = work->projected[hiIndex].vz - work->projected[loIndex].vz;
    if (span == 0) {
        span = 1;
        gTriSliceZeroSpanCount++;
    }
    firstX = work->projected[loIndex].vx +
        (-work->projected[loIndex].vz * (work->projected[hiIndex].vx - work->projected[loIndex].vx)) / span;
    firstY = work->projected[loIndex].vy +
        (-work->projected[loIndex].vz * (work->projected[hiIndex].vy - work->projected[loIndex].vy)) / span;
    midIndex = 3 - loIndex - hiIndex;
    if (work->projected[midIndex].vz >= 0) {
        span = work->projected[midIndex].vz - work->projected[loIndex].vz;
        if (span == 0) {
            span = 1;
            gTriSliceZeroSpanCount++;
        }
        secondX = work->projected[loIndex].vx +
            (-work->projected[loIndex].vz * (work->projected[midIndex].vx - work->projected[loIndex].vx)) / span;
        secondY = work->projected[loIndex].vy +
            (-work->projected[loIndex].vz * (work->projected[midIndex].vy - work->projected[loIndex].vy)) / span;
    } else {
        span = work->projected[hiIndex].vz - work->projected[midIndex].vz;
        if (span == 0) {
            span = 1;
            gTriSliceZeroSpanCount++;
        }
        secondX = work->projected[midIndex].vx +
            (-work->projected[midIndex].vz * (work->projected[hiIndex].vx - work->projected[midIndex].vx)) / span;
        secondY = work->projected[midIndex].vy +
            (-work->projected[midIndex].vz * (work->projected[hiIndex].vy - work->projected[midIndex].vy)) / span;
    }

    /* The reference triangle has vertices at the origin and the two rotated edges. */
    centerX = (work->edgeA.x + work->edgeB.x) / 3;
    centerY = (work->edgeA.y + work->edgeB.y) / 3;
    firstSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, centerX, centerY);
    secondSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, firstX, firstY);
    if ((firstSide ^ secondSide) >= 0) {
        firstSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, centerX, centerY);
        secondSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, firstX, firstY);
        if ((firstSide ^ secondSide) >= 0) {
            firstSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                                    centerX - work->edgeA.x, centerY - work->edgeA.y);
            secondSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                                     firstX - work->edgeA.x, firstY - work->edgeA.y);
            if ((firstSide ^ secondSide) >= 0)
                return 1;
        }
    }

    firstSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, centerX, centerY);
    secondSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, secondX, secondY);
    if ((firstSide ^ secondSide) >= 0) {
        firstSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, centerX, centerY);
        secondSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, secondX, secondY);
        if ((firstSide ^ secondSide) >= 0) {
            firstSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                                    centerX - work->edgeA.x, centerY - work->edgeA.y);
            secondSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                                     secondX - work->edgeA.x, secondY - work->edgeA.y);
            if ((firstSide ^ secondSide) >= 0)
                return 1;
        }
    }

    /* If neither endpoint is inside, test the slice segment against each edge. */
    firstSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, firstX, firstY);
    secondSide = SLICE_CROSS(work->edgeA.x, work->edgeA.y, secondX, secondY);
    if ((firstSide ^ secondSide) >= 0) {
        firstSide = SLICE_CROSS(secondX - firstX, secondY - firstY, work->edgeA.x - firstX, work->edgeA.y - firstY);
        secondSide = SLICE_CROSS(secondX - firstX, secondY - firstY, -firstX, -firstY);
        if ((firstSide ^ secondSide) >= 0)
            return 1;
    }

    firstSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, firstX, firstY);
    secondSide = SLICE_CROSS(work->edgeB.x, work->edgeB.y, secondX, secondY);
    if ((firstSide ^ secondSide) >= 0) {
        firstSide = SLICE_CROSS(secondX - firstX, secondY - firstY, work->edgeB.x - firstX, work->edgeB.y - firstY);
        secondSide = SLICE_CROSS(secondX - firstX, secondY - firstY, -firstX, -firstY);
        if ((firstSide ^ secondSide) >= 0)
            return 1;
    }

    firstSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                            firstX - work->edgeA.x, firstY - work->edgeA.y);
    secondSide = SLICE_CROSS(work->edgeB.x - work->edgeA.x, work->edgeB.y - work->edgeA.y,
                             secondX - work->edgeA.x, secondY - work->edgeA.y);
    if ((firstSide ^ secondSide) >= 0) {
        firstSide = SLICE_CROSS(secondX - firstX, secondY - firstY, work->edgeA.x - firstX, work->edgeA.y - firstY);
        secondSide = SLICE_CROSS(secondX - firstX, secondY - firstY, work->edgeB.x - firstX, work->edgeB.y - firstY);
        if ((firstSide ^ secondSide) >= 0)
            return 1;
    }
    return 0;
}

int IsPointInTriangleXZ(WorldVector *vertexA, WorldVector *vertexB, WorldVector *vertexC, WorldVector *probe)
{
    int ax;
    int bx;
    int cx;
    int az;
    int bz;
    int cz;
    int centerX;
    int abZ;
    int centerFromAX;
    int px;
    int probeFromAX;
    int pz;
    int probeFromAZ;
    int abX;
    int centerZ;
    int centerFromAZ;
    int acZ;
    int acX;
    int bcZ;
    int bcX;
    int centerSide;
    int probeSide;

    ax = vertexA->x;
    bx = vertexB->x;
    cx = vertexC->x;
    centerX = (ax + bx + cx) / 3;
    az = vertexA->z;
    bz = vertexB->z;
    cz = vertexC->z;
    centerZ = (az + bz + cz) / 3;
    abZ = bz - az;
    centerFromAX = centerX - ax;
    abX = bx - ax;
    px = probe->x;
    probeFromAX = px - ax;
    pz = probe->z;
    probeFromAZ = pz - az;
    /* Use the integer centroid as the reference side for all three XZ edges. */
    centerFromAZ = centerZ - az;
    centerSide = abZ * centerFromAX - abX * centerFromAZ;
    probeSide = abZ * probeFromAX - abX * probeFromAZ;
    if ((centerSide ^ probeSide) >= 0) {
        acZ = cz - az;
        acX = cx - ax;
        centerSide = acZ * centerFromAX - acX * centerFromAZ;
        probeSide = acZ * probeFromAX - acX * probeFromAZ;
        if ((centerSide ^ probeSide) >= 0) {
            bcZ = cz - bz;
            bcX = cx - bx;
            centerSide = bcZ * (centerX - bx) - bcX * (centerZ - bz);
            probeSide = bcZ * (px - bx) - bcX * (pz - bz);
            if ((centerSide ^ probeSide) >= 0) {
                return 1;
            }
        }
    }
    return 0;
}

void BuildSegmentFrame(CollisionWork *work)
{
    int yaw;
    int horizontalSquared;
    int horizontal;
    int measuredLeading;
    MATRIX *rotation;

    work->edgeA.x = work->primaryVerts[1]->x - work->primaryVerts[0]->x;
    work->edgeA.y = work->primaryVerts[1]->y - work->primaryVerts[0]->y;
    work->edgeA.z = work->primaryVerts[1]->z - work->primaryVerts[0]->z;
    yaw = ratan2(work->edgeA.x, work->edgeA.z);
    horizontalSquared = work->edgeA.x * work->edgeA.x + work->edgeA.z * work->edgeA.z;
    work->anglesOrOffset.vy = ANGLE_HALF - yaw;
    if ((unsigned int)horizontalSquared < SQRT_LUT_SIZE) {
        horizontal = gSqrtLutQ3[horizontalSquared] >> 3;
    } else {
        int leading = 0;
        unsigned int shift;
        if (horizontalSquared >= 0) {
            gte_ldlzc(horizontalSquared);
            gte_nop();
            gte_nop();
            gte_stlzc(&measuredLeading);
            leading = measuredLeading;
        }
        shift = 22 - (leading & ~1);
        horizontalSquared = gSqrtLutQ3[(unsigned int)horizontalSquared >> shift];
        horizontal = ((unsigned int)horizontalSquared << 16) >> (19 - (shift >> 1));
    }
    rotation = &work->rotation;
    work->anglesOrOffset.vx = ANGLE_HALF - ratan2(work->edgeA.y, horizontal);
    SetIdentityRotMatrix(rotation);
    RotMatrixY(work->anglesOrOffset.vy, rotation);
    RotMatrixX(work->anglesOrOffset.vx, rotation);
    gte_SetRotMatrix(rotation);
    gte_ldlv0(&work->edgeA);
    gte_rtv0();
    gte_stlvnl(&work->edgeA);
}

int TestSphereVsSegmentFrame(CollisionWork *work, WorldVector *center, int radius, int radiusSquared)
{
    SVECTOR *offset = &work->anglesOrOffset;
    int localX;
    int localY;
    int transverseSquared;
    int depthCap;
    int measuredLeading;
    int ahead;
    int behind;

    offset->vx = center->x - work->primaryVerts[0]->x;
    offset->vy = center->y - work->primaryVerts[0]->y;
    offset->vz = center->z - work->primaryVerts[0]->z;
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&work->localPoint);
    localX = work->localPoint.vx;
    if (localX < -radius || localX > radius)
        return 0;
    localY = work->localPoint.vy;
    if (localY < -radius || localY > radius)
        return 0;
    transverseSquared = localX * localX + localY * localY;
    if (transverseSquared > radiusSquared)
        return 0;
    depthCap = radiusSquared - transverseSquared;
    if ((unsigned int)depthCap < SQRT_LUT_SIZE) {
        depthCap = gSqrtLutQ3[depthCap] >> 3;
    } else {
        int leading = 0;
        unsigned int shift;
        if (depthCap >= 0) {
            gte_ldlzc(depthCap);
            gte_nop();
            gte_nop();
            gte_stlzc(&measuredLeading);
            leading = measuredLeading;
        }
        shift = 22 - (leading & ~1);
        depthCap = gSqrtLutQ3[(unsigned int)depthCap >> shift];
        depthCap = ((unsigned int)depthCap << 16) >> (19 - (shift >> 1));
    }
    ahead = 0;
    behind = 0;
    if (work->edgeA.z < 0)
        behind = work->edgeA.z;
    else
        ahead = work->edgeA.z;
    if (work->localPoint.vz - depthCap > ahead || work->localPoint.vz + depthCap < behind)
        return 0;
    return 1;
}

int GetTriangleNormalHeading(VECTOR *origin, VECTOR *vertexA, VECTOR *vertexB)
{
    CollisionWork *work;
    WorldVector *normal;
    int angle;

    work = &COLLISION_SCRATCH->work;
    work->edgeA.x = vertexA->vx - origin->vx;
    work->edgeA.y = vertexA->vy - origin->vy;
    work->edgeA.z = vertexA->vz - origin->vz;
    work->edgeB.x = vertexB->vx - origin->vx;
    work->edgeB.y = vertexB->vy - origin->vy;
    work->edgeB.z = vertexB->vz - origin->vz;
    gte_ldopv1(&work->edgeA);
    gte_ldopv2(&work->edgeB);
    gte_op0();
    normal = &work->normalOrTarget;
    gte_stlvnl(normal);
    angle = ratan2(normal->x, normal->z);
    if (normal->y > 0) {
        angle += ANGLE_HALF;
    }
    return angle;
}

int IsFighterSupine(Fighter *fighter)
{
    CollisionScratch *scratch;
    CollisionWork *work;
    WorldVector *torsoSpheres;

    /* Torso hit-spheres [1..3] (bone 1) form the reference chest triangle; +Y normal means face-up. */
    scratch = COLLISION_SCRATCH;
    torsoSpheres = scratch->bodyPartPos[fighter->side];
    work = &scratch->work;
    work->edgeA.x = torsoSpheres[2].x - torsoSpheres[1].x;
    work->edgeA.y = torsoSpheres[2].y - torsoSpheres[1].y;
    work->edgeA.z = torsoSpheres[2].z - torsoSpheres[1].z;
    work->edgeB.x = torsoSpheres[3].x - torsoSpheres[1].x;
    work->edgeB.y = torsoSpheres[3].y - torsoSpheres[1].y;
    work->edgeB.z = torsoSpheres[3].z - torsoSpheres[1].z;
    gte_ldopv1(&work->edgeA);
    gte_ldopv2(&work->edgeB);
    gte_op0();
    gte_stlvnl(&work->normalOrTarget);
    return work->normalOrTarget.y > 0;
}

/* Never true: compared unsigned, the two bounds exclude each other. */
int IsWeaponSwingPlaneVertical(Fighter *fighter)
{
    CollisionScratch *scratch;
    WorldVector *weaponPoints;
    CollisionWork *work;
    int squared;
    int measuredLeading;
    unsigned int normalYQ12;

    scratch = COLLISION_SCRATCH;
    work = &scratch->work;
    weaponPoints = scratch->weaponPoints[fighter->side];
    work->edgeA.x = weaponPoints[1].x - weaponPoints[0].x;
    work->edgeA.y = weaponPoints[1].y - weaponPoints[0].y;
    work->edgeA.z = weaponPoints[1].z - weaponPoints[0].z;
    work->edgeB.x = fighter->prevWeaponPoints[1].x - weaponPoints[0].x;
    work->edgeB.y = fighter->prevWeaponPoints[1].y - weaponPoints[0].y;
    work->edgeB.z = fighter->prevWeaponPoints[1].z - weaponPoints[0].z;
    gte_ldopv1(&work->edgeA);
    gte_ldopv2(&work->edgeB);
    gte_op0();

    gte_stlvnl(&work->normalOrTarget);
    if (work->normalOrTarget.y < 0) {
        work->normalOrTarget.y = -work->normalOrTarget.y;
        work->normalOrTarget.x = -work->normalOrTarget.x;
        work->normalOrTarget.z = -work->normalOrTarget.z;
    }
    while (work->normalOrTarget.y > NORMAL_SCALE_LIMIT ||
           work->normalOrTarget.x > NORMAL_SCALE_LIMIT ||
           work->normalOrTarget.z > NORMAL_SCALE_LIMIT) {
        work->normalOrTarget.x >>= 1;
        work->normalOrTarget.y >>= 1;
        work->normalOrTarget.z >>= 1;
    }
    squared = work->normalOrTarget.x * work->normalOrTarget.x +
              work->normalOrTarget.y * work->normalOrTarget.y +
              work->normalOrTarget.z * work->normalOrTarget.z;
    {
        int root = squared;
        if ((unsigned int)squared < SQRT_LUT_SIZE) {
            squared = gSqrtLutQ3[squared] >> 3;
        } else {
            int leading;
            unsigned int shift;
            if (root < 0) {
                leading = 0;
            } else {
                gte_ldlzc(root);
                gte_nop();
                gte_nop();
                gte_stlzc(&measuredLeading);
                leading = measuredLeading;
            }
            shift = 22 - (leading & ~1);
            root = gSqrtLutQ3[(unsigned int)root >> shift];
            squared = ((unsigned int)root << 16) >> (19 - (shift >> 1));
        }
    }
    if (squared == 0)
        squared = 1;
    normalYQ12 = (work->normalOrTarget.y << 12) / (unsigned int)squared;
    return normalYQ12 < VERTICAL_SWING_NORMAL_Y_MAX && normalYQ12 > -VERTICAL_SWING_NORMAL_Y_MAX;
}
