#ifndef COLLISION_SCRATCH_H
#define COLLISION_SCRATCH_H

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>

#include "fighter_hit_sphere.h"
#include "world_vector.h"

#define WEAPON_CONTROL_POINT_COUNT 4
/* Spheres the hit tests skip while the left arm is crippled: 8, 9 (left forearm) and 10, 11 (right upper arm);
 * the left arm is spheres 6..9. */
#define CRIPPLED_LEFT_ARM_SKIP_FIRST 8
#define CRIPPLED_LEFT_ARM_SKIP_COUNT 4

/* Blade-lock (tsubazeriai) struggle state in gBladeLockState. */
typedef enum BladeLockState {
    BLADE_LOCK_ENDING = -1,
    BLADE_LOCK_NONE = 0,
    BLADE_LOCK_ACTIVE = 1,
    BLADE_LOCK_BREAK = 2,
    BLADE_LOCK_ADVANTAGE = 3
} BladeLockState;

/*
 * Working collision area at 0x1F8002A0 (CollisionScratch + 0x2A0; 0x15C bytes).
 * Shared across CheckWeaponClashAndEnvHit, ResolveWeaponVsBodyHits, TestTriangleVsBodySpheres,
 * BuildTriangleFrame, BuildNonDegenerateTriangleFrame, TestSphereVsAlignedSegment/Triangle,
 * TestTriangleIntersectsAlignedTriangle, BuildSegmentFrame, TestSphereVsSegmentFrame,
 * GetTriangleNormalHeading, IsFighterSupine, and IsWeaponSwingPlaneVertical.
 */
typedef struct CollisionWork {
    WorldVector workPoints[2][WEAPON_CONTROL_POINT_COUNT];  /* +0x000 (0x2A0): swept weapon quad points */
    WorldVector *primaryVerts[3];                           /* +0x060 (0x300): reference triangle/segment
                                                               vertex pointers */
    WorldVector *secondaryVerts[3];                         /* +0x06C (0x30C): opposing triangle vertex pointers */
    WorldVector primaryMin;                                 /* +0x078 (0x318): reference triangle AABB min */
    WorldVector primaryMax;                                 /* +0x084 (0x324): reference triangle AABB max */
    WorldVector secondaryMin;                               /* +0x090 (0x330): opposing triangle AABB min */
    WorldVector secondaryMax;                               /* +0x09C (0x33C): opposing triangle AABB max */
    WorldVector edgeA;                                      /* +0x0A8 (0x348): rotated primary edge / segment delta
                                                               (VECTOR.vx..vz) */
    unsigned int hits;                                      /* +0x0B4 (0x354): VECTOR.pad /
                                                               22-bit outer hit-sphere mask */
    WorldVector edgeB;                                      /* +0x0B8 (0x358): rotated secondary edge (VECTOR.vx..vz) */
    unsigned int vitalHits;                                 /* +0x0C4 (0x364): VECTOR.pad /
                                                               22-bit inner vital-sphere mask */
    WorldVector normalOrTarget;                             /* +0x0C8 (0x368): triangle normal /
                                                               closest-part target point */
    int normalPad;                                          /* +0x0D4 (0x374): VECTOR.pad */
    MATRIX rotation;                                        /* +0x0D8 (0x378): GTE triangle/segment alignment matrix */
    SVECTOR anglesOrOffset;                                 /* +0x0F8 (0x398): alignment pitch/yaw (vx/vy)
                                                               or relative offset */
    VECTOR localPoint;                                      /* +0x100 (0x3A0): transformed query point /
                                                               normal squares / prop hit point */
    WorldVector base;                                       /* +0x110 (0x3B0): negated local point
                                                               for edge-circle tests */
    int basePad;                                            /* +0x11C (0x3BC): VECTOR.pad */
    VECTOR projected[3];                                    /* +0x120 (0x3C0): opposing triangle vertices
                                                               in reference frame */
    WorldVector clashContactPoint;                          /* +0x150 (0x3F0): averaged weapon-clash contact point */
} CollisionWork;

/* 1 KB PS1 scratchpad layout (0x1F800000..0x1F8003FF) during combat collision. */
typedef struct CollisionScratch {
    WorldVector bodyPartPos[2][FIGHTER_HIT_SPHERE_COUNT];     /* +0x000: current-frame world positions
                                                                 of 22 hit-spheres/fighter */
    WorldVector weaponPoints[2][WEAPON_CONTROL_POINT_COUNT];  /* +0x210: current-frame 4 weapon
                                                                 control points/fighter */
    WorldVector bodyCenters[2][2];                            /* +0x270: body separation centers */
    CollisionWork work;                                       /* +0x2A0: swept triangle / clash /
                                                                 sphere test workspace */
} CollisionScratch;

#define COLLISION_SCRATCH ((CollisionScratch *)getScratchAddr(0))

#endif
