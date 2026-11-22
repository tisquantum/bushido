/* Story mode 3D cutscene subsystem (DATA7\V00..V08.DAT): archive setup, stage anchor
 * selection, per-frame camera/pose/face/subtitle playback, and cutscene fighter poses. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "world_vector.h"
#include "fighter_pose.h"
#include "light.h"
#include "fighter.h"
#include "model_arena.h"
#include "packet_build.h"
#include "scene_node.h"
#include "camera_track.h"
#include "asset_load.h"
#include "fighter_cloth.h"
#include "floor_collision.h"
#include "fighter_skeleton.h"
#include "stage_callbacks.h"
#include "effect.h"
#include "stage_env.h"
#include "tile_clip.h"
#include "fighter_cloth_setup.h"
#include "fighter_face.h"
#include "point_light.h"
#include "display_list.h"
#include "billboard_driver.h"
#include "stage_id.h"
#include "scene_math.h"
#include "subtitle.h"
#include "light_init.h"
#include "main.h"
#include "scene.h"
#include "stage_cull.h"
#include "stage_tables.h"
#include "story_cutscene.h"
#include "globals.h"
#include "render.h"
#include "rot_matrix.h"

#ifdef VERSION_JP
#define CD_FILE_CUTSCENE_BASE          127 /* DATA7\V00..V08.DAT */
#else
#define CD_FILE_CUTSCENE_BASE          128 /* DATA7\V00..V08.DAT */
#endif
#define CUTSCENE_SCENE_COUNT           10  /* 9 story cutscenes (V00..V08) + 1 unused sentinel row */
#define CUTSCENE_WEAPON_OVERRIDE_COUNT 4   /* nibble values 0..3 reload weapon model
                                              (value | WEAPON_CUTSCENE_OVERRIDE_BASE) */
#define CUTSCENE_CAMERA_CHANNEL        0
#define CUTSCENE_FLOOR_PROBE_HEIGHT    1000
#define CUTSCENE_CAMERA_YAW_OFFSET     0x800
#define CUTSCENE_FACE_NIBBLES_PER_WORD 8
#define CUTSCENE_FACE_NIBBLE_BITS      4
#define CUTSCENE_FACE_EXPR_MASK        0x7
#define CUTSCENE_FACE_NIBBLE_MASK      0xf
#define CUTSCENE_FACE_VOICE_CUE_SHIFT  3
#define CUTSCENE_MIDPOINT_WEIGHT       2048

typedef enum CutsceneElevationBand {
    CUTSCENE_BAND_ANY = 0,
    CUTSCENE_BAND_LOWER = 1, /* both fighters below split Y (larger Y in PS1 Y-down space) */
    CUTSCENE_BAND_UPPER = 2  /* both fighters at or above split Y (smaller Y in PS1 Y-down space) */
} CutsceneElevationBand;

typedef struct SceneNode SceneNode;

/* Offset-table header at the start of DATA7\V00..V08.DAT (one track/face entry per playable character). */
typedef struct CutsceneArchiveHeader {
    int sectionCount;
    int variantTableOffset;
    int trackOffsets[PLAYABLE_CHARACTER_COUNT * 2]; /* [variant * 2 + 0] = camera track, [+ 1] = pose data */
    int faceScriptOffsets[PLAYABLE_CHARACTER_COUNT];
    int subtitleBankOffset;
} CutsceneArchiveHeader;

static VECTOR gCutsceneAnchorPos;

/* Packed weapon-override nibbles per (scene * PLAYABLE_CHARACTER_COUNT + variant): high nibble = P1,
 * low nibble = P2 (0..3 overrides the fighter's weapon for the cutscene; 5 keeps the current weapon). */
unsigned char gCutsceneWeaponPairs[CUTSCENE_SCENE_COUNT * PLAYABLE_CHARACTER_COUNT] = {
    0x55, 0x55, 0x00, 0x05, 0x23, 0x00,
    0x55, 0x55, 0x50, 0x30, 0x25, 0x53,
    0x05, 0x00, 0x05, 0x55, 0x03, 0x50,
    0x55, 0x00, 0x50, 0x55, 0x55, 0x55,
    0x55, 0x53, 0x33, 0x00, 0x35, 0x55,
    0x23, 0x25, 0x55, 0x55, 0x53, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x05, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x03, 0x35, 0x25, 0x55,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static SceneVariant *gCutsceneVariant;
static FighterPose *gCutscenePoseData;
static void *gCutsceneCameraData;
static int *gCutsceneFaceScript;
static int gCutsceneFloorY;
static int gCutsceneFighterBaseFrames[2]; /* per-fighter starting pose frame (P1 = 0, P2 = frameCount) */
static int gCutsceneFrame;
static int gCutsceneFaceTicksLeft;        /* remaining 4-bit nibbles in the current 32-bit face-script words */
static int *gCutsceneP1FaceStream;
static int *gCutsceneP2FaceStream;
static unsigned int gCutsceneP1FaceBits;
static unsigned int gCutsceneP2FaceBits;
#ifndef VERSION_JP
static int *gCutsceneSubtitleBank;
#endif

void InitStoryCutscene(int scene, int variant, char *data)
{
    unsigned char unused[48]; /* not used */
    CutsceneArchiveHeader *header;
    int *variantWords;
    int frameCount;
    void *cameraTrackData;
    FighterPose *poseData;
    int faceScriptOffset;
#ifndef VERSION_JP
    int subtitleBankOffset;
#endif
    int floorY;
    int streamCount;
    unsigned char weaponPair;
    int p1WeaponOverride, p2WeaponOverride;

    ReadCdFileSync(scene + CD_FILE_CUTSCENE_BASE, data);
    header = (CutsceneArchiveHeader *)data;
    variantWords = (int *)((SceneVariant *)(data + header->variantTableOffset) + variant);
    gCutsceneVariant = (SceneVariant *)variantWords;
    if (*variantWords != 0) {
        const SceneVariant *variantEntry = (const SceneVariant *)variantWords;
        cameraTrackData = data + header->trackOffsets[variant * 2];
        poseData = (FighterPose *)(data + header->trackOffsets[variant * 2 + 1]);
        faceScriptOffset = header->faceScriptOffsets[variant];
#ifndef VERSION_JP
        subtitleBankOffset = header->subtitleBankOffset;
#endif
        gCutsceneFighterBaseFrames[0] = 0;
        gCutsceneCameraData = cameraTrackData;
        gCutscenePoseData = poseData;
        gCutsceneFaceScript = (int *)(data + faceScriptOffset);
        frameCount = variantEntry->frameCount;
#ifndef VERSION_JP
        gCutsceneSubtitleBank = (int *)(data + subtitleBankOffset);
#endif
        gCutsceneFighterBaseFrames[1] = frameCount;
        InitCameraTrackReader(cameraTrackData, CUTSCENE_CAMERA_CHANNEL);
#ifndef VERSION_JP
        RelocateSubtitleBank(gCutsceneSubtitleBank, SUBTITLE_BANK_CUTSCENE);
#endif
        weaponPair = gCutsceneWeaponPairs[scene * PLAYABLE_CHARACTER_COUNT + variant];
        p1WeaponOverride = weaponPair >> 4;
        p2WeaponOverride = weaponPair & 0xf;
        if (p1WeaponOverride < CUTSCENE_WEAPON_OVERRIDE_COUNT) {
            RemoveWeaponShadowPart(0);
            ReloadFighterWeaponModel(0, GetFighterRootNode(0) + 1, MODEL_SLOT_P1_WEAPON,
                                     p1WeaponOverride | WEAPON_CUTSCENE_OVERRIDE_BASE);
        }
        if (p2WeaponOverride < CUTSCENE_WEAPON_OVERRIDE_COUNT) {
            RemoveWeaponShadowPart(1);
            ReloadFighterWeaponModel(1, GetFighterRootNode(1) + 1, MODEL_SLOT_P2_WEAPON,
                                     p2WeaponOverride | WEAPON_CUTSCENE_OVERRIDE_BASE);
        }
        FreeAllFighterCloth();
        SetFighterClothDirty(0, 1);
        if (gCutsceneVariant->flags & SCENE_VARIANT_HAS_SECOND_FIGHTER) {
            SetFighterClothDirty(1, 1);
        } else {
            SetFighterClothDirty(1, 0);
        }
        SelectCutsceneAnchorPoint();
        floorY = QueryFloorHeight(gCutsceneAnchorPos.vx,
                                  gCutsceneAnchorPos.vy - CUTSCENE_FLOOR_PROBE_HEIGHT,
                                  gCutsceneAnchorPos.vz);
        gCutsceneFloorY = floorY;
        gCutsceneAnchorPos.vy = floorY;
        DrawSync(0);
        StopPointLightFlash();
        ClearEffectPool();
        gCameraNode.localMatrix.t[0] = 0;
        gCameraNode.localMatrix.t[1] = 0;
        gCameraNode.localMatrix.t[2] = 0;
        gCameraNode.rotation.vx = 0;
        gCameraNode.rotation.vy = 0;
        gCameraNode.rotation.vz = 0;
        gCutsceneFrame = 0;
        gCutsceneFaceTicksLeft = 0;
        /* Face script: [0] = stream count (1 or 2), [1..count] = word counts, followed by packed 4-bit streams. */
        streamCount = gCutsceneFaceScript[0];
        if (streamCount == 1) {
            gCutsceneP1FaceStream = &gCutsceneFaceScript[2];
            gCutsceneP2FaceStream = 0;
        } else {
            int *wordCounts = gCutsceneFaceScript + 1;
            gCutsceneP1FaceStream = &gCutsceneFaceScript[streamCount + 1];
            gCutsceneP2FaceStream = gCutsceneP1FaceStream + *wordCounts;
        }
#ifndef VERSION_JP
        StartSubtitleFromBank(SUBTITLE_BANK_CUTSCENE, variant);
#endif
    }
}

void SelectCutsceneAnchorPoint(void)
{
    VECTOR first, second, midpoint;
    VECTOR candidates[STAGE_SPAWN_PAIR_COUNT];
    SceneNode *fighterNode;
    int elevationBand;
    int stage;
    int *points;
    int i, firstEligible;
    int best, bestDistance;
    int distance, excluded;

    fighterNode = GetFighterRootNode(0);
    first.vx = fighterNode->localMatrix.t[0];
    first.vy = fighterNode->localMatrix.t[1];
    first.vz = fighterNode->localMatrix.t[2];
    fighterNode = GetFighterRootNode(1);
    elevationBand = CUTSCENE_BAND_ANY;
    second.vx = fighterNode->localMatrix.t[0];
    second.vy = fighterNode->localMatrix.t[1];
    second.vz = fighterNode->localMatrix.t[2];
    LoadAverage12(&first, &second, CUTSCENE_MIDPOINT_WEIGHT, CUTSCENE_MIDPOINT_WEIGHT, &midpoint);
    stage = GetLoadedStageId();
    switch (stage) {
    case 4:
        if (first.vy > 4000) {
            if (second.vy > 4000) elevationBand = CUTSCENE_BAND_LOWER;
        } else {
            if (second.vy <= 4000) elevationBand = CUTSCENE_BAND_UPPER;
        }
        break;
    case 10:
        if (first.vy > 0) {
            if (second.vy > 0) elevationBand = CUTSCENE_BAND_LOWER;
        } else {
            if (second.vy <= 0) elevationBand = CUTSCENE_BAND_UPPER;
        }
        break;
    case STAGE_ID_UNDERGROUND_GROTTO:
        if (first.vy > 500) {
            if (second.vy > 500) elevationBand = CUTSCENE_BAND_LOWER;
        } else {
            if (second.vy <= 500) elevationBand = CUTSCENE_BAND_UPPER;
        }
        break;
    }
    firstEligible = 1;
    points = (int *)(GetStageSpawnTable() + stage * STAGE_SPAWN_PAIR_COUNT * 2);
    i = 0;
    do {
        first.vx = *points++;
        first.vy = *points++;
        first.vz = *points++;
        second.vx = *points++;
        second.vy = *points++;
        second.vz = *points++;
        LoadAverage12(&first, &second, CUTSCENE_MIDPOINT_WEIGHT, CUTSCENE_MIDPOINT_WEIGHT, &candidates[i]);
        distance = ComputeVectorLength3D(candidates[i].vx - midpoint.vx,
                                         candidates[i].vy - midpoint.vy,
                                         candidates[i].vz - midpoint.vz);
        excluded = 0;
        switch (stage) {
        case 4:
            if (elevationBand == CUTSCENE_BAND_LOWER && candidates[i].vy <= 4000) excluded = 1;
            if (elevationBand == CUTSCENE_BAND_UPPER && candidates[i].vy > 4000) excluded = 1;
            break;
        case 10:
            if (elevationBand == CUTSCENE_BAND_LOWER && candidates[i].vy <= 0) excluded = 1;
            if (elevationBand == CUTSCENE_BAND_UPPER && candidates[i].vy > 0) excluded = 1;
            break;
        case STAGE_ID_UNDERGROUND_GROTTO:
            if (elevationBand == CUTSCENE_BAND_LOWER && candidates[i].vy <= 500) excluded = 1;
            if (elevationBand == CUTSCENE_BAND_UPPER && candidates[i].vy > 500) excluded = 1;
            break;
        }
        if (!excluded) {
            if (firstEligible) {
                bestDistance = distance;
                best = i;
                firstEligible = 0;
            } else if (distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
    } while (++i < STAGE_SPAWN_PAIR_COUNT);
    if (firstEligible) {
        best = 0;
    }
    gCutsceneAnchorPos = candidates[best];
}

int UpdateStoryCutscene(void)
{
    short *cameraKey;
    int frameLimit;
    int nextFrame;
    unsigned int p1FaceCommand;
    /* Never set; the callee reads rootPos[2], past its end, and the pose then replaces what it copied. */
    PoseAngles rootRotation;
    long rootPos[2];
    PoseAngles poseAngles;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    gRenderQueueCursor = gRenderQueue;
    gSceneFrameCounter++;
    cameraKey = DecodeCameraTrackFrame(CUTSCENE_CAMERA_CHANNEL);
    gCameraNode.localMatrix.t[0] = gCutsceneAnchorPos.vx - cameraKey[0];
    gCameraNode.localMatrix.t[1] = cameraKey[1] + gCutsceneAnchorPos.vy;
    gCameraNode.localMatrix.t[2] = gCutsceneAnchorPos.vz - cameraKey[2];
    gCameraNode.rotation.vx = cameraKey[3];
    gCameraNode.rotation.vy = cameraKey[4] + CUTSCENE_CAMERA_YAW_OFFSET;
    gCameraNode.rotation.vz = cameraKey[5];
    BuildNegatedNodeRotation(&gCameraNode);
    UpdateAspectCorrectedCameraMatrix();
    StampPackedCellGrid();
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P1]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
    RebuildLightDirectionMatrix(&gLightSlots[2]);
    UpdateAndEnqueueFighterPose(0, FIGHTER_POSE_MODE_CUTSCENE,
                                rootPos, &rootRotation, &poseAngles, gCutsceneFloorY);
    if (gCutsceneVariant->flags & SCENE_VARIANT_HAS_SECOND_FIGHTER) {
        UpdateAndEnqueueFighterPose(1, FIGHTER_POSE_MODE_CUTSCENE,
                                    rootPos, &rootRotation, &poseAngles, gCutsceneFloorY);
    }
    RebuildFighterClothIfDirty(0);
    if (gCutsceneVariant->flags & SCENE_VARIANT_HAS_SECOND_FIGHTER) {
        RebuildFighterClothIfDirty(1);
    }
    ResetClothWindWithJitter();
    UpdateAllFighterCloth();
    UpdateAndDrawBillboardEffects();
    UpdateAndDrawEffects();
    CullStageAndQueueFog(ComputeVectorLength3D(-cameraKey[0], cameraKey[1], -cameraKey[2]));
    frameLimit = gCutsceneVariant->frameCount;
    nextFrame = gCutsceneFrame + 1;
    if (nextFrame < frameLimit) {
        frameLimit = nextFrame;
    }
    gCutsceneFrame = frameLimit;
    DisableStageTileNearClip();
    SubmitSceneDisplayList();
    if (gCutsceneFaceTicksLeft == 0) {
        gCutsceneP1FaceBits = *gCutsceneP1FaceStream++;
        if (gCutsceneP2FaceStream != 0) {
            gCutsceneP2FaceBits = *gCutsceneP2FaceStream++;
        }
        gCutsceneFaceTicksLeft = CUTSCENE_FACE_NIBBLES_PER_WORD;
    }
    SetFighterFacialExpression(0, GetFighterCharacterId(0), gCutsceneP1FaceBits & CUTSCENE_FACE_EXPR_MASK);
    p1FaceCommand = gCutsceneP1FaceBits & CUTSCENE_FACE_NIBBLE_MASK;
    gCutsceneP1FaceBits >>= CUTSCENE_FACE_NIBBLE_BITS;
    if (gCutsceneP2FaceStream != 0) {
        SetFighterFacialExpression(1, GetFighterCharacterId(1), gCutsceneP2FaceBits & CUTSCENE_FACE_EXPR_MASK);
        gCutsceneP2FaceBits >>= CUTSCENE_FACE_NIBBLE_BITS;
    }
    gCutsceneFaceTicksLeft--;
#ifndef VERSION_JP
    UpdateAndDrawSubtitle(0);
#endif
    p1FaceCommand >>= CUTSCENE_FACE_VOICE_CUE_SHIFT;
    if (gCutsceneFrame == gCutsceneVariant->frameCount) {
        gCutsceneFrame--;
        return STORY_CUTSCENE_FINISHED;
    }
    return p1FaceCommand;
}

int gCutsceneBoneOrder[17] = { 8, 9, 14, 15, 16, 17, 10, 11, 12, 13, 1, 2, 3, 4, 5, 6, 7 };

void ApplyCutsceneFighterPose(int side, SceneNode *base)
{
    FighterPose pose; /* not used */
    SVECTOR jointAngles;
    PoseAngles *frame;
    int baseFrame;
    int yaw, radius, cosineProduct;
    int i;

    baseFrame = gCutsceneFighterBaseFrames[side];
    base->rotation.vx = 0;
    base->rotation.vy = 0;
    base->rotation.vz = 0;
    base->localMatrix.t[0] = gCutsceneAnchorPos.vx;
    base->localMatrix.t[1] = gCutsceneAnchorPos.vy;
    base->localMatrix.t[2] = gCutsceneAnchorPos.vz;
    base->matrixState = SCENE_MATRIX_DIRTY;

    /* Entry 0 stores root polar position (height, yaw, radius); entries 1..17 store joint Euler angles. */
    frame = gCutscenePoseData[gCutsceneFrame + baseFrame].joint;
    yaw = frame->y;
    radius = frame->z;
    base[1].rotation.vx = 0;
    base[1].rotation.vy = 0;
    base[1].rotation.vz = 0;
    base[1].localMatrix.t[0] = (rsin(yaw) * radius) >> 12;
    base[1].localMatrix.t[1] = -frame->x;
    cosineProduct = rcos(yaw) * radius;
    base[1].matrixState = SCENE_MATRIX_DIRTY;
    base[1].localMatrix.t[2] = -(cosineProduct >> 12);

    for (i = 1; i < FIGHTER_JOINT_COUNT; ++i) {
        jointAngles.vx = frame[gCutsceneBoneOrder[i - 1]].x;
        jointAngles.vy = -frame[gCutsceneBoneOrder[i - 1]].y;
        jointAngles.vz = -frame[gCutsceneBoneOrder[i - 1]].z;
#ifdef VERSION_JP
        BuildRotMatrixZYX(&jointAngles, &base[i + 1].localMatrix);
#else
        BuildRotationMatrixFast(&jointAngles, &base[i + 1].localMatrix);
#endif
        base[i + 1].matrixState = SCENE_MATRIX_LOCAL_PRECOMPUTED;
    }
}
