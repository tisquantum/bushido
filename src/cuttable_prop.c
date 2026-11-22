/* Cuttable stage props: 34 scene nodes standing over the stage's own props, which a cut
 * replaces with a stump while the severed top falls along keyframes and fades away. */
#include <sys/types.h>
#include <libgte.h>

#include "light.h"
#include "scene_node.h"
#include "angle.h"
#include "scene_math.h"
#include "cuttable_prop.h"
#include "rot_matrix.h"
#include "stage_cull.h"
#include "model_table.h"

#define CUTTABLE_PROP_COUNT           34
#define CUTTABLE_PROP_KEYFRAME_STRIDE 6    /* 6 shorts per frame: (dx, dy, dz, rx, ry, rz) */
#define CUTTABLE_PROP_FADE_MAX        4096 /* Q12 full opacity (1.0) passed to fading submitters */
#define CUTTABLE_PROP_FADE_STEP       64   /* Q12 fade decrement per frame (64 frames to vanish) */

/* Sub-model indices in the cuttable-prop model bank registered by InitCuttableProps. */
typedef enum CuttablePropMesh {
    CUTTABLE_PROP_MESH_FALLING = 0, /* severed upper prop during keyframe fall */
    CUTTABLE_PROP_MESH_STUMP = 1,   /* cut base replacing the static stage node */
    CUTTABLE_PROP_MESH_FADING = 2   /* fallen upper prop using fading polygon submitters */
} CuttablePropMesh;

/* Sentinel values for SceneNode.param.propFrame; >= 0 is the active keyframe index. */
typedef enum CuttablePropFrameState {
    CUTTABLE_PROP_FRAME_GONE = -2,  /* finished fading out; skipped until stage reload */
    CUTTABLE_PROP_FRAME_INTACT = -1 /* uncut; static stage node still standing */
} CuttablePropFrameState;

static struct SceneNode gCuttableProps[CUTTABLE_PROP_COUNT];

static short *sCuttablePropKeyframes;       /* fall keyframe table: short count; short keys[][6] */
static int sCuttablePropModelSlot;          /* model bank slot registered for cuttable prop meshes */
static struct SceneNode *sStageObjectNodes; /* stage SceneNode array from GetStageObjectsBase() */
static unsigned int *sCuttablePropMarkers;  /* stage prop markers (archive section 4); write-only here */
static int sCuttablePropUnreadFlag;         /* written, never read */

void InitCuttableProps(unsigned int *resource, int slot, unsigned int *propMarkers)
{
    int keyframeOffset, modelOffset;
    struct SceneNode *prop;
    int i;

    keyframeOffset = resource[2];
    modelOffset = resource[1];
    sCuttablePropModelSlot = slot;
    sCuttablePropKeyframes = (short *)(resource + (keyframeOffset >> 2));
    RegisterModelTable(resource + (modelOffset >> 2), slot);
    sStageObjectNodes = GetStageObjectsBase();

    prop = gCuttableProps;
    for (i = 0; i < CUTTABLE_PROP_COUNT; i++) {
        prop->renderType = SCENE_NODE_STANDARD;
        prop->meshIndex = CUTTABLE_PROP_MESH_FALLING;
        prop->modelSlot = slot;
        prop->rotOrder = SCENE_ROT_ORDER_FAST;
        prop->parent = 0;
        prop->lightSlot = LIGHT_SLOT_STAGE;
        prop->rotation.vx = 0;
        prop->rotation.vy = 0;
        prop->rotation.vz = 0;
        prop->localMatrix.t[0] = sStageObjectNodes[i].worldMatrix.t[0];
        prop->localMatrix.t[1] = sStageObjectNodes[i].worldMatrix.t[1];
        prop->localMatrix.t[2] = sStageObjectNodes[i].worldMatrix.t[2];
        prop->matrixState = SCENE_MATRIX_DIRTY;
        /* the original writes this field through a pointer */
        *(int *)&prop->param.propFrame = CUTTABLE_PROP_FRAME_INTACT;
        prop++;
    }

    sCuttablePropMarkers = propMarkers;
    sCuttablePropUnreadFlag = 0;
}

void TriggerCuttablePropFall(int propIndex, int heading)
{
    struct SceneNode *node;

    if (propIndex < CUTTABLE_PROP_COUNT) {
        node = &gCuttableProps[propIndex];
        if (node->param.propFrame == CUTTABLE_PROP_FRAME_INTACT) {
            /* the original writes these fields through a pointer */
            *(int *)&node->clothSlotOrHeading.fallHeading = heading;
            *(int *)&node->param.propFrame = 0;
            node = &sStageObjectNodes[propIndex];
            propIndex = CUTTABLE_PROP_MESH_STUMP; /* reused, as in the original */
            node->meshIndex = propIndex;
            node->matrixState = propIndex;
            node->modelSlot = sCuttablePropModelSlot;
            node->renderType = SCENE_NODE_STANDARD;
            node->parent = 0;
            node->rotation.vx = 0;
            node->rotation.vy = heading;
            node->rotation.vz = 0;
            gRotMatrixBuilders[node->rotOrder](&node->rotation, &node->worldMatrix);
        }
    }
}

void UpdateCuttableProps(void)
{
    int i;
    struct SceneNode *prop;
    int frame;
    int heading;
    int sinHeading, cosHeading;
    int cosZ, sinZ, cosX, sinX;
    SVECTOR step;
    short *key;
    int fade;

    for (i = 0; i < CUTTABLE_PROP_COUNT; i++) {
        prop = &gCuttableProps[i];
        frame = prop->param.propFrame;
        if (frame >= 0) {
            if (frame < *sCuttablePropKeyframes) {
                key = &sCuttablePropKeyframes[1 + frame * CUTTABLE_PROP_KEYFRAME_STRIDE];
                step.vx = *key++;
                step.vy = *key++;
                step.vz = *key++;
                heading = prop->clothSlotOrHeading.fallHeading;
                prop->rotation.vx = *key++;
                prop->rotation.vy = *key++;
                prop->rotation.vz = *key;
                prop->matrixState = SCENE_MATRIX_DIRTY;
                prop->rotation.vy += heading;
                UpdateNodeWorldMatrix(prop);

                cosHeading = gSinLutQ12[(heading + ANGLE_QUARTER) & ANGLE_MASK];
                cosZ = cosHeading * step.vz;
                sinHeading = gSinLutQ12[heading & ANGLE_MASK];
                sinZ = sinHeading * step.vz;
                cosX = cosHeading * step.vx;
                sinX = sinHeading * step.vx;
                prop->worldMatrix.t[0] += (sinZ + cosX) >> 12;
                prop->worldMatrix.t[1] += step.vy;
                prop->worldMatrix.t[2] += (cosZ - sinX) >> 12;
                ++frame;
                if (frame >= *sCuttablePropKeyframes) {
                    prop->meshIndex = CUTTABLE_PROP_MESH_FADING;
                    prop->clothWorkOrFade.propFade = CUTTABLE_PROP_FADE_MAX;
                }
                prop->param.propFrame = frame;
            } else {
                fade = prop->clothWorkOrFade.propFade - CUTTABLE_PROP_FADE_STEP;
                if (fade < 0) {
                    fade = 0;
                }
                if (fade == 0) {
                    prop->param.propFrame = CUTTABLE_PROP_FRAME_GONE;
                }
                prop->clothWorkOrFade.propFade = fade;
            }
            *gRenderQueueCursor++ = prop;
        }
    }
}

void RelocateCuttableProps(int delta)
{
    RelocateModelTable(sCuttablePropModelSlot, delta);
    sCuttablePropKeyframes += delta / 2;
}
