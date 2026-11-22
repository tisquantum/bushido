#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

typedef struct EffectTextureEntry {
    unsigned char u, v;
    unsigned short tpage, clut;
} EffectTextureEntry;

#define BILLBOARD_EFFECT_POOL_SIZE  16
#define BILLBOARD_EFFECT_KIND_COUNT 14

/* Nothing spawns kinds 1, 3 and 8 to 14 on any build; 1 and 8 to 14 have no texture. */
typedef enum BillboardEffectKind {
    BILLBOARD_EFFECT_FREE = 0,
    BILLBOARD_EFFECT_KIND_1 = 1,
    BILLBOARD_EFFECT_LIMB_WOUND = 2,
    BILLBOARD_EFFECT_ORANGE_SPARK = 3,
    BILLBOARD_EFFECT_FATAL_HIT_PRIMARY = 4,
    BILLBOARD_EFFECT_WEAPON_CLASH = 5,
    BILLBOARD_EFFECT_FATAL_HIT_SECONDARY = 6,
    BILLBOARD_EFFECT_TORSO_WOUND = 7,
    BILLBOARD_EFFECT_KIND_8 = 8,
    BILLBOARD_EFFECT_KIND_9 = 9,
    BILLBOARD_EFFECT_KIND_10 = 10,
    BILLBOARD_EFFECT_KIND_11 = 11,
    BILLBOARD_EFFECT_KIND_12 = 12,
    BILLBOARD_EFFECT_KIND_13 = 13,
    BILLBOARD_EFFECT_KIND_14 = 14,
    BILLBOARD_EFFECT_DELAYED_AMBIENT = 15
} BillboardEffectKind;

#define BILLBOARD_FT4_OPAQUE    0x2c808080
#define BILLBOARD_FT4_SEMITRANS 0x2e808080

typedef enum BillboardSheetLayout {
    BILLBOARD_SHEET_VERTICAL = 0,
    BILLBOARD_SHEET_HORIZONTAL = 1,
    BILLBOARD_SHEET_VERTICAL_ALT = 2
} BillboardSheetLayout;

typedef struct BillboardEffectSlot {
    short kind;
    short age;
    VECTOR pos;
    int unused[5];   /* 20 bytes; nothing reads or writes them */
} BillboardEffectSlot;

typedef struct EffectTextureTable {
    EffectTextureEntry entries[BILLBOARD_EFFECT_KIND_COUNT];
} EffectTextureTable;

typedef struct SpriteDesc {
    short width;
    short height;
    short frameCount;            /* ticks */
    short pad;
    u_long colorAndCode;
    u_short (*uvs)[4];           /* per frame, four corners, each u | v << 8 */
    u_char *frames;              /* frame of each tick */
    EffectTextureEntry *texture;
    int yOffset;
} SpriteDesc;

/* UpdateAndBuildBillboardEffects's working record in the scratchpad (render_scratch.h). */
typedef struct BillboardEffectWork {
    MATRIX matrix;
    VECTOR eye;
    POLY_FT4 *packets;
    int cameraAngle;
    SVECTOR corners[4];
    VECTOR delta;
    long *depths;
    POLY_FT4 **prims;
} BillboardEffectWork;

typedef int (*BillboardEffectHandler)(
    BillboardEffectWork *work,
    BillboardEffectSlot *slot,
    int primCount,
    POLY_FT4 **nextPrim
);

void ClearBillboardEffects(void);
void BuildEffectTextureUVs(EffectTextureEntry *textures);
int SpawnBillboardEffect(int kind, const VECTOR *position);
int UpdateAndDrawBillboardEffect(BillboardEffectWork *work, BillboardEffectSlot *slot, int primCount,
                                 POLY_FT4 **nextPrim, SpriteDesc *desc);
int UpdateAndBuildBillboardEffects(BillboardEffectWork *work);

extern BillboardEffectSlot gBillboardEffects[BILLBOARD_EFFECT_POOL_SIZE];
extern EffectTextureTable gBillboardTextures;

#endif
