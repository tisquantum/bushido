/* Fighter hit geometry: the 22 body hit spheres scaled to a fighter's stature and carried to world
 * space with the weapon edge points and feet each frame, and the stage transition both fighters stand in. */

#include <sys/types.h>
#include <libgte.h>
#include <inline_o.h>
#include "fighter_skeleton.h"

#include "fighter.h"
#include "fighter_hit_sphere.h"
#include "world_vector.h"
#include "fighter_pose.h"
#include "stage_tables.h"
#include "fighter_hit.h"
#include "main.h"
#include "globals.h"

void ScaleFighterHitSpheres(int side, int statureScale)
{
    FighterHitSphere *baseSphere;
    FighterHitSphere *scaledSphere;
    int i;

    baseSphere = gBaseHitSpheres;
    scaledSphere = gFighterHitSpheres[side];
    for (i = 0; i < FIGHTER_HIT_SPHERE_COUNT; i++, baseSphere++, scaledSphere++) {
        scaledSphere->isVital = baseSphere->isVital;
        scaledSphere->boneIndex = baseSphere->boneIndex;
        scaledSphere->localOffset[0] = (baseSphere->localOffset[0] * statureScale) >> 12;
        scaledSphere->localOffset[1] = (baseSphere->localOffset[1] * statureScale) >> 12;
        scaledSphere->localOffset[2] = (baseSphere->localOffset[2] * statureScale) >> 12;
        scaledSphere->outerRadius = (baseSphere->outerRadius * statureScale) >> 12;
        scaledSphere->outerRadiusSq =
            (baseSphere->outerRadiusSq * statureScale) >> 12; /* scaled by the stature, not its square */
        scaledSphere->vitalRadius = (baseSphere->vitalRadius * statureScale) >> 12;
        scaledSphere->vitalRadiusSq =
            (baseSphere->vitalRadiusSq * statureScale) >> 12; /* scaled by the stature, not its square */
    }
}

void UpdateFighterHitGeometry(Fighter *fighter, WorldVector *sphereCenters, WorldVector *weaponPoints)
{
    WorldVector *outPoint;
    MATRIX **boneMatrices;
    FighterHitSphere *sphere;
    MATRIX *bone;
    MATRIX *footBone;
    SVECTOR *edgeOffset;
    int i;

    boneMatrices = GetFighterWorldMatrixList(fighter->side);
    sphere = gFighterHitSpheres[fighter->side];
    outPoint = sphereCenters;
    for (i = 0; i < FIGHTER_HIT_SPHERE_COUNT; i++, sphere++, outPoint++) {
        bone = boneMatrices[sphere->boneIndex];
        gte_SetRotMatrix(bone);
        gte_ldv0(sphere->localOffset);
        gte_rtv0();
        gte_stlvnl(outPoint);
        outPoint->x += bone->t[0];
        outPoint->y += bone->t[1];
        outPoint->z += bone->t[2];
    }
    bone = boneMatrices[FIGHTER_JOINT_RIGHT_HAND];
    gte_SetRotMatrix(bone);
    if (fighter->characterClass == FIGHTER_CLASS_FINAL_BOSS_ALT) {
        edgeOffset = gWeaponEdgeOffsets[WEAPON_EDGE_OFFSET_FINAL_BOSS_ALT];
    } else {
        edgeOffset = gWeaponEdgeOffsets[fighter->weaponId];
    }
    outPoint = weaponPoints;
    for (i = 0; i < gWeaponEdgePointCounts[fighter->weaponId]; i++, edgeOffset++, outPoint++) {
        gte_ldv0(edgeOffset);
        gte_rtv0();
        gte_stlvnl(outPoint);
        outPoint->x += bone->t[0];
        outPoint->y += bone->t[1];
        outPoint->z += bone->t[2];
    }
    /* Ankle-to-sole vertical offset is 142 world units at Q12 scale 1.0. */
    footBone = boneMatrices[FIGHTER_JOINT_LEFT_FOOT];
    fighter->leftFootPos.x = footBone->t[0];
    fighter->leftFootPos.y = footBone->t[1] + ((fighter->statureScale * 142) >> 12);
    fighter->leftFootPos.z = footBone->t[2];
    footBone = boneMatrices[FIGHTER_JOINT_RIGHT_FOOT];
    fighter->rightFootPos.x = footBone->t[0];
    fighter->rightFootPos.y = footBone->t[1] + ((fighter->statureScale * 142) >> 12);
    fighter->rightFootPos.z = footBone->t[2];
}

StageTransition *FindActiveStageTransition(void)
{
    StageTransition *transition = gStageTransitionsPtr;

    while (transition->stageA >= 0) {
        if (transition->stageA == gCurrentStageId) {
            int fightersInside = 0;
            int side;

            for (side = 0; side < 2; side++) {
                Fighter *fighter = &gFighters[side];

                fightersInside += (fighter->position.vx >= transition->anchors[0].x &&
                                   fighter->position.vx < transition->anchors[0].x + transition->width &&
                                   fighter->position.vz >= transition->anchors[0].z &&
                                   fighter->position.vz < transition->anchors[0].z + transition->depth);
            }
            if (fightersInside == 2) {
                return transition;
            }
        }
        transition++;
    }
    return 0;
}
