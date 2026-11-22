#ifndef SCENE_NODE_H
#define SCENE_NODE_H

#include "pose_angles.h"

/* SceneNode.renderType (+0x00) dispatch index into gSceneNodeRenderDispatch. */
typedef enum SceneNodeRenderType {
    SCENE_NODE_STANDARD = 0,
    SCENE_NODE_WORLD = 1,
    SCENE_NODE_SHADOW = 3,
    SCENE_NODE_SKINNED = 10,
    SCENE_NODE_TILE = 12,
    SCENE_NODE_CLOTH = 13,
    SCENE_NODE_OCEAN = 14,
    SCENE_NODE_POV_OVERLAY = 15,
    SCENE_NODE_CAMERA_MAIN = 0x64,
    SCENE_NODE_CAMERA_SECONDARY = 0x65
} SceneNodeRenderType;

/* SceneNode.matrixState (+0x5C) world-transform cache state. */
typedef enum SceneNodeMatrixState {
    SCENE_MATRIX_DIRTY = 0,
    SCENE_MATRIX_WORLD_VALID = 1,
    SCENE_MATRIX_LOCAL_PRECOMPUTED = 2
} SceneNodeMatrixState;

/* SceneNode.rotOrder (+0x68) index into gRotMatrixBuilders[].
 * The US/EU builds populate only the even orders; the Japanese build fills all six. */
typedef enum SceneNodeRotOrder {
    SCENE_ROT_ORDER_FAST = 0,
    SCENE_ROT_ORDER_YZX = 1,
    SCENE_ROT_ORDER_ZXY = 2,
    SCENE_ROT_ORDER_XZY = 3,
    SCENE_ROT_ORDER_YXZ = 4,
    SCENE_ROT_ORDER_XYZ = 5,
    SCENE_ROT_ORDER_COUNT = 6
} SceneNodeRotOrder;

typedef void (*RotMatrixBuilderFn)(SVECTOR *angles, MATRIX *matrix);

/* UpdateAndEnqueueFighterPose mode parameter. */
typedef enum FighterPoseMode {
    FIGHTER_POSE_MODE_ANGLES = 0,
    FIGHTER_POSE_MODE_PRECOMPUTED = 1,
    FIGHTER_POSE_MODE_OPENING_DEMO = 2,
    FIGHTER_POSE_MODE_MENU_SHOWCASE = 3,
    FIGHTER_POSE_MODE_CUTSCENE = 4
} FighterPoseMode;

/* SceneNode.param.attached.kind (+0x75) attached-part type dispatched by UpdateAttachedPartPool. */
typedef enum AttachedPartKind {
    ATTACHED_PART_MUZZLE_FLASH = 0
} AttachedPartKind;

#define SCENE_MESH_NONE     (-1) /* SceneNode.meshIndex: hidden/no mesh or shadow-list terminator */
#define FIGHTER_JOINT_COUNT 18   /* 18 skeleton joints following the root SceneNode (nodes 1..18) */

/* A transform and display-list node of the 3D scene (132 bytes). */
struct SceneNode {
    short renderType;
    SVECTOR rotation;
    MATRIX localMatrix;
    MATRIX worldMatrix;
    int unused0[4]; /* 16 bytes; nothing reads them */
    unsigned short matrixState;
    /* 2-byte alignment pad before parent */
    struct SceneNode *parent;
    short modelSlot;
    short meshIndex;
    int rotOrder;
    short unused1; /* nothing reads it */
    short lightSlot;
    short unused2; /* nothing reads it */
    unsigned short flags; /* +0x72: its one reader, a test in RenderSceneDisplayList, changes nothing */
    union {
        struct SceneNode *cloneSource;
        MATRIX **skinMatrices;
        int shadowFloorY;
        int propFrame;
        unsigned char queued;
        struct {
            unsigned char active;
            unsigned char kind;
        } attached;
    } param;
    union {
        int clothSlot;
        int fallHeading;
    } clothSlotOrHeading;
    union {
        char *clothWork; /* cloth point/link work buffer */
        short propFade;
    } clothWorkOrFade;
    struct ClothColliderSet *clothColliders;
};

/* The main camera's node. */
extern struct SceneNode gCameraNode;

extern struct SceneNode *gSecondaryCameraNodePtr;
extern struct SceneNode **gRenderQueueCursor;

#endif
