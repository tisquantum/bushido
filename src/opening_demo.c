/* The attract-mode opening: loads DATA3\O01.dat and runs its demo script (fades, overlays, the
 * fighter and camera scenes). Also a title menu that no routine of any build calls. */
#include <sys/types.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>

#include "packet_build.h"
#include "light.h"
#include "pose_angles.h"
#include "pose_stream.h"
#include "scene_node.h"
#include "display_buffer.h"
#include "gpu_code.h"
#include "model_arena.h"
#include "tim.h"
#include "sprite2d.h"
#include "camera_track.h"
#include "asset_load.h"
#include "ribbon_driver.h"
#include "scene_math.h"
#include "menu_state.h"
#include "fighter_pose.h"
#include "fighter_skeleton.h"
#include "stage_id.h"
#include "weapon_id.h"
#include "billboard_driver.h"
#include "display_list.h"
#include "fighter_cloth.h"
#include "fighter_cloth_setup.h"
#include "fog.h"
#include "light_init.h"
#include "main.h"
#include "main_menu_ui.h"
#include "opening_demo.h"
#include "opening_overlay.h"
#include "scene.h"
#include "screen_fade.h"
#include "stage_callbacks.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "stage_light.h"
#include "otz.h"
#include "globals.h"
#include "render.h"
#include "rot_matrix.h"

#define CD_FILE_OPENING_DEMO           0x52 /* DATA3\O01.dat */
#define OPENING_DEMO_VARIANT_FRAMES    900
#define OPENING_CAMERA_CHANNEL_FIGHTER 0
#define OPENING_CAMERA_CHANNEL_STAGE   1

#ifdef VERSION_EU
#define OPENING_DEMO_CARD_DELAY 102
#define OPENING_DEMO_END_DELAY  210
#else
#define OPENING_DEMO_CARD_DELAY 166
#define OPENING_DEMO_END_DELAY  300
#endif

/* Demo scripts: <delayFrames>, <opcode>, <arg> [, 0, <opcode>, <arg> ...], ended by opcode 5. */
short gOpeningDemoScriptRedShadow[102] = {
    0, 2, 0,
    190, 1, 2, 0, 4, 0,
    60, 2, 1,
    120, 2, 2,
    150, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 7, 1, 0, 2, 3,
    OPENING_DEMO_CARD_DELAY, 2, 4,
    OPENING_DEMO_CARD_DELAY, 2, 5,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 7, 0, 0, 1, 2, 0, 4, 0,
    300, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 7, 1, 0, 2, 6,
    OPENING_DEMO_CARD_DELAY, 2, 7,
    OPENING_DEMO_CARD_DELAY, 2, 8,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 7, 0, 0, 1, 2, 0, 4, 0,
    210, 1, 0,
    20, 4, 1, 0, 1, -1, 0, 2, 9,
    OPENING_DEMO_END_DELAY, 5, 0,
};

short gOpeningDemoScriptKannuki[90] = {
    0, 2, 0,
    190, 1, 2, 0, 4, 0,
    60, 2, 1,
    75, 2, 2,
    165, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 2, 3,
    OPENING_DEMO_CARD_DELAY, 2, 4,
    OPENING_DEMO_CARD_DELAY, 2, 5,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 1, 2, 0, 4, 0,
    260, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 2, 6,
    OPENING_DEMO_CARD_DELAY, 2, 7,
    OPENING_DEMO_CARD_DELAY, 2, 8,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 1, 2, 0, 4, 0,
    280, 1, 0,
    20, 4, 1, 0, 1, -1, 0, 2, 9,
    OPENING_DEMO_END_DELAY, 5, 0,
};

short gOpeningDemoScriptBlackLotus[90] = {
    0, 2, 0,
    190, 1, 2, 0, 4, 0,
    60, 2, 1,
    80, 2, 2,
    140, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 2, 3,
    OPENING_DEMO_CARD_DELAY, 2, 4,
    OPENING_DEMO_CARD_DELAY, 2, 5,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 1, 2, 0, 4, 0,
    280, 1, 3,
    20, 4, 1, 0, 6, 0, 0, 1, -1, 0, 2, 6,
    OPENING_DEMO_CARD_DELAY, 2, 7,
    OPENING_DEMO_CARD_DELAY, 2, 8,
    OPENING_DEMO_CARD_DELAY, 6, 1, 0, 1, 2, 0, 4, 0,
    280, 1, 0,
    20, 4, 1, 0, 1, -1, 0, 2, 9,
    OPENING_DEMO_END_DELAY, 5, 0,
};

short *gOpeningDemoScripts[OPENING_DEMO_VARIANT_COUNT] = {
    gOpeningDemoScriptRedShadow,
    gOpeningDemoScriptKannuki,
    gOpeningDemoScriptBlackLotus,
};

static int gOpeningDemoVariant;
static int gOpeningDemoPoseFrame;
static int gOpeningDemoPoseBaseFrame;
static int gOpeningDemoUnreadInitState; /* written, never read */
static char *gOpeningDemoPoseStream;
static void *gOpeningDemoFighterCameraTrack;
static void *gOpeningDemoStageCameraTrack;
static void *gOpeningDemoTimArchive;
static struct SceneNode *gOpeningDemoWeaponNode;
static int gOpeningDemoUnreadArchiveState; /* written, never read */
static int gOpeningDemoUnused;             /* 4 bytes; nothing reads or writes them */
static int gOpeningDemoUnreadState; /* written, never read */
static short *gOpeningDemoScriptStart;
static int gOpeningDemoScriptFrame;
static int gOpeningDemoFadeActive;
static int gOpeningDemoFighterSceneActive;
static int gOpeningDemoCameraSceneActive;
static short *gOpeningDemoScriptCursor;
static int gOpeningTitleMenuSelection;
static int gOpeningTitleUpArrowShade;
static int gOpeningTitleDownArrowShade;
static int gOpeningTitleScrollRemaining;
static int gOpeningTitleScrollDir;
static TexturedRectEntry *gOpeningTitleIncomingItem;
static TexturedRectEntry *gOpeningTitleOutgoingItem;

/* The offset table at the start of DATA3\O01.dat. */
typedef struct OpeningDemoArchiveHeader {
    int sectionCount;
    int poseStreamOffset;
    int fighterCameraOffsets[OPENING_DEMO_VARIANT_COUNT];
    int stageCameraOffset;
    int timArchiveOffset;
    int endOffset;
} OpeningDemoArchiveHeader;

int InitOpeningDemo(int variant, char *resource)
{
    char *base;
    OpeningDemoArchiveHeader *header;
    int poseStreamOffset, fighterCameraOffset, stageCameraOffset, timArchiveOffset, endOffset;
    char *poseStream, *fighterCameraTrack, *stageCameraTrack, *timArchive;
    int size;

    SetRendererMonochrome(GetMonochromeOption());
    base = resource;
    SetRendererDaytime(GetDaytimeOption());
    gOpeningDemoUnreadInitState = 0;
    variant %= OPENING_DEMO_VARIANT_COUNT;
    gOpeningDemoVariant = variant;
    ResetModelArena();
    CheckModelArenaCanary();
    ReadCdFileSync(CD_FILE_OPENING_DEMO, resource);
    CheckModelArenaCanary();
    gOpeningDemoPoseBaseFrame = variant * OPENING_DEMO_VARIANT_FRAMES;
    header = (OpeningDemoArchiveHeader *)resource;
    poseStreamOffset = header->poseStreamOffset;
    fighterCameraOffset = header->fighterCameraOffsets[variant];
    stageCameraOffset = header->stageCameraOffset;
    timArchiveOffset = header->timArchiveOffset;
    endOffset = header->endOffset;
    gOpeningDemoUnreadArchiveState = 0;
    poseStream = resource + poseStreamOffset;
    fighterCameraTrack = resource + fighterCameraOffset;
    stageCameraTrack = resource + stageCameraOffset;
    timArchive = resource + timArchiveOffset;
    gOpeningDemoPoseStream = poseStream;
    gOpeningDemoFighterCameraTrack = fighterCameraTrack;
    gOpeningDemoStageCameraTrack = stageCameraTrack;
    gOpeningDemoTimArchive = timArchive;
    resource += endOffset;
    CheckModelArenaCanary();
    resource = SetupOpeningDemoStage(variant, resource);
    CheckModelArenaCanary();
    InitOpeningOverlays();
    CheckModelArenaCanary();
    gOpeningDemoScriptStart = gOpeningDemoScripts[variant];
    ResetOpeningScriptState();
    InitCameraTrackReader(gOpeningDemoFighterCameraTrack, OPENING_CAMERA_CHANNEL_FIGHTER);
    InitCameraTrackReader(gOpeningDemoStageCameraTrack, OPENING_CAMERA_CHANNEL_STAGE);
    size = resource - base;
    gOpeningTitleMenuSelection = 0;
    return size;
}

int UpdateOpeningDemo(void)
{
    int running;
    short *nextCommand;

    gPacketCursor = GetGpuPacketBuffer(gFrameCounter & 1);
    gRenderQueueCursor = gRenderQueue;
    running = 1;
    nextCommand = StepOpeningDemoScript(gOpeningDemoScriptCursor, gOpeningDemoScriptFrame);
    if (nextCommand == gOpeningDemoScriptStart) {
        running = 0;
    } else {
        if (nextCommand != gOpeningDemoScriptCursor) {
            gOpeningDemoScriptFrame = 0;
            gOpeningDemoScriptCursor = nextCommand;
        }
        if (gOpeningDemoFadeActive != 0) {
            UpdateAndDrawScreenFade();
        }
        if (gOpeningDemoFighterSceneActive != 0) {
            UpdateOpeningDemoFighterScene();
        }
        if (gOpeningDemoCameraSceneActive != 0) {
            UpdateOpeningDemoCameraScene();
        }
        UpdateAndDrawOpeningOverlays();
        gOpeningDemoScriptFrame++;
        SubmitSceneDisplayList();
    }
    return running;
}

void ResetOpeningScriptState(void)
{
    short *start;

    start = gOpeningDemoScriptStart;
    gOpeningDemoFadeActive = 0;
    gOpeningDemoFighterSceneActive = 0;
    gOpeningDemoCameraSceneActive = 0;
    gOpeningDemoScriptFrame = 0;
    gOpeningDemoScriptCursor = start;
}

typedef enum OpeningScriptOp {
    OPENING_OP_NOP = 0,
    OPENING_OP_FADE = 1,
    OPENING_OP_OVERLAY = 2,
    OPENING_OP_LOAD_TIM = 3,
    OPENING_OP_FIGHTER_SCENE = 4,
    OPENING_OP_END = 5,
    OPENING_OP_CAMERA_SCENE = 6,
    OPENING_OP_CALL_NOP = 7
} OpeningScriptOp;

typedef enum OpeningSceneCommand {
    OPENING_SCENE_START = 0,
    OPENING_SCENE_STOP = 1,
    OPENING_SCENE_RESET_FRAME = 2
} OpeningSceneCommand;

#define OPENING_FADE_STOP (-1)

short *StepOpeningDemoScript(short *commands, int frame)
{
    int overlayIndex;
    int fadePreset;

    if (*commands != frame)
        return commands;
    commands++;
    do {
        switch (*commands++) {
        case OPENING_OP_NOP:
            break;
        case OPENING_OP_OVERLAY:
            overlayIndex = *commands++;
            switch (overlayIndex) {
            case 0:
                LoadTimGroup(gOpeningDemoTimArchive, 8);
                break;
            case 1:
                LoadTimGroup(gOpeningDemoTimArchive, 0);
                break;
            case 3:
                LoadTimGroup(gOpeningDemoTimArchive, 1);
                break;
            case 4:
                LoadTimGroup(gOpeningDemoTimArchive, 2);
                break;
            case 5:
                LoadTimGroup(gOpeningDemoTimArchive, 3);
                break;
            case 6:
                LoadTimGroup(gOpeningDemoTimArchive, 4);
                break;
            case 7:
                LoadTimGroup(gOpeningDemoTimArchive, 5);
                break;
            case 8:
                LoadTimGroup(gOpeningDemoTimArchive, 6);
                break;
            case 9:
                LoadTimGroup(gOpeningDemoTimArchive, 7);
                break;
            }
            TriggerOpeningOverlay(overlayIndex);
            DrawSync(0);
            break;
        case OPENING_OP_LOAD_TIM:
            LoadTimGroup(gOpeningDemoTimArchive, *commands++);
            DrawSync(0);
            break;
        case OPENING_OP_FADE:
            fadePreset = *commands;
            if (fadePreset == OPENING_FADE_STOP) {
                gOpeningDemoFadeActive = 0;
            } else {
                StartScreenFade(fadePreset);
                gOpeningDemoFadeActive = 1;
            }
            ++commands;
            break;
        case OPENING_OP_FIGHTER_SCENE:
            switch (*commands++) {
            case OPENING_SCENE_START:
                gOpeningDemoFighterSceneActive = 1;
                break;
            case OPENING_SCENE_STOP:
                gOpeningDemoFighterSceneActive = 0;
                break;
            case OPENING_SCENE_RESET_FRAME:
                gOpeningDemoPoseFrame = 0;
                break;
            }
            break;
        case OPENING_OP_CAMERA_SCENE:
            switch (*commands++) {
            case OPENING_SCENE_START:
                gOpeningDemoCameraSceneActive = 1;
                break;
            case OPENING_SCENE_STOP:
                gOpeningDemoCameraSceneActive = 0;
                break;
            case OPENING_SCENE_RESET_FRAME:
                gOpeningDemoPoseFrame = 0;
                break;
            }
            break;
        case OPENING_OP_CALL_NOP:
            OpeningDemoScriptNoOp(*commands++);
            break;
        case OPENING_OP_END:
            commands = gOpeningDemoScriptStart;
            gOpeningDemoScriptFrame = 0;
            gOpeningDemoPoseFrame = 0;
            return commands;
        }
    } while (*commands++ == 0);
    return commands - 1;
}

/* By variant (0 to 2): DATA6\a09.DAT, a07.DAT, a08.DAT; nothing reads the last two entries. */
int gOpeningDemoModels[5] = {
    CHAR_ARCHIVE_DEMO_RED_SHADOW, CHAR_ARCHIVE_DEMO_KANNUKI, CHAR_ARCHIVE_DEMO_BLACK_LOTUS, -1, 0
};

char *SetupOpeningDemoStage(int variant, char *nextSection)
{
    InitSceneCameras();
    InitLightSlots();
    DisableCharFogSlot();
    CheckModelArenaCanary();
    InitStageRenderEnvironment(STAGE_ID_OPENING_DEMO);
    ApplyStageLighting(STAGE_ID_OPENING_DEMO);
    BuildOpeningTintedCluts();
    CheckModelArenaCanary();
    InitClothSystem();
    SetFighterInjuryOverlayMask(0, 0);
    LoadSingleFighterSlot(0, gOpeningDemoModels[variant], WEAPON_ID_NONE, 0, 0, 0);
    CheckModelArenaCanary();
    InitPoseStream((unsigned long *)gOpeningDemoPoseStream);
    gCameraNode.localMatrix.t[0] = 0;
    gCameraNode.localMatrix.t[1] = 0;
    gCameraNode.localMatrix.t[2] = 0;
    gCameraNode.rotation.vx = 0;
    gCameraNode.rotation.vy = 0;
    gCameraNode.rotation.vz = 0;
    gOpeningDemoPoseFrame = 0;
    gOpeningDemoUnreadState = 0;
    LoadTimGroup(gOpeningDemoTimArchive, 0);
    InitRibbonTrail(0, 0);
    return nextSection;
}

#define OPENING_CAMERA_YAW_OFFSET      0x800 /* +180 deg in 4096-unit angle */
#define OPENING_CELL_LOD_START         14    /* central 4x4 grid cells around world origin (16, 16) */
#define OPENING_CELL_LOD_END           18
#define OPENING_CELL_LOD_LEVEL         2

void UpdateOpeningDemoFighterScene(void)
{
    /* Never set; the callee reads rootPos[2], past its end, and the pose then replaces what it copied. */
    PoseAngles rootRotation;
    long rootPos[2];
    PoseAngles poseAngles;
    SVECTOR hiltOffset, tipOffset;
    short *cameraKey;
    int column, row;

    cameraKey = DecodeCameraTrackFrame(OPENING_CAMERA_CHANNEL_FIGHTER);
    gCameraNode.localMatrix.t[0] = -cameraKey[0];
    gCameraNode.localMatrix.t[1] = cameraKey[1];
    gCameraNode.localMatrix.t[2] = -cameraKey[2];
    gCameraNode.rotation.vx = cameraKey[3];
    gCameraNode.rotation.vy = cameraKey[4] + OPENING_CAMERA_YAW_OFFSET;
    gCameraNode.rotation.vz = cameraKey[5];
    BuildNegatedNodeRotation(&gCameraNode);
    UpdateAspectCorrectedCameraMatrix();
    StampPackedCellGrid();
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P1]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
    RebuildLightDirectionMatrix(&gLightSlots[2]);
    UpdateAndEnqueueFighterPose(0, FIGHTER_POSE_MODE_OPENING_DEMO,
                                rootPos, &rootRotation, &poseAngles, 0);

    switch (gOpeningDemoVariant) {
    case 0:
        hiltOffset.vx = -25;
        hiltOffset.vy = 28;
        hiltOffset.vz = 72;
        tipOffset.vx = -25;
        tipOffset.vy = -42;
        tipOffset.vz = 996;
        break;
    case 1:
        hiltOffset.vx = -32;
        hiltOffset.vy = 92;
        hiltOffset.vz = 875;
        tipOffset.vx = -32;
        tipOffset.vy = 25;
        tipOffset.vz = 1320;
        break;
    case 2:
        hiltOffset.vx = -17;
        hiltOffset.vy = 91;
        hiltOffset.vz = 100;
        tipOffset.vx = -17;
        tipOffset.vy = 66;
        tipOffset.vz = 755;
        break;
    }

    UpdateRibbonTrailFromBone(0, gOpeningDemoWeaponNode, &hiltOffset, &tipOffset);
    RebuildFighterClothIfDirty(0);
    ResetClothWindWithJitter();
    UpdateAllFighterCloth();
    UpdateAndDrawBillboardEffects();
    for (row = OPENING_CELL_LOD_START; row < OPENING_CELL_LOD_END; row++) {
        for (column = OPENING_CELL_LOD_START; column < OPENING_CELL_LOD_END; column++) {
            SetPackedCellLod(column, row, OPENING_CELL_LOD_LEVEL);
        }
    }
    CullStageAndQueueFog(ComputeVectorLength3D(gCameraNode.localMatrix.t[0],
                              gCameraNode.localMatrix.t[1],
                              gCameraNode.localMatrix.t[2]));
    if (++gOpeningDemoPoseFrame >= OPENING_DEMO_VARIANT_FRAMES) {
        gOpeningDemoPoseFrame = 0;
        InitCameraTrackReader(gOpeningDemoFighterCameraTrack, OPENING_CAMERA_CHANNEL_FIGHTER);
        InitCameraTrackReader(gOpeningDemoStageCameraTrack, OPENING_CAMERA_CHANNEL_STAGE);
    }
}

void UpdateOpeningDemoCameraScene(void)
{
    short *cameraKey;

    cameraKey = DecodeCameraTrackFrame(OPENING_CAMERA_CHANNEL_STAGE);
    gCameraNode.localMatrix.t[0] = -cameraKey[0];
    gCameraNode.localMatrix.t[1] = cameraKey[1];
    gCameraNode.localMatrix.t[2] = -cameraKey[2];
    gCameraNode.rotation.vx = cameraKey[3];
    gCameraNode.rotation.vy = cameraKey[4] + OPENING_CAMERA_YAW_OFFSET;
    gCameraNode.rotation.vz = cameraKey[5];
    BuildNegatedNodeRotation(&gCameraNode);
    UpdateAspectCorrectedCameraMatrix();
    StampPackedCellGrid();
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P1]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
    RebuildLightDirectionMatrix(&gLightSlots[2]);
    UpdateAndDrawBillboardEffects();
    CullStageAndQueueFog(ComputeVectorLength3D(gCameraNode.localMatrix.t[0],
                              gCameraNode.localMatrix.t[1],
                              gCameraNode.localMatrix.t[2]));
}

int gOpeningDemoBoneOrder[17] = { 8, 9, 14, 15, 16, 17, 10, 11, 12, 13, 1, 2, 3, 4, 5, 6, 7 };

void ApplyOpeningDemoPose(struct SceneNode *parts)
{
    PoseAngles pose[FIGHTER_JOINT_COUNT];
    PoseAngles angles;
    PoseAngles *decodedPose;
    int heading, distance;
    int sineProduct, cosineProduct;
    int i;

    DecodePoseFrame(gOpeningDemoPoseFrame + gOpeningDemoPoseBaseFrame, pose, POSE_DECODE_WORK);
    decodedPose = pose;
    parts[0].rotation.vx = 0;
    parts[0].rotation.vy = 0;
    parts[0].rotation.vz = 0;
    parts[0].localMatrix.t[0] = 0;
    parts[0].localMatrix.t[1] = 0;
    parts[0].localMatrix.t[2] = 0;
    parts[0].matrixState = SCENE_MATRIX_DIRTY;

    /* Entry 0 of the decoded pose holds the root (height, heading, distance); 1..17 the joint angles. */
    heading = decodedPose->y;
    distance = decodedPose->z;
    parts[1].rotation.vx = 0;
    parts[1].rotation.vy = 0;
    parts[1].rotation.vz = 0;
    sineProduct = rsin(heading) * distance;
    parts[1].localMatrix.t[1] = -decodedPose->x;
    parts[1].localMatrix.t[0] = sineProduct >> 12;
    cosineProduct = rcos(heading) * distance;
    parts[1].matrixState = SCENE_MATRIX_DIRTY;
    parts[1].localMatrix.t[2] = -(cosineProduct >> 12);

    for (i = 1; i < FIGHTER_JOINT_COUNT; ++i) {
        angles.x = decodedPose[gOpeningDemoBoneOrder[i - 1]].x;
        angles.y = -decodedPose[gOpeningDemoBoneOrder[i - 1]].y;
        angles.z = -decodedPose[gOpeningDemoBoneOrder[i - 1]].z;
#ifdef VERSION_JP
        BuildRotMatrixZYX((SVECTOR *)&angles, &parts[i + 1].localMatrix);
#else
        BuildRotationMatrixFast((SVECTOR *)&angles, &parts[i + 1].localMatrix);
#endif
        parts[i + 1].matrixState = SCENE_MATRIX_LOCAL_PRECOMPUTED;
    }

    gOpeningDemoWeaponNode = &parts[FIGHTER_NODE_RIGHT_HAND];
}

/* Nothing calls it. */
static void OpeningDemoNoOp(void)
{
}

/* The opening's overlay textures: entries 0..29 are the six title banners' five frames each, 30..46
 * the 17 slash-cut and wide overlays (OPENING_OVERLAY_TEX_BASE). */
OverlayTex gOpeningOverlayTextures[47] = {
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 32 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 32, 256, 64 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 64, 256, 96 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 96, 256, 128 },
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 40 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 40, 256, 80 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 80, 256, 120 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 120, 256, 160 },
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 32 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 32, 256, 64 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 64, 256, 96 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 96, 256, 128 },
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 32 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 32, 256, 64 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 64, 256, 96 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 96, 256, 128 },
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 32 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 32, 256, 64 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 64, 256, 96 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 96, 256, 128 },
    { GET_TPAGE(1, 0, 832, 0), GET_CLUT(320, 480), 0, 0, 192, 144 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 0, 256, 32 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 32, 256, 64 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 64, 256, 96 },
    { GET_TPAGE(1, 0, 896, 0), GET_CLUT(320, 480), 64, 96, 256, 128 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(896, 20), 0, 0, 335, 15 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(896, 252), 0, 32, 255, 85 },
    { GET_TPAGE(0, 2, 896, 0), GET_CLUT(896, 252), 0, 32, 255, 85 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(928, 252), 0, 0, 255, 31 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(896, 196), 0, 160, 255, 191 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(912, 196), 0, 160, 255, 191 },
    { GET_TPAGE(0, 1, 960, 0), GET_CLUT(928, 196), 0, 160, 255, 191 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(944, 196), 0, 192, 255, 223 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(896, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(912, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 960, 0), GET_CLUT(928, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(944, 180), 0, 176, 255, 207 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(832, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(848, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 960, 0), GET_CLUT(896, 180), 0, 144, 255, 175 },
    { GET_TPAGE(0, 1, 832, 0), GET_CLUT(960, 252), 0, 0, 551, 143 },
    { GET_TPAGE(0, 1, 896, 0), GET_CLUT(1008, 244), 0, 224, 455, 239 },
};

void InitOpeningOverlays(void)
{
    ClearSlashOverlaySlots();
    InitOpeningOverlaySystem(gOpeningOverlayTextures);
}

void UpdateAndDrawOpeningOverlays(void)
{
    PacketBuilder build;
    char *cursor;
    int count;

    build.cursor = gPacketCursor;
    count = UpdateSlashOverlays(&build);
    gPacketCursor = build.cursor;
    SubmitOpeningOverlayPrims(count);

    cursor = gPacketCursor;
    count = UpdateOpeningBannerPackets(&cursor);
    gPacketCursor = cursor;
    SubmitOpeningOverlayPrims(count);
}

#define OPENING_OT_BACK_BUCKET (OTZ_BUCKET_COUNT - 1)

void SubmitOpeningOverlayPrims(int count)
{
    int i;
    int *prims;
    int *depths;
    int bucket;

    prims = gPacketCursor;
    depths = prims + count;
    for (i = 0; i < count; i++) {
        bucket = CompressDepthToOtz(depths[i] * 4, 0);
        if (bucket < OTZ_BUCKET_COUNT) {
            AddPrim(&gCurrentOt[bucket], (void *)prims[i]);
        }
    }
}

#define OPENING_TITLE_MENU_ITEM_COUNT 7
#define OPENING_TITLE_ITEM_HEIGHT     16
#define OPENING_TITLE_ITEM_Y          188
#define OPENING_TITLE_SCROLL_STEP     3
#define OPENING_TITLE_SCROLL_UP       (-1)
#define OPENING_TITLE_SCROLL_DOWN     1

/* Arrow flash brightness in Q4 fixed point (0x800 = neutral 0x80, 0xff0 = flash 0xff). */
#define OPENING_TITLE_ARROW_SHADE_MIN   0x800
#define OPENING_TITLE_ARROW_SHADE_MAX   0xff0
#define OPENING_TITLE_ARROW_SHADE_DECAY 192

/* Nothing calls it. */
static void InitOpeningTitleMenu(void)
{
    LoadTimGroup(gOpeningDemoTimArchive, 7);
    gOpeningTitleMenuSelection = 0;
    gOpeningTitleDownArrowShade = OPENING_TITLE_ARROW_SHADE_MIN;
    gOpeningTitleUpArrowShade = OPENING_TITLE_ARROW_SHADE_MIN;
    InitWhiteFullscreenBackdrop();
    gOpeningTitleScrollRemaining = 0;
}

TexturedRectEntry gOpeningTitleLogoSprites[3] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(960, 252), 255, 0,
        GET_TPAGE(0, 0, 832, 0), 0, 144, 255, 144 }, 1, 44, 20, 256, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(960, 252), 255, 0,
        GET_TPAGE(0, 0, 896, 0), 0, 144, 255, 144 }, 1, 300, 20, 256, 144 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 0, GET_CLUT(960, 252), 40, 0,
        GET_TPAGE(0, 0, 960, 0), 0, 144, 40, 144 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 556, 20, 40, 144 },
};

TexturedRectEntry gOpeningTitleArrowSprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 160, 224, GET_CLUT(976, 244), 192, 224,
        GET_TPAGE(0, 0, 832, 0), 160, 240, 192, 240 }, 1, 304, 171, 32, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 192, 224, GET_CLUT(992, 244), 224, 224,
        GET_TPAGE(0, 0, 832, 0), 192, 240, 224, 240 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 304, 208, 32, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem0Sprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 208, GET_CLUT(976, 248), 224, 208,
        GET_TPAGE(0, 0, 832, 0), 0, 224, 224, 224 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 208, 188, 224, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem1Sprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 224, GET_CLUT(992, 248), 160, 224,
        GET_TPAGE(0, 0, 832, 0), 0, 240, 160, 240 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 240, 188, 160, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem2Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 160, GET_CLUT(992, 252), 255, 160,
        GET_TPAGE(0, 0, 832, 0), 0, 176, 255, 176 }, 1, 144, 188, 255, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 160, GET_CLUT(992, 252), 96, 160,
        GET_TPAGE(0, 0, 896, 0), 0, 176, 96, 176 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 399, 188, 96, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem3Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 176, GET_CLUT(1008, 252), 255, 176,
        GET_TPAGE(0, 0, 832, 0), 0, 192, 255, 192 }, 1, 168, 188, 255, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 176, GET_CLUT(1008, 252), 48, 176,
        GET_TPAGE(0, 0, 896, 0), 0, 192, 48, 192 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 423, 188, 48, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem4Sprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 208, GET_CLUT(1008, 248), 184, 208,
        GET_TPAGE(0, 0, 896, 0), 0, 224, 184, 224 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 228, 188, 184, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem5Sprites[2] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(960, 248), 255, 192,
        GET_TPAGE(0, 0, 832, 0), 0, 208, 255, 208 }, 1, 176, 188, 255, 16 },
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 0, 192, GET_CLUT(960, 248), 32, 192,
        GET_TPAGE(0, 0, 896, 0), 0, 208, 32, 208 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 431, 188, 32, 16 },
};

TexturedRectEntry gOpeningTitleMenuItem6Sprite[1] = {
    { { 128, 128, 128, GPU_CODE_POLY_FT4, 32, 192, GET_CLUT(960, 244), 160, 192,
        GET_TPAGE(0, 0, 896, 0), 32, 208, 160, 208 }, (int)(TEXTURED_RECT_LAST_GROUP | 1), 256, 188, 128, 16 },
};

TexturedRectEntry *gOpeningTitleMenuSprites[OPENING_TITLE_MENU_ITEM_COUNT] = {
    gOpeningTitleMenuItem0Sprite,
    gOpeningTitleMenuItem1Sprite,
    gOpeningTitleMenuItem2Sprites,
    gOpeningTitleMenuItem3Sprites,
    gOpeningTitleMenuItem4Sprite,
    gOpeningTitleMenuItem5Sprites,
    gOpeningTitleMenuItem6Sprite,
};

/* Nothing calls it. */
static int UpdateAndDrawOpeningTitleMenu(int move)
{
    int oldIndex;
    int shade;

    shade = gOpeningTitleUpArrowShade - OPENING_TITLE_ARROW_SHADE_DECAY;
    if (shade < OPENING_TITLE_ARROW_SHADE_MIN) {
        shade = OPENING_TITLE_ARROW_SHADE_MIN;
    }
    gOpeningTitleUpArrowShade = shade;
    shade = gOpeningTitleDownArrowShade - OPENING_TITLE_ARROW_SHADE_DECAY;
    if (shade < OPENING_TITLE_ARROW_SHADE_MIN) {
        shade = OPENING_TITLE_ARROW_SHADE_MIN;
    }
    gOpeningTitleDownArrowShade = shade;

    if (gOpeningTitleScrollRemaining <= 0) {
        switch (move) {
        case OPENING_TITLE_SCROLL_DOWN:
            oldIndex = gOpeningTitleMenuSelection;
            if ((gOpeningTitleMenuSelection = oldIndex + 1) >= OPENING_TITLE_MENU_ITEM_COUNT) {
                gOpeningTitleMenuSelection = 0;
            }
            gOpeningTitleDownArrowShade = OPENING_TITLE_ARROW_SHADE_MAX;
            break;
        case OPENING_TITLE_SCROLL_UP:
            oldIndex = gOpeningTitleMenuSelection;
            if ((gOpeningTitleMenuSelection = oldIndex - 1) < 0) {
                gOpeningTitleMenuSelection = OPENING_TITLE_MENU_ITEM_COUNT - 1;
            }
            gOpeningTitleUpArrowShade = OPENING_TITLE_ARROW_SHADE_MAX;
            break;
        }
        if (move != 0) {
            gOpeningTitleScrollDir = move;
            gOpeningTitleScrollRemaining = OPENING_TITLE_ITEM_HEIGHT;
            gOpeningTitleOutgoingItem = gOpeningTitleMenuSprites[oldIndex];
            gOpeningTitleIncomingItem = gOpeningTitleMenuSprites[gOpeningTitleMenuSelection];
        }
    }
    if (gOpeningTitleScrollRemaining <= 0) {
        SetMenuSpriteListYAndHeight(gOpeningTitleMenuSprites[gOpeningTitleMenuSelection],
                                    OPENING_TITLE_ITEM_HEIGHT, OPENING_TITLE_ITEM_Y);
        DrawTexturedRectList(gOpeningTitleMenuSprites[gOpeningTitleMenuSelection], 0, 0, 0);
    } else {
        if (gOpeningTitleScrollDir == OPENING_TITLE_SCROLL_DOWN) {
            int remaining = gOpeningTitleScrollRemaining;
            SetMenuSpriteListYAndHeight(gOpeningTitleIncomingItem, OPENING_TITLE_ITEM_HEIGHT - remaining,
                                        OPENING_TITLE_ITEM_Y + remaining);
            SetMenuSpriteListYAndHeight(gOpeningTitleOutgoingItem, gOpeningTitleScrollRemaining, OPENING_TITLE_ITEM_Y);
        } else {
            int remaining = gOpeningTitleScrollRemaining;
            SetMenuSpriteListYAndHeight(gOpeningTitleIncomingItem, OPENING_TITLE_ITEM_HEIGHT - remaining,
                                        OPENING_TITLE_ITEM_Y);
            SetMenuSpriteListYAndHeight(gOpeningTitleOutgoingItem, gOpeningTitleScrollRemaining,
                                        OPENING_TITLE_ITEM_Y + OPENING_TITLE_ITEM_HEIGHT -
                                            gOpeningTitleScrollRemaining);
        }
        gOpeningTitleScrollRemaining -= OPENING_TITLE_SCROLL_STEP;
        DrawTexturedRectList(gOpeningTitleIncomingItem, 0, 0, 0);
        DrawTexturedRectList(gOpeningTitleOutgoingItem, 0, 0, 0);
    }
    gOpeningTitleArrowSprites[0].texture.r = gOpeningTitleArrowSprites[0].texture.g =
        gOpeningTitleArrowSprites[0].texture.b = (unsigned int)gOpeningTitleUpArrowShade >> 4;
    gOpeningTitleArrowSprites[1].texture.r = gOpeningTitleArrowSprites[1].texture.g =
        gOpeningTitleArrowSprites[1].texture.b = (unsigned int)gOpeningTitleDownArrowShade >> 4;
    DrawTexturedRectList(gOpeningTitleArrowSprites, 0, 0, 0);
    DrawTexturedRectList(gOpeningTitleLogoSprites, 0, 0, 0);
    SubmitWhiteFullscreenBackdrop();
    return gOpeningTitleMenuSelection;
}

static POLY_G4 gWhiteBackdropQuads[2];

void InitWhiteFullscreenBackdrop(void)
{
    int i = 0;
    do {
        POLY_G4 *quad = &gWhiteBackdropQuads[i];
        SetPolyG4(quad);
        quad->x0 = 0;
        quad->y0 = 0;
        quad->x1 = 639;
        quad->y1 = 0;
        quad->x2 = 0;
        quad->y2 = 240;
        quad->x3 = 639;
        quad->y3 = 240;
        quad->r0 = 255;
        quad->g0 = 255;
        quad->b0 = 255;
        quad->r1 = 255;
        quad->g1 = 255;
        quad->b1 = 255;
        quad->r2 = 255;
        quad->g2 = 255;
        quad->b2 = 255;
        quad->r3 = 255;
        quad->g3 = 255;
        quad->b3 = 255;
    } while (++i < 2);
}

void SubmitWhiteFullscreenBackdrop(void)
{
    AddPrim(gCurrentOt + OPENING_OT_BACK_BUCKET, &gWhiteBackdropQuads[gFrameCounter & 1]);
}

void BuildOpeningTintedCluts(void)
{
    int redGreenScale;
    int blueScale;
    int clutY;
    RECT clutRow;

    clutRow.y = 0x1e0;
    clutRow.w = 0x140;
    clutRow.x = 0;
    clutRow.h = 1;
    DrawSync(0);
    MoveImage(&clutRow, 0, 0x1e1);
    MoveImage(&clutRow, 0, 0x1e2);
    DrawSync(0);
    redGreenScale = 0xe00;
    blueScale = 0xb00;
    clutY = 0x1e2;
    TintVramClut(CLUT_TINT_MODE_LUMINANCE, 0, clutY, 0x100, redGreenScale, redGreenScale, blueScale, 0, clutY);
    TintVramClut(CLUT_TINT_MODE_LUMINANCE, 0x100, clutY, 0x40, redGreenScale, redGreenScale, blueScale, 0x100, clutY);
}

void OpeningDemoScriptNoOp(int operand)
{
    unsigned char unused[8]; /* not used */
}
