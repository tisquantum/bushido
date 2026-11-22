#ifndef EFFECT_SLOT_H
#define EFFECT_SLOT_H

#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>

#define EFFECT_POOL_SIZE 24

/* EffectSlot.active (+0x00) lifecycle state in gEffectPool[EFFECT_POOL_SIZE]. */
typedef enum EffectSlotState {
    EFFECT_SLOT_FREE = 0,
    EFFECT_SLOT_ACTIVE = 1,
    EFFECT_SLOT_UPDATE_ONLY = 2
} EffectSlotState;

/* Spray particle variant (+0x04) used by SpawnDirectionalSpray / UpdateSprayParticle. */
typedef enum SprayParticleKind {
    SPRAY_KIND_NONE = 0,          /* nothing spawns it */
    SPRAY_KIND_STREAM = 1,        /* Stage 25 stream splashes (UpdateGrottoStream) */
    SPRAY_KIND_WATERFALL = 2,     /* Stage 1 waterfall splashes (UpdateStage1Waterfall*) */
    SPRAY_KIND_WATER_SURFACE = 3, /* nothing spawns it; it would expire when pos.vy > 1200 */
    SPRAY_KIND_SHORELINE = 4      /* Stage 12 shoreline spray (UpdateCoveShoreline), expires when pos.vy > 400 */
} SprayParticleKind;


/* Effect type IDs passed to SpawnEffect and indexing gEffectTypeTable[EFFECT_TYPE_COUNT]. */
typedef enum EffectTypeId {
    EFFECT_PARRY_SPARK = 0,
    EFFECT_WALL_HIT_SPARK = 1,
    EFFECT_DUST_PUFF = 2,
    EFFECT_BLADE_LOCK_FLASH = 3,
    EFFECT_UNUSED_4 = 4,
    EFFECT_GUN_MUZZLE_FLASH_P1 = 5,
    EFFECT_GUN_MUZZLE_FLASH_P2 = 6,
    EFFECT_UNUSED_7 = 7,
    EFFECT_WATER_SPLASH = 8,
    EFFECT_WATER_SURFACE_SPLASH = 9,
    EFFECT_WATER_RIPPLE = 10,
    EFFECT_WATER_FOOTSTEP_SPLASH = 11,
    EFFECT_SPRAY_PARTICLE = 12,
    EFFECT_SNOW_FOOTSTEP_PUFF = 13,
    EFFECT_TYPE_COUNT = 14
} EffectTypeId;

/* Entry in gEffectTypeTable[EFFECT_TYPE_COUNT] used by SpawnEffect. */
typedef struct EffectType {
    short frameCount;
    short ticksPerFrame;
    void *mesh;
} EffectType;

/* EffectMesh*.flags (+0x00): bit 31 ends the mesh chain; low bits select SetupEffectTransform mode. */
#define EFFECT_MESH_BILLBOARD 0
#define EFFECT_MESH_WORLD     1
#define EFFECT_MESH_MODE_MASK 0x7fffffff
#define EFFECT_MESH_LAST      0x80000000

/* One textured quad of an effect mesh; vertexIndices index the mesh's vertices. */
typedef struct EffectQuad {
    union {
        CVECTOR color;
        u_long rgbc;
    } tint;
    u_char u0, v0;
    u_short clut;
    u_char u1, v1;
    u_short tpage;
    u_char u2, v2;
    u_char u3, v3;
    u_char vertexIndices[4];
} EffectQuad;

/* An effect mesh of vertexTotal vertices and quadSlots quads, of which quadCount from quadStart
 * are drawn. Meshes follow one another in memory up to one whose flags hold EFFECT_MESH_LAST. */
#define EFFECT_MESH(vertexTotal, quadSlots) \
    struct {                                \
        int flags;                          \
        int pos[3];                         \
        int vertexCount;                    \
        SVECTOR vertices[vertexTotal];      \
        int quadTotal;                      \
        int quadStart;                      \
        int quadCount;                      \
        EffectQuad quads[quadSlots];        \
    }

/* A four-vertex mesh of any quad count, as UpdateSpriteEffect reaches the sprite effects' meshes. */
typedef EFFECT_MESH(4, 0) EffectMesh4xN;
typedef EFFECT_MESH(4, 1) EffectMesh4x1;
typedef EFFECT_MESH(4, 3) EffectMesh4x3;
typedef EFFECT_MESH(4, 5) EffectMesh4x5;
typedef EFFECT_MESH(4, 6) EffectMesh4x6;
typedef EFFECT_MESH(4, 7) EffectMesh4x7;
typedef EFFECT_MESH(4, 8) EffectMesh4x8;
typedef EFFECT_MESH(4, 10) EffectMesh4x10;
typedef EFFECT_MESH(9, 4) EffectMesh9x4;
typedef EFFECT_MESH(9, 16) EffectMesh9x16;

struct EffectSlot;
typedef void (*EffectUpdateFn)(struct EffectSlot *);

/* 44-byte (0x2C) active 3D visual effect slot in gEffectPool[EFFECT_POOL_SIZE]. */
typedef struct EffectSlot {
    int active;            /* +0x00: EffectSlotState (0=free, 1=active+render, 2=update-only) */
    int variant;           /* +0x04: SprayParticleKind, Stage 4 light index (0..1), or UV-anim index (0..1) */
    VECTOR pos;            /* +0x08: world position (pad at +0x14 unused) */
    void *mesh;            /* +0x18: an EFFECT_MESH of any size, for DrawEffectMesh */
    EffectUpdateFn update; /* +0x1C: per-frame update callback */
    short frameOrTick;     /* +0x20: current frame, ring radius (water ring), or spray tick */
    short subTick;         /* +0x22: sub-frame tick / frame counter */
    union {
        struct {
            short frameCount;    /* +0x24: total frames for UpdateSpriteEffect */
            short ticksPerFrame; /* +0x26: ticks per frame for UpdateSpriteEffect */
        } sprite;
        struct {
            short x; /* +0x24: velocity X (Q4 fixed-point; integrated as x >> 4) */
            short y; /* +0x26: velocity Y (Q4 fixed-point; gravity added per frame) */
            short z; /* +0x28: velocity Z (Q4 fixed-point; integrated as z >> 4) */
        } vel;
        struct {
            short pitch;     /* +0x24: blood spray emitter pitch (4096 = 360 deg) */
            short yaw;       /* +0x26: blood spray emitter yaw (4096 = 360 deg) */
            short wavesLeft; /* +0x28: remaining 3-particle blood waves */
        } emitter;
    } params;
    short floorY; /* +0x2A: ballistic particle kill plane (expires when pos.vy > floorY) */
} EffectSlot;

#endif
