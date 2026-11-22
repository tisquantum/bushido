/* Stage callbacks: the per-stage init, post-cull and frame-begin routines gStageCallbacks names
 * (snow, rain and ground decals, Stage 9's cloth strips, Stage 11's props and red light,
 * Stage 12's shoreline, Stage 19's rain and foliage, water spray), and the helpers they share. */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include "fighter_skeleton.h"
#include "otz.h"
#include "scene.h"

#include "packet_build.h"
#include "weather.h"
#include "ocean.h"
#include "decal_driver.h"
#include "water_anim.h"
#include "scene_node.h"
#include "light.h"
#include "world_vector.h"
#include "fighter_hit_sphere.h"
#include "effect_slot.h"
#include "model_arena.h"
#include "foliage.h"
#include "cloth_strip.h"
#include "stage_cull.h"
#include "stage_callbacks.h"
#include "display_buffer.h"
#include "cuttable_prop.h"
#include "effect.h"
#include "fighter_pose.h"
#include "light_init.h"
#include "main.h"
#include "rot_matrix.h"
#include "scene_math.h"
#include "stage_light.h"
#include "stage_moat.h"
#include "tim.h"
#include "globals.h"

/* The cloth texture in VRAM: image position and size, CLUT position. */
typedef struct {
    short x, y;
    short w, h;
    short clutX, clutY;
} ImageRect;

#define WEATHER_SCRATCH         ((WeatherScratch *)getScratchAddr(8))
#define STAGE_CLOTH_SCRATCH     ((StageClothRenderContext *)getScratchAddr(8))
#define SWAYING_FOLIAGE_SCRATCH ((SwayingFoliageScratch *)getScratchAddr(8))

#define STAGE9_PROP_COUNT         15
#define STAGE11_PROP_COUNT        6
#define YAGURA_POINT_PROP_COUNT   10
#define STAGE11_LIGHT_PITCH       0xe42 /* 4096-unit pitch (-446, ~-39.2 deg) */
#define MONOCHROME_RED_LUMA_Q12   1351  /* 0x547 / 4096 (~0.330), matches ConvertRgbToLuminance */

SwayingFoliageBody gMoatFoliageStorage;
unsigned char gRainWorkStorage[WEATHER_WORK_BYTES];

static void SetupSnowBuffers(void);
static int ReturnTrueRainSplashEnabled(VECTOR *groundPos);
static void InitStagePropNode(struct SceneNode *node);

static MATRIX gAspectCorrectedCameraMatrix;
/* Prop nodes: six on stage 11, one at night on stage 12, ten at night on stage 17;
 * stage 9 sets up fifteen that nothing queues. */
static struct SceneNode gStagePropNodes[20];
static char gSnowWorkStorage[WEATHER_WORK_BYTES];
ImageRect gStage9ClothImage = { 864, 320, 64, 64, 288, 480 };
/* 184 bytes; nothing reads or writes them. */
int gUnusedGroundPath[1 + 15 * 3] = {
    0,
    44800, -600, -22400,
    44800, -600, -32000,
    38400, -600, -32000,
    32000, -600, -32000,
    25600, -600, -32000,
    19200, -600, -32000,
    12800, -600, -32000,
    6400, -600, -32000,
    0, -600, -32000,
    -6400, -600, -32000,
    -12800, -600, -32000,
    -19200, -600, -32000,
    -25600, -600, -25600,
    -32000, -600, -22400,
    -32000, -600, -16000,
};

WorldVector gStage9ClothOrigins[STAGE_CLOTH_STRIP_COUNT] = {
    { 10800, -5600, -2000 },
    { 10800, -5600, -5850 },
    { 10800, -5600, -9700 },
};

int gStage11PropPositions[STAGE11_PROP_COUNT][3] = {
    { -33270, 287,   802 },
    { -24356, 287, 16473 },
    {  -8915, 287, 25521 },
    {   8915, 287, 25521 },
    {  24356, 287, 16473 },
    {  33271, 287,   802 },
};

/* 4096-unit sine phase of stage 11's pulsing red light. */
unsigned short gStage11LightPhase = 0;

CVECTOR gRainColorTop;

int gYaguraPointPropPositions[YAGURA_POINT_PROP_COUNT][3] = {
    { 38400, -600, -44800 },
    { 32000, -600, -44800 },
    { 25600, -600, -44800 },
    { 19200, -600, -44800 },
    { 12800, -600, -44800 },
    { 6400, -600, -44800 },
    { 0, -600, -44800 },
    { -6400, -600, -44800 },
    { -12800, -600, -44800 },
    { -52800, -7671, 0 },
};

void UpdateAspectCorrectedCameraMatrix(void)
{
    gAspectCorrectedCameraMatrix = gCameraNode.worldMatrix;
    gAspectCorrectedCameraMatrix.m[1][0] >>= 1;
    gAspectCorrectedCameraMatrix.m[1][1] >>= 1;
    gAspectCorrectedCameraMatrix.m[1][2] >>= 1;
}

MATRIX *GetAspectCorrectedCameraMatrix(void)
{
    return &gAspectCorrectedCameraMatrix;
}

void InsertDeferredPrimsIntoOt(int count)
{
    int i;
    void **prims;
    int *keys;
    int bucket;

    prims = gPacketCursor;
    keys = (int *)(prims + count);
    for (i = 0; i < count; ++i) {
        bucket = CompressDepthToOtz(keys[i] * 4, 0);
        if (bucket < 8192)
            AddPrim(&gCurrentOt[bucket], prims[i]);
    }
}

void InitStageSnowIfEnabled(void)
{
    int stageId;

    stageId = GetLoadedStageId();
    switch (stageId) {
    case 0: case 1: case 3: case 4: case 5: case 10:
        SetupSnowBuffers();
        break;
    }
}

static void SetupSnowBuffers(void)
{
    SnowInit init;

    init.tpage = GetTPage(0, 1, 0x280, 0x100);
    init.clut = GetClut(0x130, 0x1e0);
    init.u = 0;
    init.v = 0;
    init.storage = gSnowWorkStorage;
    InitSnowSystem(&init);
}

static void UpdateStageSnow(int density)
{
    MATRIX view = gAspectCorrectedCameraMatrix; /* never read */
    int count;

    WEATHER_SCRATCH->density = density;
    WEATHER_SCRATCH->cameraHeading = (0x800 - GetCameraHeadingAngle()) & 0xfff;

    WEATHER_SCRATCH->viewMatrix = gAspectCorrectedCameraMatrix;
    WEATHER_SCRATCH->cameraPos.vx = gAspectCorrectedCameraMatrix.t[0];
    WEATHER_SCRATCH->cameraPos.vy = gAspectCorrectedCameraMatrix.t[1];
    WEATHER_SCRATCH->cameraPos.vz = gAspectCorrectedCameraMatrix.t[2];
    WEATHER_SCRATCH->packetCursor = gPacketCursor;

    count = UpdateAndDrawSnow(WEATHER_SCRATCH);
    gPacketCursor = WEATHER_SCRATCH->packetCursor;
    InsertDeferredPrimsIntoOt(count);
}

void InitStageSnowAndDecals(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
}

void UpdateStageSnowAndDecals(void)
{
    UpdateStageSnow(0x40);
    UpdateAndDrawStageGroundDecals();
}

void InitStage1Waterfall(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
    ResetWaterScrollOffset();
}

void UpdateStage1Waterfall(void)
{
    VECTOR direction;
    VECTOR position;

    UpdateStageSnow(0x20);
    UpdateAndDrawStageGroundDecals();
    UpdateWaterTextureScroll(1);
    UpdateStage1WaterfallSplashes();
    QueueAnimatedVramTextures(0, 0);
    QueueAnimatedVramTextures(1, 1);
    if ((gSceneFrameCounter & 3) == 0) {
        direction.vx = 0;
        direction.vy = -4096;
        direction.vz = 0;
        position.vx = -12100;
        position.vy = 2000;
        position.vz = 8150;
        SpawnDirectionalSprayUnit(SPRAY_KIND_WATERFALL, &position, &direction);
        position.vx = -12100;
        position.vy = 2000;
        position.vz = 7850;
        SpawnDirectionalSprayUnit(SPRAY_KIND_WATERFALL, &position, &direction);
    }
}

void InitStage3SnowAndDecals(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
}

void UpdateStage3PropsAndSnow(void)
{
    UpdateCuttableProps();
    UpdateStageSnow(4);
    UpdateAndDrawStageGroundDecals();
}

void InitStage4SnowDecalsAndGlows(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
    SpawnStage4LightGlows();
}

void UpdateStage4SnowAndDecals(void)
{
    UpdateStageSnow(0x40);
    UpdateAndDrawStageGroundDecals();
}

void InitStage5SnowAndDecals(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
}

void UpdateStage5SnowAndDecals(void)
{
    UpdateStageSnow(0x40);
    UpdateAndDrawStageGroundDecals();
}

void InitStage9PropsAndCloth(void)
{
    StageClothConfig config;
    struct SceneNode *node = gStagePropNodes;
    int i;

    for (i = 0; i < STAGE9_PROP_COUNT; i++) {
        InitStagePropNode(node);
        node->meshIndex = i;
        node++->renderType = SCENE_NODE_WORLD;
    }

    config.tpage = GetTPage(0, 0, gStage9ClothImage.x, gStage9ClothImage.y);
    config.clut = GetClut(gStage9ClothImage.clutX, gStage9ClothImage.clutY);

    config.u0 = (gStage9ClothImage.x % 64) * 4;
    config.v0 = gStage9ClothImage.y % 256;
    config.u1 = config.u0 + gStage9ClothImage.w - 1;
    config.v1 = config.v0 + gStage9ClothImage.h - 1;

    for (i = 0; i < STAGE_CLOTH_STRIP_COUNT; i++) {
        config.origins[i].vx = gStage9ClothOrigins[i].x;
        config.origins[i].vy = gStage9ClothOrigins[i].y;
        config.origins[i].vz = gStage9ClothOrigins[i].z;
    }

    InitStageClothStrip(&config);
}

void UpdateStage9ClothStrips(void)
{
    ApplyLightSlotToGte(LIGHT_SLOT_STAGE, ONE);
    STAGE_CLOTH_SCRATCH->yaw = (0x800 - GetCameraHeadingAngle()) & 0xfff;

    STAGE_CLOTH_SCRATCH->matrix = gAspectCorrectedCameraMatrix;
    STAGE_CLOTH_SCRATCH->eye.vx = gAspectCorrectedCameraMatrix.t[0];
    STAGE_CLOTH_SCRATCH->eye.vy = gAspectCorrectedCameraMatrix.t[1];
    STAGE_CLOTH_SCRATCH->eye.vz = gAspectCorrectedCameraMatrix.t[2];
    STAGE_CLOTH_SCRATCH->packet = gPacketCursor;

    UpdateAndDrawStageClothStrips(STAGE_CLOTH_SCRATCH);
    gPacketCursor = STAGE_CLOTH_SCRATCH->packet;
}

void InitStage10SnowAndDecals(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
}

void UpdateStage10SnowAndVramAnim(void)
{
    UpdateStageSnow(0x10);
    UpdateAndDrawStageGroundDecals();
    QueueAnimatedVramTextures(2, 0);
}

void InitStage11Props(void)
{
    struct SceneNode *node;
    int i;

    node = gStagePropNodes;
    for (i = 0; i < STAGE11_PROP_COUNT; i++) {
        InitStagePropNode(node);
        node->meshIndex = i;
        node->renderType = SCENE_NODE_WORLD;
        node++;
    }
}

void UpdateStage11PropsAndLight(void)
{
    int i;
    int slotIndex;
    int phase;
    int intensity;
    int isMonochrome;
    struct SceneNode *node;
    int *position;
    LightSlot *slot;

    node = gStagePropNodes;
    position = gStage11PropPositions[0];
    for (i = 0; i < STAGE11_PROP_COUNT; i++) {
        node->worldMatrix.t[0] = *position++;
        node->worldMatrix.t[1] = *position++;
        node->worldMatrix.t[2] = *position++;
        *gRenderQueueCursor++ = node++;
    }

    phase = gStage11LightPhase + 16;
    intensity = gSinLutQ12[phase & 0xfff];
    gStage11LightPhase = phase;
    if (intensity < 0)
        intensity = -intensity;

    isMonochrome = IsRendererMonochrome();
    if (isMonochrome)
        intensity = (intensity * MONOCHROME_RED_LUMA_Q12) >> 12;

    for (slotIndex = 0; slotIndex < 3; slotIndex++) {
        switch (slotIndex) {
        case 0:
            slot = &gLightSlots[LIGHT_SLOT_P1];
            break;
        case 1:
            slot = &gLightSlots[LIGHT_SLOT_P2];
            break;
        case 2:
            slot = &gLightSlots[LIGHT_SLOT_STAGE];
            break;
        }

        slot->directions[LIGHT_DIR_PRIMARY].pitch = STAGE11_LIGHT_PITCH;
        slot->directions[LIGHT_DIR_PRIMARY].yaw = 0;
        slot->directions[LIGHT_DIR_PRIMARY].active = 1;
        if (isMonochrome) {
            slot->colorMatrix.m[0][LIGHT_DIR_PRIMARY] = intensity;
            slot->colorMatrix.m[1][LIGHT_DIR_PRIMARY] = intensity;
            slot->colorMatrix.m[2][LIGHT_DIR_PRIMARY] = intensity;
        } else {
            slot->colorMatrix.m[0][LIGHT_DIR_PRIMARY] = intensity;
            slot->colorMatrix.m[1][LIGHT_DIR_PRIMARY] = 0;
            slot->colorMatrix.m[2][LIGHT_DIR_PRIMARY] = 0;
        }
        RebuildLightDirectionMatrix(slot);
    }
}

void InitCoveShoreline(void)
{
    struct SceneNode *node;

    node = gStagePropNodes;
    InitStagePropNode(node);
    node->meshIndex = 0;
    node->renderType = SCENE_NODE_WORLD;
    node->worldMatrix.t[0] = 0;
    node->worldMatrix.t[1] = 0;
    node->worldMatrix.t[2] = 0;
    InitOceanWaveMesh();
    InitStageGroundDecals();
}

void UpdateCoveShoreline(void)
{
    int side;
    MATRIX **joints;
    VECTOR direction;
    VECTOR position;
    int scale;
    int boundary;

    if (IsRendererDaytime() == 0) {
        struct SceneNode *node = gStagePropNodes;
        node->worldMatrix.t[0] = -50539;
        node->worldMatrix.t[1] = -1600;
        node->worldMatrix.t[2] = 0;
        *gRenderQueueCursor++ = node;
    }
    UpdateOceanWaveMesh();
    UpdateAndDrawStageGroundDecals();
    if ((gSceneFrameCounter & 3) == 0) {
        direction.vx = 4096;
        direction.vy = -1024;
        direction.vz = 0;
        for (side = 0; side < 2; side++) {
            joints = GetFighterWorldMatrixList(side);
            boundary = SampleOceanWaveEdgeX(joints[FIGHTER_JOINT_RIGHT_FOOT]->t[2]);
            if (joints[FIGHTER_JOINT_RIGHT_FOOT]->t[0] < boundary) {
                scale = ((SampleOceanWaveVelocity() + 16) << 12) / 88;
                if (scale < -16) {
                    scale = 0;
                }
                position.vx = joints[FIGHTER_JOINT_RIGHT_FOOT]->t[0] - 120;
                position.vz = joints[FIGHTER_JOINT_RIGHT_FOOT]->t[2];
                position.vy = 150;
                SpawnDirectionalSpray(SPRAY_KIND_SHORELINE, &position, &direction, scale);
            }
            boundary = SampleOceanWaveEdgeX(joints[FIGHTER_JOINT_LEFT_FOOT]->t[2]);
            if (joints[FIGHTER_JOINT_LEFT_FOOT]->t[0] < boundary) {
                scale = ((SampleOceanWaveVelocity() + 16) << 12) / 88;
                if (scale < -16) {
                    scale = 0;
                }
                position.vx = joints[FIGHTER_JOINT_LEFT_FOOT]->t[0] - 120;
                position.vz = joints[FIGHTER_JOINT_LEFT_FOOT]->t[2];
                position.vy = 150;
                SpawnDirectionalSpray(SPRAY_KIND_SHORELINE, &position, &direction, scale);
            }
        }
    }
}

void InitBambooThicketNoOp(void)
{
}

void UpdateBambooThicketCuttableProps(void)
{
    UpdateCuttableProps();
}

void InitYaguraPointProps(void)
{
    struct SceneNode *node;
    int i;

    node = gStagePropNodes;
    for (i = 0; i < YAGURA_POINT_PROP_COUNT; i++) {
        InitStagePropNode(node);
        node->meshIndex = i;
        node->renderType = SCENE_NODE_WORLD;
        node++;
    }
}

void UpdateYaguraPointNightProps(void)
{
    int i;
    struct SceneNode *node;
    int *position;

    if (IsRendererDaytime() != 0)
        return;

    node = gStagePropNodes;
    position = gYaguraPointPropPositions[0];
    for (i = 0; i < YAGURA_POINT_PROP_COUNT; i++) {
        node->worldMatrix.t[0] = *position++;
        node->worldMatrix.t[1] = *position++;
        node->worldMatrix.t[2] = *position++;
        *gRenderQueueCursor++ = node++;
    }
}

void InitMoatRainAndFoliage(void)
{
    RainInit rainInit;
    SwayingFoliageInit foliageInit;

    rainInit.splashTpage = GetTPage(0, 0, 0x340, 0x180);
    rainInit.splashClut = GetClut(0x120, 0x1e0);
    rainInit.splashU = 0;
    rainInit.splashV = 0x80;
    rainInit.splashEnabled = ReturnTrueRainSplashEnabled;
    rainInit.floorHeightAt = GetMoatFloorHeight;
    rainInit.workBuffer = gRainWorkStorage;
    InitRainParticles(&rainInit);

    gRainColorTop.r = 0x40;
    gRainColorTop.g = 0x60;
    gRainColorTop.b = 0x90;
    gRainColorBottom.r = 0xa0;
    gRainColorBottom.g = 0xa0;
    gRainColorBottom.b = 0xc0;
    if (gRendererMonochrome) {
        gRainColorTop.r = gRainColorTop.g = gRainColorTop.b =
            ConvertRgbToLuminance(gRainColorTop.r, gRainColorTop.g, gRainColorTop.b);
        gRainColorBottom.r = gRainColorBottom.g = gRainColorBottom.b =
            ConvertRgbToLuminance(gRainColorBottom.r, gRainColorBottom.g, gRainColorBottom.b);
    }

    foliageInit.tpage = GetTPage(0, 0, 0x340, 0x100);
    foliageInit.clut = GetClut(0x130, 0x1e0);
    foliageInit.v1 = 0x7f;
    foliageInit.pos[0].vx = -2100;
    foliageInit.pos[0].vy = -3600;
    foliageInit.pos[0].vz = -1400;
    foliageInit.u0 = 0;
    foliageInit.v0 = 0;
    foliageInit.u1 = 0x80;
    foliageInit.storage = &gMoatFoliageStorage;
    InitSwayingFoliageMesh(&foliageInit);
    InitSpecialLightSlot();
}

static int ReturnTrueRainSplashEnabled(VECTOR *groundPos)
{
    return 1;
}

void BeginMoatFrame(void)
{
    ResetMoatBillboardFlags();
}

void UpdateMoatRainAndFoliage(void)
{
    MATRIX view;
    int count;

    view = gAspectCorrectedCameraMatrix;
    WEATHER_SCRATCH->lineColor[0].color = gRainColorTop;
    WEATHER_SCRATCH->lineColor[1].color = gRainColorBottom;
    WEATHER_SCRATCH->density = 0x40;
    WEATHER_SCRATCH->cameraHeading = (0x800 - GetCameraHeadingAngle()) & 0xfff;
    WEATHER_SCRATCH->viewMatrix = gAspectCorrectedCameraMatrix;
    WEATHER_SCRATCH->cameraPos.vx = gAspectCorrectedCameraMatrix.t[0];
    WEATHER_SCRATCH->cameraPos.vy = gAspectCorrectedCameraMatrix.t[1];
    WEATHER_SCRATCH->cameraPos.vz = gAspectCorrectedCameraMatrix.t[2];
    WEATHER_SCRATCH->packetCursor = gPacketCursor;
    count = UpdateAndDrawRain(WEATHER_SCRATCH);
    gPacketCursor = WEATHER_SCRATCH->packetCursor;
    InsertDeferredPrimsIntoOt(count);

    SWAYING_FOLIAGE_SCRATCH->tint.color.r = 0x80;
    SWAYING_FOLIAGE_SCRATCH->tint.color.g = 0x80;
    SWAYING_FOLIAGE_SCRATCH->tint.color.b = 0x80;
    SWAYING_FOLIAGE_SCRATCH->cameraHeading = (0x800 - GetCameraHeadingAngle()) & 0xfff;
    SWAYING_FOLIAGE_SCRATCH->viewMatrix = view;
    SWAYING_FOLIAGE_SCRATCH->cameraPos.vx = view.t[0];
    SWAYING_FOLIAGE_SCRATCH->cameraPos.vy = view.t[1];
    SWAYING_FOLIAGE_SCRATCH->cameraPos.vz = view.t[2];
    SWAYING_FOLIAGE_SCRATCH->packetCursor = gPacketCursor;
    count = UpdateAndDrawSwayingFoliage(SWAYING_FOLIAGE_SCRATCH);
    gPacketCursor = SWAYING_FOLIAGE_SCRATCH->packetCursor;
    InsertDeferredPrimsIntoOt(count);
}

void InitStage20NoOp(void)
{
}

void UpdateStage20NoOp(void)
{
}

void InitStage22SnowAndDecals(void)
{
    SetupSnowBuffers();
    InitStageGroundDecals();
}

void UpdateStage22SnowAndVramAnim(void)
{
    UpdateStageSnow(0x40);
    UpdateAndDrawStageGroundDecals();
    QueueAnimatedVramTextures(3, 0);
}

void InitGrottoStream(void)
{
    ResetWaterScrollOffset();
}

void UpdateGrottoStream(void)
{
    int side;
    MATRIX **joints;
    VECTOR direction;
    VECTOR position;

    UpdateWaterTextureScroll(0);
    if ((gSceneFrameCounter & 3) == 0) {
        direction.vx = 4096;
        direction.vy = -1024;
        direction.vz = 0;
        for (side = 0; side < 2; side++) {
            joints = GetFighterWorldMatrixList(side);
            if (joints[FIGHTER_JOINT_RIGHT_FOOT]->t[1] > 1800) {
                position.vx = joints[FIGHTER_JOINT_RIGHT_FOOT]->t[0] - 100;
                position.vz = joints[FIGHTER_JOINT_RIGHT_FOOT]->t[2];
                position.vy = 2000;
                SpawnDirectionalSprayUnit(SPRAY_KIND_STREAM, &position, &direction);
            }
            if (joints[FIGHTER_JOINT_LEFT_FOOT]->t[1] > 1800) {
                position.vx = joints[FIGHTER_JOINT_LEFT_FOOT]->t[0] - 100;
                position.vz = joints[FIGHTER_JOINT_LEFT_FOOT]->t[2];
                position.vy = 2000;
                SpawnDirectionalSprayUnit(SPRAY_KIND_STREAM, &position, &direction);
            }
        }
    }
}

static void InitStagePropNode(struct SceneNode *node)
{
    node->modelSlot = MODEL_SLOT_STAGE_EXTRA;
    node->rotOrder = SCENE_ROT_ORDER_FAST;
    node->meshIndex = 0;
    node->renderType = SCENE_NODE_STANDARD;
    node->parent = 0;
    node->lightSlot = LIGHT_SLOT_STAGE;
    node->rotation.vx = 0;
    node->rotation.vy = 0;
    node->rotation.vz = 0;

    gRotMatrixBuilders[node->rotOrder](&node->rotation, &node->localMatrix);
    node->localMatrix.t[0] = node->localMatrix.t[1] = node->localMatrix.t[2] = 0;
    node->worldMatrix = node->localMatrix;
}
