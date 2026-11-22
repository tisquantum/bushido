/*
 * How a hit lands on a fighter: wounds, crippled limbs and kills across the 22 hit spheres, guard
 * parries and blade clashes, Katze's gunshot ray, the blade-lock struggle and body separation.
 */

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <inline_o.h>

#include "fighter.h"
#include "world_vector.h"
#include "fighter_hit_sphere.h"
#include "collision_scratch.h"
#include "weapon_id.h"
#include "game_state.h"
#include "model_arena.h"
#include "rumble_pulse.h"
#include "subweapon.h"
#include "combat_event.h"
#include "combat_hit.h"
#include "floor_collision.h"
#include "angle.h"
#include "cloth.h"
#include "geometry.h"
#include "main.h"
#include "scene_math.h"
#include "globals.h"

#define ANGLE_THREE_EIGHTHS       1536

#define GUNSHOT_MUZZLE_Y_OFFSET   100
#define GUNSHOT_RAY_LENGTH_SHIFT  7
#define GUNSHOT_WALL_MARCH_STEPS  32

#define BLADE_LOCK_TARGET_DIST    1100
#define BLADE_LOCK_BIAS_DEADZONE  16
#define BLADE_LOCK_BIAS_MAX       256
#define BLADE_LOCK_PUSH_THRESHOLD 2048
#define BLADE_LOCK_BREAK_SELF_MAX 512
#define BLADE_LOCK_BREAK_OPP_MAX  2048
#define BLADE_LOCK_ADVANTAGE_DIFF 8192
#define BLADE_LOCK_SPARK_Y_OFFSET 200
#define FIGHTER_SPRINT_THRESHOLD  3000

short gBladeLockMashButtons[] = { PADRup, PADRright, PADRdown };
short gBladeLockWinner;
/* Body separation sphere radii: [0] upper torso (hit-sphere 1), [1] lower body (midpoint of thigh spheres 15 & 19). */
int gFighterBodyRadii[2] = { 350, 420 };

static void MarkFighterInjuredRegion(int slot, int sphere)
{
    switch (sphere) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        gFighterInjuredRegions[slot] |= INJURY_OVERLAY_HEAD_TORSO;
        break;
    case 6:
    case 7:
    case 8:
    case 9:
        gFighterInjuredRegions[slot] |= INJURY_OVERLAY_LEFT_ARM;
        break;
    case 10:
    case 11:
    case 12:
    case 13:
        gFighterInjuredRegions[slot] |= INJURY_OVERLAY_RIGHT_ARM;
        break;
    case 14:
    case 15:
    case 16:
    case 17:
        gFighterInjuredRegions[slot] |= INJURY_OVERLAY_RIGHT_LEG;
        break;
    case 18:
    case 19:
    case 20:
    case 21:
        gFighterInjuredRegions[slot] |= INJURY_OVERLAY_LEFT_LEG;
        break;
    }
}

void ApplyFighterHitImpact(int mode, Fighter *fighter, int sphere, int impactAngle, int isVitalHit, int guardDeflects)
{
    int slot = fighter->side;
    Fighter *opponent = fighter->opponent;
    int event;

    if (mode == HIT_BY_BLADE) {
        gBloodSprayDir.vx = -opponent->weaponTipVel.vx;
        gBloodSprayDir.vy = -opponent->weaponTipVel.vy;
        gBloodSprayDir.vz = -opponent->weaponTipVel.vz;
        VectorNormal(&gBloodSprayDir, &gBloodSprayDir);
    }

    if (fighter->action == FIGHTER_ACTION_DEAD || fighter->action == FIGHTER_ACTION_FATAL_KO) {
        if (mode < HIT_BY_DEBRIS) {
            EmitCombatEvent(slot, COMBAT_EVENT_FATAL_HIT, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
            TriggerFighterRumble(fighter, RUMBLE_FATAL_HIT);
        }
        return;
    }

    if (fighter->placement->flags & MOTION_FLAG_FATAL) {
        if (mode < HIT_BY_DEBRIS) {
            EmitCombatEvent(slot, COMBAT_EVENT_FATAL_HIT, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
            TriggerFighterRumble(fighter, RUMBLE_FATAL_HIT);
        }
        return;
    }

    if (fighter->action == FIGHTER_ACTION_GUARD && mode < HIT_BY_DEBRIS && opponent->weaponId != WEAPON_ID_GUN &&
        impactAngle > ANGLE_QUARTER) {
        if (mode == HIT_BY_BLADE) {
            if (fighter->clashPower < opponent->clashPower) {
                fighter->pendingAction =
                    (fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK) + FORCED_ACTION_GUARD_BREAK_BASE;
                TriggerFighterRumble(fighter, RUMBLE_GUARD_BREAK);
                EmitCombatEvent(slot, COMBAT_EVENT_GUARD_BREAK,
                                (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
            } else {
                fighter->pendingAction = opponent->animInfo->flags & ANIM_FLAG_STANCE_MASK;
                TriggerFighterRumble(fighter, RUMBLE_GUARD_PARRY);
                EmitCombatEvent(slot, COMBAT_EVENT_GUARD_PARRY,
                                (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
            }
        } else if (guardDeflects & 1) {
            fighter->pendingAction = FORCED_ACTION_GUARD_PARRY_MID;
            TriggerFighterRumble(fighter, RUMBLE_GUARD_PARRY);
            EmitCombatEvent(slot, COMBAT_EVENT_GUARD_PARRY, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
        } else {
            fighter->pendingAction =
                (fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK) + FORCED_ACTION_GUARD_BREAK_BASE;
            TriggerFighterRumble(fighter, RUMBLE_GUARD_BREAK);
            EmitCombatEvent(slot, COMBAT_EVENT_GUARD_BREAK, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
        }
        return;
    }
    if (mode == HIT_BY_DEBRIS) {
        if (fighter->action == FIGHTER_ACTION_DOWNED ||
            (fighter->action == FIGHTER_ACTION_ATTACK && (fighter->animInfo->flags & ANIM_FLAG_DOWNED)) ||
            impactAngle <= ANGLE_QUARTER)
            return;
        if (sphere < FIGHTER_VITAL_SPHERE_COUNT)
            fighter->pendingAction = FORCED_ACTION_STAGGER_FRONT;
        TriggerFighterRumble(fighter, RUMBLE_DEBRIS_HIT);
        fighter->faceExpression = FIGHTER_FACE_WOUND;
        return;
    }

    if (fighter->side == 0)
        gStoryNoDamageFlag = 0;
    if (fighter->action == FIGHTER_ACTION_DEFEATED) {
        fighter->pendingAction = impactAngle > ANGLE_QUARTER ? FORCED_ACTION_FATAL_FRONT : FORCED_ACTION_FATAL_BACK;
        EmitCombatEvent(slot, COMBAT_EVENT_FATAL_HIT, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
        TriggerFighterRumble(fighter, RUMBLE_FATAL_HIT);
        fighter->faceExpression = FIGHTER_FACE_LETHAL;
        gFighterInjuredRegions[0] |= INJURY_OVERLAY_ALL; /* player one's, whichever fighter was struck */
        return;
    }

    event = COMBAT_EVENT_LIMB_WOUND;
    if (fighter->action == FIGHTER_ACTION_DOWNED ||
        (fighter->action == FIGHTER_ACTION_ATTACK && (fighter->animInfo->flags & ANIM_FLAG_DOWNED))) {
        if (sphere < FIGHTER_VITAL_SPHERE_COUNT) {
            if (fighter->isSupine != 0) {
                fighter->pendingAction = FORCED_ACTION_FATAL_FRONT;
                if (fighter->facingState == MOTION_FACING_INVERTED)
                    fighter->facingState = MOTION_FACING_SUPINE_FLIP;
            } else {
                fighter->pendingAction = FORCED_ACTION_FATAL_BACK;
                if (fighter->facingState == MOTION_FACING_INVERTED)
                    fighter->facingState = MOTION_FACING_ON_ENTRY;
                else if (!(fighter->animInfo->flags & ANIM_FLAG_DOWNED))
                    fighter->facingState = MOTION_FACING_INVERTED;
            }
            event = COMBAT_EVENT_FATAL_HIT;
            TriggerFighterRumble(fighter, RUMBLE_FATAL_HIT);
            fighter->faceExpression = FIGHTER_FACE_LETHAL;
        } else {
            if (fighter->isSupine != 0) {
                fighter->pendingAction = FORCED_ACTION_STAGGER_FRONT;
                if (fighter->facingState == MOTION_FACING_INVERTED)
                    fighter->facingState = MOTION_FACING_SUPINE_FLIP;
            } else {
                fighter->pendingAction = FORCED_ACTION_STAGGER_BACK;
                if (fighter->facingState == MOTION_FACING_INVERTED)
                    fighter->facingState = MOTION_FACING_ON_ENTRY;
                else if (!(fighter->animInfo->flags & ANIM_FLAG_DOWNED))
                    fighter->facingState = MOTION_FACING_INVERTED;
            }
            switch (sphere) {
            case 6: case 7: case 8: case 9:
                fighter->leftArmIntact = 0;
                break;
            case 10: case 11: case 12: case 13:
                fighter->rightArmWoundCount++;
                break;
            case 14: case 15: case 18: case 19:
                fighter->thighWoundCount++;
                break;
            case 16: case 17: case 20: case 21:
                fighter->legsIntact = 0;
                fighter->pendingAction = fighter->pendingAction == FORCED_ACTION_STAGGER_FRONT
                                             ? FORCED_ACTION_LEG_CRIPPLE
                                             : FORCED_ACTION_PRONE_LEG_CRIPPLE;
                break;
            }
            TriggerFighterRumble(fighter, RUMBLE_WOUND);
            fighter->faceExpression = FIGHTER_FACE_WOUND;
        }
        EmitCombatEvent(slot, event, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
        MarkFighterInjuredRegion(slot, sphere);
        return;
    }

    switch (sphere) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        if (isVitalHit != 0) {
            event = COMBAT_EVENT_FATAL_HIT;
            fighter->pendingAction =
                impactAngle > ANGLE_QUARTER ? FORCED_ACTION_FATAL_FRONT : FORCED_ACTION_FATAL_BACK;
            TriggerFighterRumble(fighter, RUMBLE_FATAL_HIT);
            fighter->faceExpression = FIGHTER_FACE_LETHAL;
        } else {
            fighter->torsoWoundCount++;
            event = COMBAT_EVENT_TORSO_WOUND;
            fighter->pendingAction =
                impactAngle > ANGLE_QUARTER ? FORCED_ACTION_STAGGER_FRONT : FORCED_ACTION_STAGGER_BACK;
            TriggerFighterRumble(fighter, RUMBLE_WOUND);
            fighter->faceExpression = FIGHTER_FACE_WOUND;
        }
        break;
    case 6: case 7: case 8: case 9:
        fighter->leftArmIntact = 0;
        fighter->pendingAction = FORCED_ACTION_LEFT_ARM_CRIPPLE;
        TriggerFighterRumble(fighter, RUMBLE_WOUND);
        fighter->faceExpression = FIGHTER_FACE_WOUND;
        break;
    case 10: case 11: case 12: case 13:
        fighter->pendingAction = FORCED_ACTION_RIGHT_ARM_WOUND;
        fighter->rightArmWoundCount++;
        TriggerFighterRumble(fighter, RUMBLE_WOUND);
        fighter->faceExpression = FIGHTER_FACE_WOUND;
        break;
    case 14: case 15: case 18: case 19:
        fighter->pendingAction = FORCED_ACTION_THIGH_WOUND;
        fighter->thighWoundCount++;
        TriggerFighterRumble(fighter, RUMBLE_WOUND);
        fighter->faceExpression = FIGHTER_FACE_WOUND;
        break;
    case 16: case 17: case 20: case 21:
        fighter->legsIntact = 0;
        fighter->pendingAction = FORCED_ACTION_LEG_CRIPPLE;
        TriggerFighterRumble(fighter, RUMBLE_WOUND);
        fighter->faceExpression = FIGHTER_FACE_WOUND;
        break;
    }
    EmitCombatEvent(slot, event, (VECTOR *)&COLLISION_SCRATCH->bodyPartPos[slot][sphere]);
    MarkFighterInjuredRegion(slot, sphere);
}

int ResolveBladeClashReaction(Fighter *fighter, int impactAngle, VECTOR *contactPoint)
{
    int result = 1;
    short action = fighter->action;
    Fighter *opponent = fighter->opponent;
    unsigned char animFlags;
    int stance, opponentStance;
    int event;
    unsigned int legs;

    if (action == FIGHTER_ACTION_DEAD || action == FIGHTER_ACTION_FATAL_KO || action == FIGHTER_ACTION_DEFEATED ||
        (fighter->placement->flags & MOTION_FLAG_FATAL) || action == FIGHTER_ACTION_DOWNED ||
        action == FIGHTER_ACTION_SPECIAL) {
        TriggerFighterRumble(fighter, RUMBLE_BLADE_CLASH);
    } else if (action == FIGHTER_ACTION_ATTACK) {
        animFlags = fighter->animInfo->flags;
        if (animFlags & ANIM_FLAG_DOWNED)
            return 1;
        legs = (unsigned short)fighter->legsIntact;
        if ((unsigned short)opponent->legsIntact == legs) {
            stance = animFlags & ANIM_FLAG_STANCE_MASK;
            opponentStance = opponent->animInfo->flags & ANIM_FLAG_STANCE_MASK;
            if (stance == opponentStance || (stance + 1) % 3 != opponentStance) {
                fighter->pendingAction = FORCED_ACTION_CLASH_RECOIL;
                TriggerFighterRumble(fighter, RUMBLE_BLADE_CLASH);
            }
        } else if (legs == 0) {
            fighter->pendingAction = FORCED_ACTION_CLASH_RECOIL;
            TriggerFighterRumble(fighter, RUMBLE_BLADE_CLASH);
        }
    } else if (action == FIGHTER_ACTION_NEUTRAL || action == FIGHTER_ACTION_CROUCH ||
               action == FIGHTER_ACTION_CLIMB || action == FIGHTER_ACTION_FALL) {
        fighter->pendingAction = FORCED_ACTION_CLASH_RECOIL;
        TriggerFighterRumble(fighter, RUMBLE_BLADE_CLASH);
    } else if (action == FIGHTER_ACTION_GUARD && impactAngle > ANGLE_QUARTER) {
        result = 0;
        if (fighter->clashPower > opponent->clashPower) {
            fighter->pendingAction = opponent->animInfo->flags & ANIM_FLAG_STANCE_MASK;
            TriggerFighterRumble(fighter, RUMBLE_GUARD_PARRY);
            event = COMBAT_EVENT_GUARD_PARRY;
        } else {
            fighter->pendingAction =
                (fighter->animInfo->flags & ANIM_FLAG_STANCE_MASK) + FORCED_ACTION_GUARD_BREAK_BASE;
            TriggerFighterRumble(fighter, RUMBLE_GUARD_BREAK);
            event = COMBAT_EVENT_GUARD_BREAK;
        }
        EmitCombatEvent(fighter->side, event, contactPoint);
    } else {
        fighter->pendingAction = FORCED_ACTION_CLASH_RECOIL;
        TriggerFighterRumble(fighter, RUMBLE_BLADE_CLASH);
    }
    return result;
}

void FireGunshotRayAndCheckHit(Fighter *target, int *hitMask, int *vitalHitMask)
{
    CollisionWork *work = &COLLISION_SCRATCH->work;
    Fighter *shooter = target->opponent;
    int targetSide = target->side;
    FighterHitSphere *sphere;
    int i;
    int sphereBit;
    int floorY;
    int dx;
    int dz;
    int wallDistSq;
    short ammo;

    if (shooter->action != FIGHTER_ACTION_ATTACK) {
        return;
    }
    if (shooter->animInfo->activeStartFrame != shooter->animFrame) {
        return;
    }
    ammo = shooter->gunAmmo;
    if (ammo == 0) {
        return;
    }
    shooter->gunAmmo = ammo - 1;
    if (gGameState != GAME_STATE_UPDATE_SLASH_CLEAR_CUTSCENE) {
        TriggerFighterRumble(shooter, RUMBLE_GUNSHOT);
    }
    COLLISION_SCRATCH->work.normalOrTarget.x = COLLISION_SCRATCH->weaponPoints[targetSide == 0][1].x;
    COLLISION_SCRATCH->work.normalOrTarget.y =
        COLLISION_SCRATCH->weaponPoints[targetSide == 0][1].y - GUNSHOT_MUZZLE_Y_OFFSET;
    COLLISION_SCRATCH->work.normalOrTarget.z = COLLISION_SCRATCH->weaponPoints[targetSide == 0][1].z;
    EmitCombatEvent(targetSide == 0, COMBAT_EVENT_GUNSHOT, (VECTOR *)&COLLISION_SCRATCH->work.normalOrTarget);
    if (gGameState == GAME_STATE_UPDATE_SLASH_CLEAR_CUTSCENE) {
        return;
    }
    COLLISION_SCRATCH->work.primaryVerts[0] =
        targetSide == 0 ? &COLLISION_SCRATCH->weaponPoints[1][0] : &COLLISION_SCRATCH->weaponPoints[0][0];
    work->primaryVerts[1] =
        targetSide == 0 ? &COLLISION_SCRATCH->weaponPoints[1][1] : &COLLISION_SCRATCH->weaponPoints[0][1];
    COLLISION_SCRATCH->weaponPoints[targetSide == 0][1].y = COLLISION_SCRATCH->weaponPoints[targetSide == 0][0].y;
    BuildSegmentFrame(work);
    sphere = gFighterHitSpheres[targetSide];
    work->edgeA.z <<= GUNSHOT_RAY_LENGTH_SHIFT;
    for (i = 0; i < FIGHTER_HIT_SPHERE_COUNT; i++, sphere++) {
        if (target->leftArmIntact != 0 ||
            (unsigned int)(i - CRIPPLED_LEFT_ARM_SKIP_FIRST) >= CRIPPLED_LEFT_ARM_SKIP_COUNT) {
            if (TestSphereVsSegmentFrame(work, &COLLISION_SCRATCH->bodyPartPos[targetSide][i],
                                         sphere->outerRadius, sphere->outerRadiusSq) != 0) {
                sphereBit = 1 << i;
                *hitMask |= sphereBit;
                if (sphere->isVital != 0 &&
                    TestSphereVsSegmentFrame(work, &COLLISION_SCRATCH->bodyPartPos[targetSide][i],
                                             sphere->vitalRadius, sphere->vitalRadiusSq) != 0) {
                    *vitalHitMask |= sphereBit;
                }
            }
        }
    }
    i = 0;
    dx = work->primaryVerts[1]->x - work->primaryVerts[0]->x;
    dz = work->primaryVerts[1]->z - work->primaryVerts[0]->z;
    work->normalOrTarget.x = work->primaryVerts[1]->x;
    work->normalOrTarget.y = work->primaryVerts[1]->y;
    work->normalOrTarget.z = work->primaryVerts[1]->z;
    while (i < GUNSHOT_WALL_MARCH_STEPS) {
        floorY = QueryFloorHeight(work->normalOrTarget.x, work->normalOrTarget.y, work->normalOrTarget.z);
        if (floorY == FLOOR_QUERY_MISS) {
            floorY = FLOOR_HEIGHT_NONE;
        }
        if (floorY < work->normalOrTarget.y) {
            break;
        }
        i++;
        work->normalOrTarget.x += dx;
        work->normalOrTarget.z += dz;
    }
    if (i < GUNSHOT_WALL_MARCH_STEPS) {
        if (*hitMask != 0) {
            wallDistSq = i * i * (dx * dx + dz * dz);
            dx = work->primaryVerts[1]->x - target->position.vx;
            dz = work->primaryVerts[1]->z - target->position.vz;
            if (wallDistSq >= dx * dx + dz * dz) {
                return;
            }
            *vitalHitMask = 0;
            *hitMask = 0;
            EmitCombatEvent(targetSide == 0, COMBAT_EVENT_GUN_IMPACT, (VECTOR *)&work->normalOrTarget);
        } else {
            EmitCombatEvent(targetSide == 0, COMBAT_EVENT_GUN_IMPACT, (VECTOR *)&work->normalOrTarget);
        }
    }
}

void UpdateBladeLockSeparation(void)
{
    Fighter *p1 = &gFighters[0];
    Fighter *p2 = &gFighters[1];
    int dx;
    int dz;
    unsigned int distanceSquared;
    int distance;
    int leadingZeros;
    int separationPush;
    int struggleBias;
    int p1VelX, p1VelZ, p2VelX, p2VelZ;

    dx = p1->position.vx - p2->position.vx;
    dz = p1->position.vz - p2->position.vz;
    distanceSquared = dx * dx + dz * dz;
    if ((unsigned int)distanceSquared < 1024) {
        distance = gSqrtLutQ3[distanceSquared] >> 3;
    } else {
        unsigned int shift;
        gte_ldlzc(distanceSquared);
        gte_nop();
        gte_nop();
        gte_stlzc(&leadingZeros);
        shift = leadingZeros;
        shift = 22 - (shift & ~1);
        distanceSquared = gSqrtLutQ3[(unsigned int)distanceSquared >> shift];
        distance = ((unsigned int)distanceSquared << 16) >> (19 - (shift >> 1));
    }
    if (distance < BLADE_LOCK_TARGET_DIST) {
        separationPush = (BLADE_LOCK_TARGET_DIST - distance) / 2;
    } else {
        separationPush = (BLADE_LOCK_TARGET_DIST - distance) / 8;
    }
    struggleBias = (p2->bladeLockMeter - p1->bladeLockMeter) >> 9;
    if (struggleBias > -BLADE_LOCK_BIAS_DEADZONE && struggleBias < BLADE_LOCK_BIAS_DEADZONE) {
        struggleBias = 0;
    }
    if (struggleBias < -BLADE_LOCK_BIAS_MAX) {
        struggleBias = -BLADE_LOCK_BIAS_MAX;
    } else if (struggleBias > BLADE_LOCK_BIAS_MAX) {
        struggleBias = BLADE_LOCK_BIAS_MAX;
    }
    p1VelX = dx * (separationPush + struggleBias) / distance;
    p1VelZ = dz * (separationPush + struggleBias) / distance;
    p2VelX = dx * (struggleBias - separationPush) / distance;
    p2VelZ = dz * (struggleBias - separationPush) / distance;
    p1->velocity.x += p1VelX;
    p1->velocity.z += p1VelZ;
    p2->velocity.x += p2VelX;
    p2->velocity.z += p2VelZ;
}

void UpdateBladeLockInput(Fighter *fighter)
{
    Fighter *opponent;
    MotionPlacement *placement;
    int i;
    int pushed;

    opponent = fighter->opponent;
    if (gBladeLockState == BLADE_LOCK_BREAK) {
        return;
    }
    if (gBladeLockState == BLADE_LOCK_ADVANTAGE) {
        return;
    }

    pushed = 0;
    for (i = 0; i < 3; i++) {
        if (fighter->currentInput.pressed & gBladeLockMashButtons[i]) {
            fighter->bladeLockMeter += (fighter->clashPower * 2) / (fighter->bladeLockTimers[i] + 1);
            fighter->bladeLockTimers[i] = 0;
            pushed = 1;
        } else {
            fighter->bladeLockTimers[i]++;
        }
    }

    if (!pushed) {
        fighter->bladeLockMeter = (fighter->bladeLockMeter * 7) / 8;
    }

    if (fighter->action == FIGHTER_ACTION_BLADE_LOCK) {
        return;
    }

    if (fighter->bladeLockMeter >= BLADE_LOCK_PUSH_THRESHOLD) {
        fighter->pendingAction = BLADE_LOCK_ACTION_PUSH;
    } else {
        fighter->pendingAction = BLADE_LOCK_ACTION_DEFEND;
    }

    placement = fighter->placement;
    if (placement->actionTable[fighter->pendingAction] == placement) {
        fighter->pendingAction = FORCED_ACTION_NONE;
    }

    if (fighter->bladeLockMeter < BLADE_LOCK_BREAK_SELF_MAX && opponent->bladeLockMeter < BLADE_LOCK_BREAK_OPP_MAX) {
        if (fighter->currentInput.held & PADLleft) {
            fighter->pendingAction = BLADE_LOCK_ACTION_DISENGAGE;
            gBladeLockState = BLADE_LOCK_BREAK;
        } else if (fighter->currentInput.held & PADLup) {
            goto sidestep_left;
        }
    }

    if (fighter->currentInput.held & PADLup) {
    sidestep_left:
        fighter->velocity.x =
            (gSinLutQ12[(fighter->angles.heading - ANGLE_QUARTER) & ANGLE_MASK] * fighter->statureScale) >> 20;
        fighter->velocity.z = (gSinLutQ12[fighter->angles.heading & ANGLE_MASK] * fighter->statureScale) >> 20;
    } else if (fighter->currentInput.held & PADLdown) {
        fighter->velocity.x =
            (gSinLutQ12[(fighter->angles.heading + ANGLE_QUARTER) & ANGLE_MASK] * fighter->statureScale) >> 20;
        fighter->velocity.z =
            (gSinLutQ12[(fighter->angles.heading + ANGLE_HALF) & ANGLE_MASK] * fighter->statureScale) >> 20;
    }
}

void InitFighterBladeLock(Fighter *fighter)
{
    int i;

    fighter->pendingAction = FORCED_ACTION_BLADE_LOCK_ENTER;
    fighter->hitActive = 0;
    fighter->bladeLockMeter = 0;
    for (i = 3; i >= 0; i--) {
        fighter->bladeLockTimers[i] = 0;
    }
    gBladeLockWinner = -1;
}

void UpdateBladeLockState(void)
{
    Fighter *first = &gFighters[0];
    Fighter *second = &gFighters[1];
    VECTOR center;
    int timer = gBladeLockTimer;
    int side, delta;
    short firstAction, secondAction;

    if (timer >= 16 && (timer & 7) == 0) {
        center.vx = (COLLISION_SCRATCH->bodyPartPos[0][FIGHTER_SPHERE_TORSO].x +
                     COLLISION_SCRATCH->bodyPartPos[1][FIGHTER_SPHERE_TORSO].x) /
                    2;
        center.vy = (COLLISION_SCRATCH->bodyPartPos[0][FIGHTER_SPHERE_TORSO].y +
                     COLLISION_SCRATCH->bodyPartPos[1][FIGHTER_SPHERE_TORSO].y) /
                        2 -
                    BLADE_LOCK_SPARK_Y_OFFSET;
        center.vz = (COLLISION_SCRATCH->bodyPartPos[0][FIGHTER_SPHERE_TORSO].z +
                     COLLISION_SCRATCH->bodyPartPos[1][FIGHTER_SPHERE_TORSO].z) /
                    2;
        EmitCombatEvent((timer >> 3) & 1, COMBAT_EVENT_BLADE_LOCK_SPARK, &center);
    }
    gBladeLockTimer++;
    if (gBladeLockState == BLADE_LOCK_ADVANTAGE) {
        side = gBladeLockWinner;
        if (gFighters[side].animFrame + 1 == gFighters[side].placement->frameLimit) {
            gFighters[side].pendingAction = BLADE_LOCK_ACTION_WIN_PUSH;
            gFighters[!side].pendingAction = BLADE_LOCK_ACTION_LOSE_STAGGER;
            gBladeLockState = BLADE_LOCK_ENDING;
        } else if (gFighters[!side].currentInput.pressed & (PADLleft | PADLdown | PADLup)) {
            gFighters[side].pendingAction = BLADE_LOCK_ACTION_ADVANTAGE_EVADED;
            gFighters[!side].pendingAction = BLADE_LOCK_ACTION_DISENGAGE;
            gBladeLockState = BLADE_LOCK_ENDING;
        }
    } else if (gBladeLockState == BLADE_LOCK_BREAK) {
        firstAction = first->pendingAction;
        if (firstAction == FORCED_ACTION_NONE || (unsigned int)firstAction < BLADE_LOCK_ACTION_WIN_PUSH) {
            first->pendingAction = BLADE_LOCK_ACTION_MUTUAL_BREAK;
        }
        secondAction = second->pendingAction;
        if (secondAction == FORCED_ACTION_NONE || (unsigned int)secondAction < BLADE_LOCK_ACTION_WIN_PUSH) {
            second->pendingAction = BLADE_LOCK_ACTION_MUTUAL_BREAK;
        }
        gBladeLockState = BLADE_LOCK_ENDING;
    } else if (first->action != FIGHTER_ACTION_BLADE_LOCK) {
        delta = second->bladeLockMeter - first->bladeLockMeter;
        if (delta > BLADE_LOCK_ADVANTAGE_DIFF) {
            if (!(second->currentInput.held & PADLright)) {
                gBladeLockState = BLADE_LOCK_ADVANTAGE;
                gBladeLockWinner = 1;
                second->pendingAction = BLADE_LOCK_ACTION_ADVANTAGE;
            }
        } else if (delta < -BLADE_LOCK_ADVANTAGE_DIFF) {
            if (!(first->currentInput.held & PADLright)) {
                gBladeLockState = BLADE_LOCK_ADVANTAGE;
                gBladeLockWinner = 0;
                first->pendingAction = BLADE_LOCK_ACTION_ADVANTAGE;
            }
        }
    }
}

void ResolveFighterBodyCollision(void)
{
    int side, i, j;
    int contacts;
    int p1PushX, p1PushZ, p2PushX, p2PushZ;
    int leadingZeros;
    int dx, dy, dz;
    int combinedRadius, distance, overlap, clashBias;
    int p1Scale, p2Scale;
    int p1StepX, p1StepZ, p2StepX, p2StepZ;
    int p1RunningFast, p2RunningFast, headingDiff;

    for (side = 0; side < 2; side++) {
        COLLISION_SCRATCH->bodyCenters[side][0].x = COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_TORSO].x;
        COLLISION_SCRATCH->bodyCenters[side][0].y = COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_TORSO].y;
        COLLISION_SCRATCH->bodyCenters[side][0].z = COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_TORSO].z;
        COLLISION_SCRATCH->bodyCenters[side][1].x =
            (COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_RIGHT_THIGH].x +
             COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_LEFT_THIGH].x) /
            2;
        COLLISION_SCRATCH->bodyCenters[side][1].y =
            (COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_RIGHT_THIGH].y +
             COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_LEFT_THIGH].y) /
            2;
        COLLISION_SCRATCH->bodyCenters[side][1].z =
            (COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_RIGHT_THIGH].z +
             COLLISION_SCRATCH->bodyPartPos[side][FIGHTER_SPHERE_LEFT_THIGH].z) /
            2;
    }

    contacts = 0;
    p2PushZ = 0;
    p2PushX = 0;
    p1PushZ = 0;
    p1PushX = 0;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            dx = COLLISION_SCRATCH->bodyCenters[0][i].x - COLLISION_SCRATCH->bodyCenters[1][j].x;
            dy = COLLISION_SCRATCH->bodyCenters[0][i].y - COLLISION_SCRATCH->bodyCenters[1][j].y;
            dz = COLLISION_SCRATCH->bodyCenters[0][i].z - COLLISION_SCRATCH->bodyCenters[1][j].z;
            combinedRadius = gFighterBodyRadii[i] + gFighterBodyRadii[j];
            if (dx > combinedRadius || dx < -combinedRadius || dy > combinedRadius || dy < -combinedRadius ||
                dz > combinedRadius || dz < -combinedRadius) {
                continue;
            }
            distance = dx * dx + dy * dy + dz * dz;
            if (distance > combinedRadius * combinedRadius) {
                continue;
            }

            contacts++;
            {
                unsigned int sqrtInput = distance;
                if ((unsigned int)sqrtInput < 1024) {
                    distance = gSqrtLutQ3[sqrtInput] >> 3;
                } else {
                    unsigned int shift;
                    gte_ldlzc(sqrtInput);
                    gte_nop();
                    gte_nop();
                    gte_stlzc(&leadingZeros);
                    shift = leadingZeros;
                    shift = 22 - (shift & ~1);
                    sqrtInput = gSqrtLutQ3[(unsigned int)sqrtInput >> shift];
                    distance = ((unsigned int)sqrtInput << 16) >> (19 - (shift >> 1));
                }
            }
            overlap = combinedRadius - distance;
            if (overlap > 128) {
                overlap = 128;
            }
            if (distance == 0) {
                distance = 1;
            }
            clashBias = (gFighters[1].clashPower - gFighters[0].clashPower) / 4;
            p1Scale = overlap * (1024 + clashBias);
            p1StepX = dx * p1Scale / distance;
            p1StepZ = dz * p1Scale / distance;
            p2Scale = overlap * (1024 - clashBias);
            p2StepX = dx * p2Scale / distance;
            p2StepZ = dz * p2Scale / distance;
            p1PushX += p1StepX >> 10;
            p1PushZ += p1StepZ >> 10;
            p2PushX -= p2StepX >> 10;
            p2PushZ -= p2StepZ >> 10;
        }
    }

    if (contacts) {
        p1RunningFast =
            (gFighters[0].animInfo->flags & ANIM_FLAG_RUN) && gFighters[0].runSpeed > FIGHTER_SPRINT_THRESHOLD;
        p2RunningFast =
            (gFighters[1].animInfo->flags & ANIM_FLAG_RUN) && gFighters[1].runSpeed > FIGHTER_SPRINT_THRESHOLD;
        headingDiff = (gFighters[0].angles.heading - gFighters[1].angles.heading) & ANGLE_MASK;
        if (headingDiff >= ANGLE_HALF) {
            headingDiff = ANGLE_FULL - headingDiff;
        }
        if (p1RunningFast) {
            if (p2RunningFast) {
                if (headingDiff > ANGLE_THREE_EIGHTHS) {
                    contacts = 0;
                    gFighters[1].pendingAction = FORCED_ACTION_HEAD_ON_STUMBLE;
                    gFighters[0].pendingAction = FORCED_ACTION_HEAD_ON_STUMBLE;
                    TriggerFighterRumble(&gFighters[0], RUMBLE_BODY_BUMP);
                    TriggerFighterRumble(&gFighters[1], RUMBLE_BODY_BUMP);
                }
            } else if (headingDiff < ANGLE_EIGHTH && gFighters[1].action == FIGHTER_ACTION_NEUTRAL) {
                gFighters[1].pendingAction = FORCED_ACTION_STAGGER_BACK;
                EmitCombatEvent(1, COMBAT_EVENT_BODY_BUMP, (VECTOR *)COLLISION_SCRATCH->bodyCenters[1]);
                TriggerFighterRumble(&gFighters[1], RUMBLE_BODY_BUMP);
            }
        } else if (p2RunningFast && headingDiff < ANGLE_EIGHTH && gFighters[0].action == FIGHTER_ACTION_NEUTRAL) {
            gFighters[0].pendingAction = FORCED_ACTION_STAGGER_BACK;
            EmitCombatEvent(0, COMBAT_EVENT_BODY_BUMP, (VECTOR *)COLLISION_SCRATCH->bodyCenters[0]);
            TriggerFighterRumble(&gFighters[0], RUMBLE_BODY_BUMP);
        }
        if (contacts) {
            gFighters[0].velocity.x += p1PushX;
            gFighters[0].velocity.z += p1PushZ;
            gFighters[1].velocity.x += p2PushX;
            gFighters[1].velocity.z += p2PushZ;
        }
    }
}
