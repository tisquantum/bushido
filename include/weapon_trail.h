#ifndef WEAPON_TRAIL_H
#define WEAPON_TRAIL_H

#include <sys/types.h>
#include <libgpu.h>
#include "world_vector.h"

#define WEAPON_TRAIL_HISTORY_LEN      8
#define WEAPON_TRAIL_HISTORY_MASK     (WEAPON_TRAIL_HISTORY_LEN - 1)
#define WEAPON_TRAIL_VISIBLE_SEGMENTS 6
#define WEAPON_TRAIL_LOD_BANDS        4
#define WEAPON_TRAIL_UV_STEPS         17
#define WEAPON_TRAIL_MAX_QUADS        48
#define WEAPON_TRAIL_MAX_PER_FIGHTER  3

/* WeaponTrailState.flags (+0x00) bitmask. */
typedef enum WeaponTrailFlags {
    WEAPON_TRAIL_FLAG_RESET = 0x01, /* reseed all 8 history edges from current endpoints */
    WEAPON_TRAIL_FLAG_FREEZE = 0x02 /* hold history ring buffer without advancing or smoothing */
} WeaponTrailFlags;

/* Historical pair of weapon-blade endpoints (24 bytes). */
typedef struct WeaponTrailEdge {
    WorldVector first; /* +0x00: first blade endpoint in world space */
    WorldVector last;  /* +0x0C: second blade endpoint in world space */
} WeaponTrailEdge;

/* Combat weapon slash trail state (gWeaponTrails[2][3]; 0xF8 bytes). */
typedef struct WeaponTrailState {
    u_short flags;                                   /* +0x00: WeaponTrailFlags bitmask */
    short cursor;                                    /* +0x02: ring-buffer cursor 0..7 (advances by 2/frame) */
    WeaponTrailEdge edges[WEAPON_TRAIL_HISTORY_LEN]; /* +0x04: 8 historical blade endpoint pairs (0xC0 bytes) */
    u_short uvRamp[WEAPON_TRAIL_UV_STEPS];           /* +0xC4: 17 interpolated packed (u,v) steps along blade */
    u_short unused;                                  /* +0xE6: nothing reads it */
    u_char uvStart[2];                               /* +0xE8: start (u,v) on trail texture strip */
    u_char uvEnd[2];                                 /* +0xEA: end (u,v) on trail texture strip */
    u_short tpage;                                   /* +0xEC: GPU texture page */
    u_short clut;                                    /* +0xEE: GPU CLUT */
    POLY_GT4 *packets[2];                            /* +0xF0: double-buffered WEAPON_TRAIL_MAX_QUADS-quad pools */
} WeaponTrailState;

void InitFighterWeaponTrails(int player);
void ResetFighterWeaponTrails(int player);
void DrawAllWeaponTrails(void);

#endif
