#ifndef STAGE_TABLES_H
#define STAGE_TABLES_H

#include "world_vector.h"

#define STAGE_TRANSITION_TABLE_COUNT 28
#define STAGE_TRANSITION_END (-1)

/* Directed stage portal trigger box and paired world-space origins
 * (gStageTransitions[STAGE_TRANSITION_TABLE_COUNT]; 0x20 bytes).
 * Each entry is one-way from stageA to stageB (return portals are separate entries with shifted boxes);
 * FindActiveStageTransition tests both fighters' XZ against [anchors[0].x, +width) x [anchors[0].z, +depth),
 * CheckStageTransition returns stageB, and InitFighterForScene / InitCombatState shift coordinates by
 * anchors[index] - anchors[!index] where index = (stageA != stageId). */
typedef struct StageTransition {
    short stageA;            /* +0x00: source stage ID (STAGE_TRANSITION_END terminates table) */
    short stageB;            /* +0x02: destination stage ID */
    short width;             /* +0x04: portal box X extent */
    short depth;             /* +0x06: portal box Z extent */
    WorldVector anchors[2]; /* +0x08: [0] source-stage origin, [1] destination-stage origin */
} StageTransition;

extern StageTransition *gPendingTransition;

#define STAGE_SPAWN_PAIR_COUNT 4

StageTransition *GetStageTransitionTable(void);
WorldVector *GetStageSpawnTable(void);

#endif
