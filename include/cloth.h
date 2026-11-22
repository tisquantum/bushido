#ifndef CLOTH_H
#define CLOTH_H

#include <sys/types.h>
#include <libgte.h>

/* The three wind channels: one per fighter's cloth, and the stage's, which snow, rain, stage cloth and
 * foliage read. */
typedef enum ClothWindSlot {
    CLOTH_WIND_SLOT_FIGHTER_0 = 0,
    CLOTH_WIND_SLOT_FIGHTER_1 = 1,
    CLOTH_WIND_SLOT_STAGE = 2,
    CLOTH_WIND_SLOT_COUNT = 3
} ClothWindSlot;

/*
 * 16-byte live wind channel state. Updated by ResetClothWindSlots / StepClothWindSlot and loaded by
 * LoadClothWindToScratch.
 */
typedef struct Wind {
    int strength;         /* wind magnitude (0..8192; 1024 default, multiplied by Q12 rcos/rsin) */
    int angle;            /* 4096 to the turn, passed to rcos/rsin */
    int gustColumnOrigin; /* of gClothWindFieldLut, +153 a step (StepClothWindSlot) */
    int gustRowOrigin;    /* +827 a step */
} Wind;

#define CLOTH_TETHER_MIN      (-255) /* massOrPin >= -255: soft-tethered; -256: fully free */
#define CLOTH_FIGHTER_DAMPING 0xe00  /* 7/8 in Q12, the fighters' damping;
                                        the stage step measures its push limit from it */

/* 64-byte cloth mass-spring point state (positions scaled by << 7). */
typedef struct ClothPoint {
    int x, y, z;
    int vx, vy, vz;
    int massOrPin;      /* 0 = rigid pin, >0 = soft pin, <0 = dynamic (-256 free) */
    int outLinkCount;
    int inLinkCount;
    u_char outLinks[8];
    u_char inLinks[8];
    int windWeight;     /* distance-from-center wind weight (0..256) */
    int unused[2];      /* 8 bytes nothing reads or writes */
} ClothPoint;

/* 16-byte cloth spring link constraint. */
typedef struct ClothLink {
    int restLength;
    union {
        u_int packed;
        struct {
            u_short to;
            short from;
        } indices;
    } endpoints;
    int maxLength;      /* restLength * 3; written, never read */
    int stiffnessDiv;   /* stiffness divisor: 3 on stage cloth; 2 on fighters, whose solver does not read it */
} ClothLink;

#define CLOTH_MODEL_COUNT 16

/* 52-byte active soft-body cloth model controller in gClothModels[16]. */
typedef struct ClothModel {
    SVECTOR *sourceVertices; /* NULL = free slot */
    short pointCount;
    short linkCount;
    int damping;             /* Q12 velocity damping (3712 stage, 0xE00 fighter) */
    ClothPoint *points;
    ClothLink *links;
    MATRIX prevTransform;
} ClothModel;

/*
 * 20-byte allocation descriptor passed to AllocClothModel / InitClothPointsAndLinks. The work buffer
 * holds the model's ClothPoint array followed by its ClothLink array; AllocClothModel splits it.
 */
typedef struct ClothModelSpec {
    short pointCount;
    short damping;
    SVECTOR *sourceVertices;
    short *faceLinkLists;
    MATRIX *initialTransform;
    void *workBuffer;
} ClothModelSpec;

/* ClothColliderSet from floorY on, which ClothItem.colliderInput points at. */
typedef struct ClothColliderInput {
    int floorY;
    int activeCount;
    VECTOR worldEndpoints[3][2]; /* the limb capsules' ends in world space */
    int radii[3];
} ClothColliderInput;

/*
 * The limb capsules one cloth part collides with (gFighterClothColliders[3][20] in fighter_cloth.c): added by
 * AddClothLimbCollider, carried to world space each frame by TransformClothColliders.
 */
typedef struct ClothColliderSet {
    int count;                    /* capsules added */
    struct SceneNode *limbs[3];   /* the limb node whose worldMatrix places each capsule */
    ClothColliderInput input;
    SVECTOR localEndpoints[3][2]; /* the capsules' ends in their limb's space */
} ClothColliderSet;

/* ClothItem.flags bitmask tested by UpdateClothModelsWithOutput / UpdateFighterClothModels. */
typedef enum ClothItemFlag {
    CLOTH_ITEM_FLAG_NONE = 0,
    CLOTH_ITEM_FLAG_RESET = 1
} ClothItemFlag;

#define FIGHTER_CLOTH_SLOT_COUNT 3
#define FIGHTER_CLOTH_MAX_ITEMS 20

/*
 * 16-byte cloth simulation item passed to UpdateClothModelsWithOutput and UpdateFighterClothModels;
 * the fighters' items are gFighterClothItems[20] (fighter_cloth.c), whose outputPositions is the cloth
 * node's work buffer.
 */
typedef struct ClothItem {
    short modelIndex;                         /* gClothModels[16] slot returned by AllocClothModel */
    u_char flags;                             /* ClothItemFlag bitmask (bit 0 = reset to rest pose) */
    u_char windSlot;                          /* ClothWindSlot index (0..2) for LoadClothWindToScratch */
    MATRIX *transform;                        /* world transform MATRIX */
    VECTOR *outputPositions;                  /* world-space output vertices (>> 7), or NULL */
    struct ClothColliderInput *colliderInput; /* &ClothColliderSet.input, or NULL */
} ClothItem;

extern unsigned char gSqrtLutQ3[1024]; /* floor(sqrt(i) * 8) */

void ClearClothModels(void);
int CalcClothWorkSize(int pointCount, int linkCount);
int AllocClothModel(ClothModelSpec *spec);
void FreeClothModel(int clothSlot);
void RelocateClothWorkPtrs(int clothSlot, int deltaBytes);
void RelocateClothVertexPtr(int clothSlot, int deltaBytes);
void ResetClothWindSlots(void);
void StepClothWindSlot(int magnitude, int angle, int windSlot);
void UpdateClothModelsWithOutput(ClothItem *items, int count);
void UpdateFighterClothModels(ClothItem *items, int count);

extern ClothModel gClothModels[CLOTH_MODEL_COUNT];
extern Wind gClothWindSlots[CLOTH_WIND_SLOT_COUNT];

void SolveStageClothLinks(ClothModel *model);
void StepStageClothModel(ClothItem *item, ClothModel *model);
void ResetClothModelPositions(ClothItem *item, ClothModel *model);
void SolveFighterClothLinks(ClothModel *model);
void StepFighterClothModel(ClothItem *item, ClothModel *model);
void ResetFighterClothPositions(ClothItem *item, ClothModel *model);

#endif
