#ifndef STAGE_PROP_HIT_H
#define STAGE_PROP_HIT_H

#include "world_vector.h"

/* StagePropMarker.kind (+0x00): 0 terminates the list, 2 is a cuttable prop,
 * any other nonzero value is a solid blocking prop that bounces weapons. */
typedef enum StagePropKind {
    STAGE_PROP_END = 0,
    STAGE_PROP_CUTTABLE = 2
} StagePropKind;

/* 16-byte stage prop marker in gStagePropMarkers (stage archive section 4). */
typedef struct StagePropMarker {
    short kind;        /* +0x00: StagePropKind */
    short cut;         /* +0x02: nonzero once a cuttable prop has been cut */
    WorldVector pos;   /* +0x04: world (x, y, z) */
} StagePropMarker;

void SetStagePropMarkers(StagePropMarker *markers);
int CheckWeaponStagePropHit(int side);
void InitStagePropMarkers(StagePropMarker *markers);

#endif
