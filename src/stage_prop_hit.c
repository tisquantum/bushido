/* Weapon strikes against the stage: cuttable props (bamboo and the like) severed by a sweeping
 * blade, and Stage 19's water surface broken by a weapon. */

#include <sys/types.h>
#include <libgte.h>

#include "world_vector.h"
#include "fighter.h"
#include "collision_scratch.h"
#include "stage_id.h"
#include "weapon_id.h"
#include "combat_event.h"
#include "effect_slot.h"
#include "stage_prop_hit.h"
#include "rumble_pulse.h"
#include "cuttable_prop.h"
#include "geometry.h"
#include "stage_moat.h"
#include "globals.h"

/* CollisionWork.workPoints[2][4] viewed as [side][this frame, previous frame][the two weapon points swept]. */
typedef struct WeaponSweepGrid {
    WorldVector corner[2][2][2];
} WeaponSweepGrid;

static StagePropMarker *gStagePropMarkers;

void InitStagePropMarkers(StagePropMarker *markers)
{
    gStagePropMarkers = markers;
    while (markers->kind != STAGE_PROP_END) {
        markers->cut = 0;
        markers++;
    }
}

void SetStagePropMarkers(StagePropMarker *markers)
{
    gStagePropMarkers = markers;
}

int CheckWeaponStagePropHit(int side)
{
    CollisionWork *work = &COLLISION_SCRATCH->work;
    StagePropMarker *marker;
    WorldVector *a;
    WorldVector *b;
    WorldVector *c;
    int i;
    int j;
    int k;
    int coord;
    short weaponId;
    int blocked;

    if (gCurrentStageId == STAGE_ID_DOZAEMON_MOAT && work->workPoints[side][3].y < MOAT_WATER_SURFACE_Y &&
        work->workPoints[side][1].y >= MOAT_WATER_SURFACE_Y) {
        EmitCombatEvent(side, COMBAT_EVENT_WATER_SPLASH, (VECTOR *)&work->workPoints[side][1]);
    }
    work->primaryMax = work->workPoints[side][0];
    work->primaryMin = work->primaryMax;
    /* the original reads the four corners here as a two-by-two grid */
    for (i = 1; i < 4; i++) {
        j = i / 2;
        k = i & 1;
        coord = ((WeaponSweepGrid *)work->workPoints)->corner[side][j][k].x;
        if (coord < work->primaryMin.x) {
            work->primaryMin.x = coord;
        } else if (work->primaryMax.x < coord) {
            work->primaryMax.x = coord;
        }
        coord = ((WeaponSweepGrid *)work->workPoints)->corner[side][j][k].z;
        if (coord < work->primaryMin.z) {
            work->primaryMin.z = coord;
        } else if (work->primaryMax.z < coord) {
            work->primaryMax.z = coord;
        }
        coord = ((WeaponSweepGrid *)work->workPoints)->corner[side][j][k].y;
        if (work->primaryMax.y < coord) {
            work->primaryMax.y = coord;
        }
    }
    for (j = 0, marker = gStagePropMarkers; marker->kind != STAGE_PROP_END; j++, marker++) {
        if (marker->cut != 0 || marker->pos.y > work->primaryMax.y) {
            continue;
        }
        if (marker->pos.x < work->primaryMin.x || work->primaryMax.x < marker->pos.x) {
            continue;
        }
        if (marker->pos.z < work->primaryMin.z || work->primaryMax.z < marker->pos.z) {
            continue;
        }
        work->localPoint.vx = marker->pos.x;
        work->localPoint.vz = marker->pos.z;
        for (k = 0; k < 2; k++) {
            if (k == 0) {
                a = &work->workPoints[side][0];
                b = &work->workPoints[side][1];
                c = &work->workPoints[side][2];
            } else {
                a = &work->workPoints[side][1];
                b = &work->workPoints[side][2];
                c = &work->workPoints[side][3];
            }
            if (IsPointInTriangleXZ(a, b, c, (WorldVector *)&work->localPoint) != 0) {
                work->localPoint.vy = (work->primaryMin.y + work->primaryMax.y) / 2;
                if (marker->kind != STAGE_PROP_CUTTABLE) {
                    return 1;
                }
                weaponId = gFighters[side].weaponId;
                if (weaponId == WEAPON_ID_SLEDGEHAMMER) {
                    return 1;
                }
                if (weaponId == WEAPON_ID_NAGINATA) {
                    work->primaryMin = COLLISION_SCRATCH->weaponPoints[side][0];
                    work->primaryMax = gFighters[side].prevWeaponPoints[0];
                    for (k = 0; k < 2; k++) {
                        if (k == 0) {
                            a = &work->workPoints[side][3];
                            b = &work->workPoints[side][1];
                            c = &work->primaryMax;
                        } else {
                            a = &work->workPoints[side][1];
                            b = &work->primaryMax;
                            c = &work->primaryMin;
                        }
                        if (IsPointInTriangleXZ(a, b, c, (WorldVector *)&work->localPoint) != 0) {
                            goto cutProp;
                        }
                    }
                    blocked = 1;
                    goto done;
                }
cutProp:
                marker->cut = 1;
                TriggerCuttablePropFall(j, GetTriangleNormalHeading((VECTOR *)a, (VECTOR *)b, (VECTOR *)c));
                DispatchCombatEventEffect(side, COMBAT_EVENT_PROP_CUT, (VECTOR *)a);
                TriggerFighterRumble(&gFighters[side], RUMBLE_PROP_CUT);
                return 0;
            }
        }
    }
    blocked = 0;
done:
    return blocked;
}
