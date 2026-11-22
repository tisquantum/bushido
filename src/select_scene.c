/* Character select: the character, weapon and stage phases, the three showcase figures driven by
 * pose scripts, the scripted camera and the weapon models. */
#include <sys/types.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include "scene_node.h"
#include "light.h"
#include "screen_fade.h"
#include "camera.h"
#include "menu_animation.h"
#include "pose_stream.h"
#include "scene_math.h"
#include "angle.h"
#include "fighter_skeleton.h"
#include "resource_cache.h"
#include "fighter.h"
#include "model_arena.h"
#include "packet_build.h"
#include "fighter_cloth.h"
#include "fighter_cloth_setup.h"
#include "display_list.h"
#include "billboard_driver.h"
#include "fighter_pose.h"
#include "weapon_id.h"
#include "stage_callbacks.h"
#include "select_ui.h"
#include "select_scene.h"
#include "gradient_bg.h"
#include "menu_state.h"
#include "asset_load.h"
#include "light_init.h"
#include "main.h"
#include "model_table.h"
#include "scene.h"
#include "stage_light.h"
#include "tim.h"
#include "stage_id.h"
#include "globals.h"
#include "render.h"
#include "rot_matrix.h"
#include "fog.h"

typedef enum SelectPhase {
    SELECT_PHASE_CHARACTER = 0,
    SELECT_PHASE_WEAPON = 1,
    SELECT_PHASE_CONFIRMED = 2,
    SELECT_PHASE_STAGE = 3
} SelectPhase;

typedef enum SelectSoundId {
    SELECT_SOUND_CURSOR = 0,
    SELECT_SOUND_WEAPON_CURSOR = 1,
    SELECT_SOUND_CONFIRM = 2,
    SELECT_SOUND_WEAPON_CONFIRM = 3
} SelectSoundId;

#define CD_FILE_SELECT_TIMS        0x53
#define CD_FILE_SELECT_SCENE_DATA  0x54

/* The offset tables at the start of the two select archives (files 0x53 and 0x54). */
typedef struct SelectTimArchiveHeader {
    int sectionCount;
    int katzeTimOffset;
    int uiTimOffsets[3];
} SelectTimArchiveHeader;

typedef struct SelectSceneArchiveHeader {
    int sectionCount;
    int poseStreamOffset;
    int cameraTrackOffset;  /* one 12-byte key per frame */
    int weaponModelsOffset;
    int handModelsOffset;
    int endOffset;
} SelectSceneArchiveHeader;

typedef struct SelectCamSegment {
    short firstKey;
    short length; /* frames */
} SelectCamSegment;

#define SELECT_AUX_PART_COUNT      20
#define SELECT_MAIN_BUFFER_LIMIT   0xAF000
#define SELECT_AUX_BUFFER_LIMIT    0x32000

#define SELECT_WEAPON_MODEL_SLOT   0x10
#define SELECT_HAND_MODEL_SLOT     0x11
#define SELECT_HOST_MODEL_SLOT     0x12
#define SELECT_HOST_SHADOW_SLOT    0x13
#define SELECT_LIGHTING_SCENE      0x1A /* the lighting presets after the 26 stages' */
#define SELECT_SLOT_KATZE          6
#define SELECT_WEAPON_SLOT_COUNT   8
#define SELECT_WEAPON_SLOT_KATZE   8    /* the gun */
#define SELECT_STAGE_CONFIRM_FRAMES 30
#define SELECT_WEAPON_FROM_HOST    0x01
#define SELECT_WEAPON_IN_HAND      0x02
#define SELECT_WEAPON_ATTACHED     (SELECT_WEAPON_FROM_HOST | SELECT_WEAPON_IN_HAND)
#define SELECT_P1_STEP_AFTER_WEAPON 6 /* where P1's script resumes once a weapon is chosen */

static int gSelectGameMode;
static int gSelectPhase;
static unsigned long *gSelectPoseStream;
static unsigned char gSelectUnused[4]; /* 4 bytes; nothing reads or writes them */
static int gSelectUnreadFlag; /* written, never read */
static int gSelectCostumeBank;
static int gSelectTwoPlayer;
static int gSelectInitCount;
static int gSelectWeaponSlotP1;
static int gSelectWeaponSlotP2;
static int gSelectWeaponAttach[2];
static struct SceneNode *gSelectFighterParts[2];
static struct SceneNode *gSelectAuxParts;
static struct SceneNode *gSelectAuxShadows;
static MATRIX **gSelectAuxMatrixTable;
static int gSelectStageIndex;
static int gSelectStageSubState;
static int gSelectStageConfirmTimer;
static SelectCameraKey *gSelectCameraTrack;
static SelectCamSegment *gSelectCamSegment;
static short gSelectCamSegmentFrame[2]; /* only [0] is used */
static short gSelectCamPhase[2];        /* only [0] is used */
static unsigned short *gSelectKatzeTimGroup;
static int gSelectCharIndex[MENU_ANIM_SLOT_COUNT];
static struct SceneNode gSelectWeaponNodes[2];
static struct SceneNode gSelectWeaponShadows[2];
static int gDefaultSelection[2][2]; /* only [0] is used */
static struct MenuAnimState gSelectAnimations[MENU_ANIM_SLOT_COUNT];

char *InitCharacterSelectScene(int costumeLayout, int selectMode, char *mainCursor, struct SceneNode *workBuffer)
{
    char *mainBase;
    char *auxCursor;
    char *auxBase;
    int *weaponModels;
    int *handModels;
    int auxUsedBytes;
    int remainingBytes;
    int firstTimOffset;
    int twoPlayerMode;
    int defaultCharacterSlot;
    int defaultWeaponSlot;
    unsigned long *poseStream;

    gSelectAuxParts = workBuffer;
    gSelectAuxShadows = workBuffer + SELECT_AUX_PART_COUNT;
    gSelectAuxMatrixTable = (MATRIX **)(workBuffer + SELECT_AUX_PART_COUNT * 2);
    ResetModelArena();
    mainBase = mainCursor;
    InitClothSystem();
    SetFighterClothSkeletonBase(0, 0);
    SetFighterClothSkeletonBase(1, 0);
    auxCursor = GetAuxResourceBuffer();
    gSelectCostumeBank = 1 - costumeLayout;
    ReadCdFileSync(CD_FILE_SELECT_TIMS, mainCursor);
    firstTimOffset = ((SelectTimArchiveHeader *)mainCursor)->uiTimOffsets[0];
    gSelectKatzeTimGroup = (unsigned short *)(mainCursor + ((SelectTimArchiveHeader *)mainCursor)->katzeTimOffset);
    LoadTimGroup((unsigned short *)(mainCursor + firstTimOffset), 0);
    LoadTimGroup((unsigned short *)(mainCursor + ((SelectTimArchiveHeader *)mainCursor)->uiTimOffsets[1]), 0);
    LoadTimGroup((unsigned short *)(mainCursor + ((SelectTimArchiveHeader *)mainCursor)->uiTimOffsets[2]), 0);
    mainCursor += ((SelectTimArchiveHeader *)mainCursor)->uiTimOffsets[0];
    DrawSync(0);
    mainCursor = (char *)LoadSelectAuxModel((unsigned int *)mainCursor);
    auxBase = auxCursor;
    /* These checks compare a distance in words with a limit in bytes. */
    if ((unsigned)((int *)mainCursor - (int *)mainBase) >= SELECT_MAIN_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    ClearCachedResources();
    mainCursor = (char *)LoadResourceGroup(
        gSelectCostumeBank == 0 ? RESOURCE_GROUP_SELECT_VS_MAIN : RESOURCE_GROUP_SELECT_STORY_MAIN,
        (int *)mainCursor);
    if ((unsigned)((int *)mainCursor - (int *)mainBase) >= SELECT_MAIN_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    ReadCdFileSync(CD_FILE_SELECT_SCENE_DATA, auxCursor);
    gSelectPoseStream = (unsigned long *)(auxCursor + ((SelectSceneArchiveHeader *)auxCursor)->poseStreamOffset);
    gSelectCameraTrack = (SelectCameraKey *)(auxCursor + ((SelectSceneArchiveHeader *)auxCursor)->cameraTrackOffset);
    weaponModels = (int *)(auxCursor + ((SelectSceneArchiveHeader *)auxCursor)->weaponModelsOffset);
    handModels = (int *)(auxCursor + ((SelectSceneArchiveHeader *)auxCursor)->handModelsOffset);
    auxCursor += ((SelectSceneArchiveHeader *)auxCursor)->endOffset;
    if ((unsigned)((int *)auxCursor - (int *)auxBase) >= SELECT_AUX_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    /* the original makes the same test twice */
    if ((unsigned)((int *)auxCursor - (int *)auxBase) >= SELECT_AUX_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    if ((unsigned)((int *)mainCursor - (int *)mainBase) >= SELECT_MAIN_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    RegisterModelTable(weaponModels, SELECT_WEAPON_MODEL_SLOT);
    RegisterModelTable(handModels, SELECT_HAND_MODEL_SLOT);
    if (gSelectCostumeBank == 0) {
        auxCursor = (char *)LoadResourceGroup(RESOURCE_GROUP_SELECT_VS_AUX, (int *)auxCursor);
    } else {
        auxCursor = (char *)LoadResourceGroup(RESOURCE_GROUP_SELECT_STORY_AUX, (int *)auxCursor);
    }
    auxUsedBytes = auxCursor - auxBase;
    if ((unsigned)((int *)auxCursor - (int *)auxBase) >= SELECT_AUX_BUFFER_LIMIT) {
        TriggerDebugTrap();
    }
    remainingBytes = SELECT_MAIN_BUFFER_LIMIT - (mainCursor - mainBase);
    if (remainingBytes < 0) {
        TriggerDebugTrap();
    }
    remainingBytes = SELECT_AUX_BUFFER_LIMIT - auxUsedBytes;
    if (remainingBytes < 0) {
        TriggerDebugTrap();
    }
    gSelectGameMode = selectMode;
    switch (selectMode) {
    case SELECT_MODE_STORY:
    case SELECT_MODE_SLASH:
        gSelectInitCount = 0;
        twoPlayerMode = SELECT_PLAYERS_1P;
        ResetCharacterSelectState(twoPlayerMode);
        break;
    case SELECT_MODE_VS:
    case SELECT_MODE_POV_VS:
    case SELECT_MODE_PRACTICE:
    case SELECT_MODE_LINK_VS:
    case SELECT_MODE_VS_KATZE:
        gSelectInitCount = 0;
        twoPlayerMode = SELECT_PLAYERS_2P;
        ResetCharacterSelectState(twoPlayerMode);
        break;
    case SELECT_MODE_STORY_CHALLENGER:
        twoPlayerMode = SELECT_PLAYERS_CHALLENGER;
        defaultCharacterSlot = gDefaultSelection[0][0];
        defaultWeaponSlot = gDefaultSelection[0][1];
        gSelectAnimations[MENU_ANIM_SLOT_P1].flags = MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED;
        gSelectInitCount = 1;
        gSelectCharIndex[0] = defaultCharacterSlot;
        gSelectWeaponSlotP1 = defaultWeaponSlot;
        ResetCharacterSelectState(twoPlayerMode);
        break;
    }
    poseStream = gSelectPoseStream;
    gSelectStageIndex = -1;
    gSelectStageSubState = SELECT_STAGE_NAVIGATE;
    gSelectStageConfirmTimer = 0;
    InitPoseStream(poseStream);
    InitSceneCameras();
    InitLightSlots();
    ApplyStageLighting(SELECT_LIGHTING_SCENE);
    BuildScreenFadePrim();
    SetStageClearAndFogColor(0);
    InitMenuGradientBackdrop(0);
    return mainCursor;
}

int gSelectPeakPacketBytes = 0; /* read only by its own update */

#ifdef VERSION_JP
int UpdateCharacterSelectFrame(int p1Move, int p2Move, int p1Confirm, int p2Confirm)
#else
int UpdateCharacterSelectFrame(int p1Move, int p2Move, int p1Confirm, int p2Confirm, int p1Vertical, int p2Vertical)
#endif
{
    int frameParity;
#ifndef VERSION_JP
    int p1VerticalInput;
    int p2VerticalInput;
#endif
    int result;
    char *packetStart;
    char *packetEnd;
    int highWater;
    int packetBytes;

    frameParity = gFrameCounter;
#ifndef VERSION_JP
    p1VerticalInput = p1Vertical;
    p2VerticalInput = p2Vertical;
#endif
    gPacketCursor = GetAuxGpuPacketBuffer(frameParity & 1);
    gRenderQueueCursor = gRenderQueue;
#ifdef VERSION_JP
    result = UpdateCharacterSelectMenu(p1Move, p2Move, p1Confirm, p2Confirm);
#else
    result = UpdateCharacterSelectMenu(p1Move, p2Move, p1Confirm, p2Confirm, p1VerticalInput, p2VerticalInput);
#endif
    SubmitSceneDisplayList();
    SubmitMenuGradientBackdrop();
    packetStart = GetAuxGpuPacketBuffer(gFrameCounter & 1);
    packetEnd = gPacketCursor;
    highWater = gSelectPeakPacketBytes;
    packetBytes = packetEnd - packetStart;
    if ((unsigned int)highWater < packetBytes) {
        gSelectPeakPacketBytes = packetBytes;
    }
    if (result == 0) {
        if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
            result = 2;
        } else {
            result = 1;
        }
    } else {
        result = 0;
    }
    return result;
}

/* Nothing calls it. */
static void SelectSceneNoOp(void)
{
}

short gSelectSlotToCharacter[8] = { 4, 3, 1, 2, 5, 0, 14, 0 }; /* indexed 0 to 6: nothing reads the last entry */

short gSelectSlotToWeapon[10] = {
    WEAPON_ID_KATANA, WEAPON_ID_RAPIER, WEAPON_ID_NAGINATA, WEAPON_ID_SLEDGEHAMMER,
    WEAPON_ID_NODACHI, WEAPON_ID_LONG_SWORD, WEAPON_ID_SABER, WEAPON_ID_BROADSWORD,
    WEAPON_ID_GUN, 0
}; /* the tenth is not read */
/* Stage ids by SelectGameMode, -1 for none; 64 bytes, nothing reads them. */
signed char gStageSelectStagesByMode[8][8] = {
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    {
        8, STAGE_ID_YAHIRO_ROAD, STAGE_ID_UNDERGROUND_GROTTO, STAGE_ID_BAMBOO_THICKET,
        STAGE_ID_CHERRY_BLOSSOM_GROVE, STAGE_ID_DOZAEMON_MOAT, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN,
    },
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    {
        8, STAGE_ID_YAHIRO_ROAD, STAGE_ID_UNDERGROUND_GROTTO, STAGE_ID_BAMBOO_THICKET,
        STAGE_ID_CHERRY_BLOSSOM_GROVE, 7, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN,
    },
    {
        STAGE_ID_MEIKYOKAN, STAGE_ID_CHERRY_BLOSSOM_GROVE, STAGE_ID_EXECUTIONERS_COVE, -1,
        -1, -1, -1, -1,
    },
    {
        8, STAGE_ID_WARRIORS_DWELLING, STAGE_ID_BAMBOO_THICKET, STAGE_ID_CHERRY_BLOSSOM_GROVE,
        7, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN, -1,
    },
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    {
        8, STAGE_ID_YAHIRO_ROAD, STAGE_ID_UNDERGROUND_GROTTO, STAGE_ID_BAMBOO_THICKET,
        STAGE_ID_CHERRY_BLOSSOM_GROVE, STAGE_ID_DOZAEMON_MOAT, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN,
    },
};
unsigned short gSelectHostScript[22] = {
    0, 1,
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 0), MENU_ANIM_CMD(MENU_ANIM_OP_BRANCH_MODE, 0x30),
    MENU_ANIM_CMD(MENU_ANIM_OP_BRANCH_COND, 0x02), 11,
    MENU_ANIM_CMD(MENU_ANIM_OP_JUMP, 9), 2,
    3, MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 1),
    4, MENU_ANIM_CMD(MENU_ANIM_OP_BRANCH_COND, 0x11),
    MENU_ANIM_CMD(MENU_ANIM_OP_BRANCH_MODE, 0x02), 5,
    MENU_ANIM_CMD(MENU_ANIM_OP_HALT, 0), 6,
    7, 8,
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 2), 9,
    10, MENU_ANIM_CMD(MENU_ANIM_OP_HALT, 0),
};
unsigned short gSelectP1Script[14] = {
    MENU_ANIM_CMD(MENU_ANIM_OP_PLAY_CHAR_CLIP, 12),
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_LOOP, 0),
    MENU_ANIM_CMD(MENU_ANIM_OP_PLAY_CHAR_CLIP, 13),
    36,
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 1),
    38,
    39,
    MENU_ANIM_CMD(MENU_ANIM_OP_NOP, 0),
    MENU_ANIM_CMD(MENU_ANIM_OP_NOP, 0),
    MENU_ANIM_CMD(MENU_ANIM_OP_NOP, 0),
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 2),
    40,
    MENU_ANIM_CMD(MENU_ANIM_OP_HALT, 0),
    0,
};
unsigned short gSelectP2Script[10] = {
    MENU_ANIM_CMD(MENU_ANIM_OP_PLAY_CHAR_CLIP, 14),
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_LOOP, 0),
    MENU_ANIM_CMD(MENU_ANIM_OP_PLAY_CHAR_CLIP, 15),
    36,
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 3),
    37, 39,
    MENU_ANIM_CMD(MENU_ANIM_OP_WAIT_FLAG_HOLD, 2),
    41,
    MENU_ANIM_CMD(MENU_ANIM_OP_HALT, 0),
};
struct MenuAnimEntry gMenuAnimClips[42] = {
    { 0, 45 }, { 45, 2 }, { 47, 35 }, { 82, 2 }, { 84, 30 }, { 114, 2 }, { 116, 20 },
    { 136, 15 }, { 151, 2 }, { 153, 45 }, { 198, 2 }, { 200, 35 }, { 235, 40 }, { 275, 20 },
    { 295, 40 }, { 335, 20 }, { 355, 45 }, { 400, 20 }, { 420, 30 }, { 450, 20 }, { 470, 30 },
    { 500, 15 }, { 515, 30 }, { 545, 15 }, { 560, 36 }, { 596, 20 }, { 616, 40 }, { 656, 20 },
    { 676, 30 }, { 706, 20 }, { 726, 40 }, { 766, 20 }, { 786, 30 }, { 816, 15 }, { 831, 100 },
    { 931, 15 }, { 946, 2 }, { 948, 34 }, { 982, 34 }, { 1016, 2 }, { 1018, 60 }, { 1078, 60 },
};

int gSelectShownPhase = 0;
SelectCamSegment gSelectCamScript1P[4] = { { 0, 15 }, { 15, 10 }, { 25, 10 }, { 35, 60 } };
SelectCamSegment gSelectCamScript2P[7] = {
    { 95, 15 }, { 110, 10 }, { 120, 10 }, { 130, 10 }, { 140, 10 }, { 150, 60 }, { 0, 0 },
};

void ResetCharacterSelectState(int twoPlayer)
{
    int entryCount;
    int gameMode;
    int slot;
    struct MenuAnimState *menuAnim;
    int savedFlags;
    int resumeStep;
    struct SceneNode *hostHand;
    struct SceneNode *node;
    struct SceneNode *p2Parts;
    int *weaponAttach;

    entryCount = gSelectInitCount;
    gSelectTwoPlayer = twoPlayer;
    if (entryCount == 0) {
        gSelectCharIndex[0] = 0;
        gSelectWeaponSlotP1 = 0;
        gSelectCharIndex[1] = 5;
    } else {
        gSelectGameMode = SELECT_MODE_STORY_CHALLENGER;
        if (gSelectCharIndex[0] == 5) {
            gSelectCharIndex[1] = 4;
        } else {
            gSelectCharIndex[1] = 5;
        }
    }
    gSelectWeaponSlotP2 = 0;
    FreeAllFighterCloth();
    ReloadSelectFighterModels(1, 1);
    gameMode = gSelectGameMode;
    switch (gameMode) {
    case SELECT_MODE_STORY: case SELECT_MODE_POV_VS: case SELECT_MODE_PRACTICE: case SELECT_MODE_SLASH:
        SetFighterClothDirty(1, 0);
        break;
    case SELECT_MODE_VS: case SELECT_MODE_STORY_CHALLENGER: case SELECT_MODE_LINK_VS: case SELECT_MODE_VS_KATZE:
        break;
    }
    slot = 0;
    gCameraNode.localMatrix.t[0] = 0;
    gCameraNode.localMatrix.t[1] = 0;
    gCameraNode.localMatrix.t[2] = 0;
    gCameraNode.rotation.vx = 0;
    gCameraNode.rotation.vy = 0;
    gCameraNode.rotation.vz = 0;
    gSelectPhase = SELECT_PHASE_CHARACTER;
    do {
        menuAnim = &gSelectAnimations[slot];
        if (slot == MENU_ANIM_SLOT_P1 && gSelectInitCount > 0) {
            savedFlags = menuAnim->flags;
            resumeStep = -(savedFlags & MENU_ANIM_FLAG_CHAR_CONFIRMED) & 3;
            if ((savedFlags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
                resumeStep = SELECT_P1_STEP_AFTER_WEAPON;
            }
            menuAnim->script = &gSelectP1Script[resumeStep];
            menuAnim->step = resumeStep;
            menuAnim->duration = MENU_ANIM_DURATION_HOLD;
            menuAnim->flags &= MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED;
        } else {
            switch (slot) {
            case MENU_ANIM_SLOT_P1:
                gSelectAnimations[slot].script = gSelectP1Script;
                break;
            case MENU_ANIM_SLOT_P2:
                gSelectAnimations[slot].script = gSelectP2Script;
                break;
            case MENU_ANIM_SLOT_HOST:
                gSelectAnimations[slot].script = gSelectHostScript;
                break;
            }
            gSelectAnimations[slot].flags = 0;
            gSelectAnimations[slot].clipIndex = 0;
            gSelectAnimations[slot].startFrame = 0;
            gSelectAnimations[slot].timer = 0;
            gSelectAnimations[slot].duration = MENU_ANIM_DURATION_HOLD;
            gSelectAnimations[slot].step = 0;
        }
        ++slot;
    } while (slot < MENU_ANIM_SLOT_COUNT);
    slot = 0;
    weaponAttach = gSelectWeaponAttach;
    gSelectUnreadFlag = 0;
    hostHand = &gSelectAuxParts[FIGHTER_NODE_RIGHT_HAND];
    do {
        node = &gSelectWeaponNodes[slot];
        node->meshIndex = 0;
        node->modelSlot = SELECT_WEAPON_MODEL_SLOT;
        node->renderType = SCENE_NODE_STANDARD;
        node->parent = hostHand; /* set three times in this loop; the last, player two's hand, stands */
        node->parent = &gSelectFighterParts[0][FIGHTER_NODE_RIGHT_HAND];
        p2Parts = gSelectFighterParts[1];
        node->rotation.vx = -58;
        node->rotation.vy = -61;
        node->rotation.vz = 0x7c7;
        node->localMatrix.t[0] = 7;
        node->localMatrix.t[1] = 0x43;
        node->localMatrix.t[2] = 0x149;
        node->rotOrder = SCENE_ROT_ORDER_FAST;
        node->lightSlot = slot;
        node->matrixState = SCENE_MATRIX_DIRTY;
        node->parent = &p2Parts[FIGHTER_NODE_RIGHT_HAND];
        if (gSelectInitCount != 0 && slot == 0) {
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
                gSelectWeaponAttach[0] = SELECT_WEAPON_ATTACHED;
            } else {
                gSelectWeaponAttach[0] = 0;
            }
        } else {
            weaponAttach[slot] = 0;
        }
        node = &gSelectWeaponShadows[slot];
        node->parent = &gSelectWeaponNodes[slot];
        node->lightSlot = slot;
        ++slot;
        node->modelSlot = SELECT_WEAPON_MODEL_SLOT;
        node->renderType = SCENE_NODE_SHADOW;
        node->rotOrder = SCENE_ROT_ORDER_FAST;
        node->rotation.vz = 0;
        node->rotation.vy = 0;
        node->rotation.vx = 0;
        node->localMatrix.t[2] = 0;
        node->localMatrix.t[1] = 0;
        node->localMatrix.t[0] = 0;
        node->matrixState = SCENE_MATRIX_DIRTY;
    } while (slot < 2);
    gSelectCamSegment = twoPlayer != 0 ? gSelectCamScript2P : gSelectCamScript1P;
    gSelectCamSegmentFrame[0] = 0;
    gSelectCamPhase[0] = 0;
    InitSelectScreenUiState();
    gSelectInitCount += 1;
}

/* The Japanese build's stage select moves on the horizontal input alone. */
#ifdef VERSION_JP
int UpdateCharacterSelectMenu(int p1Move, int p2Move, int p1Confirm, int p2Confirm)
#else
int UpdateCharacterSelectMenu(int p1Move, int p2Move, int p1Confirm, int p2Confirm, int p1Vertical, int p2Vertical)
#endif
{
    int menuValue;
    int result;
    int p1Dir;
    int p2Dir;
    int player;
    struct MenuAnimState *anim;
    int keyIndex;
    int advance;
    unsigned short p1Weapon;
    unsigned short p2Weapon;
    struct SceneNode *weaponNode;
    int shown;
    struct SceneNode *hand;
    struct SceneNode *shadowNode;
    int p1Value;
    int p1Chosen;
    int p2Value;
    int camFrame;
    VECTOR position;
    WorldVector rotation;

    UpdateMenuAnimScript(MENU_ANIM_SLOT_P1, gSelectTwoPlayer);
    result = 0;
    if (gSelectTwoPlayer != 0) {
        UpdateMenuAnimScript(MENU_ANIM_SLOT_P2, gSelectTwoPlayer);
    }
    UpdateMenuAnimScript(MENU_ANIM_SLOT_HOST, gSelectTwoPlayer);
    if (gSelectTwoPlayer == 0) {
        p2Move = 0;
        p2Confirm = 0;
    }
    if (p1Move != 0) {
        p1Dir = p1Move < 0 ? -1 : 1;
    } else {
        p1Dir = 0;
    }
    if (p2Move != 0) {
        p2Dir = p2Move < 0 ? -1 : 1;
    } else {
        p2Dir = 0;
    }
    switch (gSelectPhase) {
    case SELECT_PHASE_CHARACTER:
        if ((unsigned int)(gSelectGameMode - SELECT_MODE_POV_VS) < 2) {
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_CHAR_CONFIRMED) == 0) {
                p2Dir = 0;
                gSelectCharIndex[1] = -1;
                p2Confirm = 0;
            } else {
                if (gSelectCharIndex[1] == -1) {
                    gSelectCharIndex[1] = 5;
                }
                if (gSelectCharIndex[0] == gSelectCharIndex[1]) {
                    do {
                        gSelectCharIndex[1]--;
                    } while (gSelectCharIndex[0] == gSelectCharIndex[1]);
                    ReloadSelectFighterModels(0, 1);
                    gSelectAnimations[MENU_ANIM_SLOT_P2].script = gSelectP2Script;
                    gSelectAnimations[MENU_ANIM_SLOT_P2].duration = 0;
                    gSelectAnimations[MENU_ANIM_SLOT_P2].timer = 0;
                    gSelectAnimations[MENU_ANIM_SLOT_P2].step = 0;
                    UpdateMenuAnimScript(MENU_ANIM_SLOT_P2, gSelectTwoPlayer);
                }
                p2Dir = p1Dir;
                p1Dir = 0;
                p2Confirm = p1Confirm;
                p1Confirm = 0;
            }
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_CHAR_CONFIRMED) != 0) {
            p1Dir = 0;
            p1Confirm = 0;
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_CHAR_CONFIRMED) != 0) {
            p2Dir = 0;
            p2Confirm = 0;
        }
        if (gSelectGameMode == SELECT_MODE_VS_KATZE) {
            menuValue = SELECT_SLOT_KATZE;
        } else {
            menuValue = PLAYABLE_CHARACTER_COUNT - 1;
        }
        gSelectCharIndex[0] += p1Dir;
        if (gSelectCharIndex[0] < 0) {
            gSelectCharIndex[0] = menuValue;
        }
        if (gSelectCharIndex[0] > menuValue) {
            gSelectCharIndex[0] = 0;
        }
        if (p2Dir != 0) {
            gSelectCharIndex[1] += p2Dir;
            if (gSelectCharIndex[1] < 0) {
                gSelectCharIndex[1] = menuValue;
            }
            if (gSelectCharIndex[1] > menuValue) {
                gSelectCharIndex[1] = 0;
            }
        }
        if (gSelectTwoPlayer != 0) {
            if (gSelectCharIndex[0] == gSelectCharIndex[1]) {
                if (p1Dir != 0) {
                    gSelectCharIndex[0] += p1Dir;
                    if (gSelectCharIndex[0] < 0) {
                        gSelectCharIndex[0] = menuValue;
                    }
                    if (gSelectCharIndex[0] > menuValue) {
                        gSelectCharIndex[0] = 0;
                    }
                } else {
                    gSelectCharIndex[1] += p2Dir;
                    if (gSelectCharIndex[1] < 0) {
                        gSelectCharIndex[1] = menuValue;
                    }
                    if (gSelectCharIndex[1] > menuValue) {
                        gSelectCharIndex[1] = 0;
                    }
                }
            }
        }
        if (p1Dir != 0 || p2Dir != 0) {
            ReloadSelectFighterModels(p1Dir, p2Dir);
            if (p1Dir != 0) {
                gSelectAnimations[MENU_ANIM_SLOT_P1].script = gSelectP1Script;
                gSelectAnimations[MENU_ANIM_SLOT_P1].duration = 0;
                gSelectAnimations[MENU_ANIM_SLOT_P1].timer = 0;
                gSelectAnimations[MENU_ANIM_SLOT_P1].step = 0;
                UpdateMenuAnimScript(MENU_ANIM_SLOT_P1, gSelectTwoPlayer);
            }
            if (p2Dir != 0) {
                gSelectAnimations[MENU_ANIM_SLOT_P2].script = gSelectP2Script;
                gSelectAnimations[MENU_ANIM_SLOT_P2].duration = 0;
                gSelectAnimations[MENU_ANIM_SLOT_P2].timer = 0;
                gSelectAnimations[MENU_ANIM_SLOT_P2].step = 0;
                UpdateMenuAnimScript(MENU_ANIM_SLOT_P2, gSelectTwoPlayer);
            }
            PlayCharacterSelectSound(SELECT_SOUND_CURSOR);
        }
        break;
    case SELECT_PHASE_WEAPON:
        if ((unsigned int)(gSelectGameMode - SELECT_MODE_POV_VS) < 2) {
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) == 0) {
                p2Confirm = 0;
                p2Dir = 0;
            } else {
                p2Confirm = p1Confirm;
                p2Dir = p1Dir;
                p1Confirm = 0;
                p1Dir = 0;
            }
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) == 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].step == 9) {
            if (gSelectGameMode == SELECT_MODE_SLASH) {
                gSelectWeaponSlotP1 = 0;
                gSelectAnimations[MENU_ANIM_SLOT_P1].flags |= MENU_ANIM_FLAG_WEAPON_CONFIRMED;
                PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CONFIRM);
            } else {
                gSelectWeaponSlotP1 += p1Dir;
                if (gSelectWeaponSlotP1 < 0) {
                    gSelectWeaponSlotP1 = SELECT_WEAPON_SLOT_COUNT - 1;
                }
                if (gSelectWeaponSlotP1 >= SELECT_WEAPON_SLOT_COUNT) {
                    gSelectWeaponSlotP1 = 0;
                }
                if (p1Dir != 0) {
                    PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CURSOR);
                }
            }
        } else {
            p1Confirm = 0;
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) == 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].step >= 9) {
            gSelectWeaponSlotP2 += p2Dir;
            if (gSelectWeaponSlotP2 < 0) {
                gSelectWeaponSlotP2 = SELECT_WEAPON_SLOT_COUNT - 1;
            }
            if (gSelectWeaponSlotP2 >= SELECT_WEAPON_SLOT_COUNT) {
                gSelectWeaponSlotP2 = 0;
            }
            if (p2Dir != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CURSOR);
            }
        } else {
            p2Confirm = 0;
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].step == 0x12) {
            gSelectAnimations[MENU_ANIM_SLOT_P2].flags |= MENU_ANIM_FLAG_P2_WEAPON_CUE;
        }
        break;
    case SELECT_PHASE_CONFIRMED:
        break;
    case SELECT_PHASE_STAGE:
#ifndef VERSION_JP
        menuValue = p1Vertical;
#endif
        switch (gSelectStageSubState) {
        case SELECT_STAGE_NAVIGATE: {
            int horizontal;
            horizontal = p1Dir;
#ifdef VERSION_JP
            if ((gSelectGameMode == SELECT_MODE_VS || gSelectGameMode == SELECT_MODE_LINK_VS ||
                 gSelectGameMode == SELECT_MODE_VS_KATZE) && p1Dir == 0) {
                horizontal = p2Dir;
            }
            horizontal = horizontal != 0 ? (horizontal < 0 ? -1 : 1) : 0;
            gSelectStageIndex += horizontal;
            if (horizontal != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_CURSOR);
            }
#else
            if ((gSelectGameMode == SELECT_MODE_VS || gSelectGameMode == SELECT_MODE_LINK_VS ||
                 gSelectGameMode == SELECT_MODE_VS_KATZE) && p1Dir == 0 && p1Vertical == 0) {
                menuValue = p2Vertical;
                horizontal = p2Dir;
            }
            menuValue = menuValue != 0 ? (menuValue < 0 ? -1 : 1) : 0;
            horizontal = horizontal != 0 ? (horizontal < 0 ? -1 : 1) : 0;
            gSelectStageIndex = StepStageSelectCursor(gSelectGameMode, gSelectStageIndex, horizontal, menuValue);
            if (menuValue != 0 || horizontal != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_CURSOR);
            }
#endif
            menuValue = p1Confirm;
            if (gSelectGameMode == SELECT_MODE_VS || gSelectGameMode == SELECT_MODE_LINK_VS ||
                gSelectGameMode == SELECT_MODE_VS_KATZE) {
                menuValue |= p2Confirm;
            }
            if (menuValue != 0) {
                gSelectStageConfirmTimer = SELECT_STAGE_CONFIRM_FRAMES;
                gSelectStageSubState++;
                PlayCharacterSelectSound(SELECT_SOUND_CONFIRM);
            }
            break;
        }
        case SELECT_STAGE_CONFIRM_DELAY:
            gSelectStageConfirmTimer--;
            if (gSelectStageConfirmTimer <= 0) {
                result = 1;
                gSelectStageSubState = SELECT_STAGE_DONE;
            }
            break;
        }
        break;
    }
    player = 0;
    if (gSelectTwoPlayer >= 0) {
        do {
            int pressed;

            anim = &gSelectAnimations[player];
            if (player != 0) {
                pressed = p2Confirm;
                if (player != 1) {
                    pressed = 0;
                }
            } else {
                pressed = p1Confirm;
            }
            if (pressed != 0) {
                switch (anim->step) {
                case 1:
                    anim->flags |= MENU_ANIM_FLAG_CHAR_CONFIRMED;
                    PlayCharacterSelectSound(SELECT_SOUND_CONFIRM);
                    break;
                case 4:
                    anim->flags |= MENU_ANIM_FLAG_WEAPON_CONFIRMED;
                    PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CONFIRM);
                    break;
                }
            }
            menuValue = gSelectAnimations[MENU_ANIM_SLOT_HOST].step;
            if (menuValue == 0x15 || menuValue == 0x0e) {
                anim->flags |= MENU_ANIM_FLAG_HOST_DONE;
            }
        } while (gSelectTwoPlayer >= ++player);
    }
    if (gSelectGameMode == SELECT_MODE_VS_KATZE) {
        if (gSelectCharIndex[0] == SELECT_SLOT_KATZE) {
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags &
                 (MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED)) == MENU_ANIM_FLAG_CHAR_CONFIRMED) {
                anim = gSelectAnimations;
                anim->script = &gSelectP1Script[SELECT_P1_STEP_AFTER_WEAPON];
                anim->step = SELECT_P1_STEP_AFTER_WEAPON;
                anim->flags |= MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED;
                anim->duration = MENU_ANIM_DURATION_HOLD;
                gSelectWeaponSlotP1 = SELECT_WEAPON_SLOT_KATZE;
            }
        }
        if (gSelectCharIndex[1] == SELECT_SLOT_KATZE) {
            if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags &
                 (MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED)) == MENU_ANIM_FLAG_CHAR_CONFIRMED) {
                gSelectAnimations[MENU_ANIM_SLOT_P2].flags |=
                    MENU_ANIM_FLAG_CHAR_CONFIRMED | MENU_ANIM_FLAG_WEAPON_CONFIRMED;
                gSelectAnimations[MENU_ANIM_SLOT_P2].duration = MENU_ANIM_DURATION_HOLD;
                gSelectWeaponSlotP2 = SELECT_WEAPON_SLOT_KATZE;
            }
        }
    }
    if (gSelectTwoPlayer != 0) {
        if ((gSelectAnimations[MENU_ANIM_SLOT_HOST].flags & MENU_ANIM_HOST_CHARS_CONFIRMED) == 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_P1].step >= 4 && gSelectAnimations[MENU_ANIM_SLOT_P2].step == 4) {
            gSelectAnimations[MENU_ANIM_SLOT_HOST].flags |= MENU_ANIM_HOST_CHARS_CONFIRMED;
            gSelectPhase = SELECT_PHASE_WEAPON;
            if (gSelectWeaponSlotP1 == SELECT_WEAPON_SLOT_KATZE || gSelectWeaponSlotP2 == SELECT_WEAPON_SLOT_KATZE) {
                LoadTimGroup(gSelectKatzeTimGroup, 0);
            }
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CONFIRM);
            }
            if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CONFIRM);
            }
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_P1].step == 0xa) {
            gSelectAnimations[MENU_ANIM_SLOT_HOST].flags |= MENU_ANIM_HOST_P1_WEAPON_DONE;
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_HOST].flags & MENU_ANIM_HOST_P2_WEAPON_DONE) == 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_P1].step == 0xa && gSelectAnimations[MENU_ANIM_SLOT_P2].step == 7) {
            gSelectAnimations[MENU_ANIM_SLOT_HOST].flags |= MENU_ANIM_HOST_P2_WEAPON_DONE;
            gSelectPhase = SELECT_PHASE_CONFIRMED;
        }
    } else {
        if ((gSelectAnimations[MENU_ANIM_SLOT_HOST].flags & MENU_ANIM_HOST_CHARS_CONFIRMED) == 0 &&
            gSelectAnimations[MENU_ANIM_SLOT_P1].step == 4) {
            gSelectAnimations[MENU_ANIM_SLOT_HOST].flags |= MENU_ANIM_HOST_CHARS_CONFIRMED;
            gSelectPhase = SELECT_PHASE_WEAPON;
            if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
                PlayCharacterSelectSound(SELECT_SOUND_WEAPON_CONFIRM);
            }
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_P1].step == 0xa) {
            gSelectPhase = SELECT_PHASE_CONFIRMED;
            gSelectAnimations[MENU_ANIM_SLOT_HOST].flags |= MENU_ANIM_HOST_P1_WEAPON_DONE;
        }
    }
    if (gSelectCamSegment->length - 1 < gSelectCamSegmentFrame[0]) {
        gSelectCamSegmentFrame[0] = gSelectCamSegment->length - 1;
        if (gSelectCamPhase[0] == 5 || gSelectCamPhase[0] == 7) {
            if (gSelectGameMode == SELECT_MODE_STORY || gSelectGameMode == SELECT_MODE_STORY_CHALLENGER ||
                gSelectGameMode == SELECT_MODE_SLASH) {
                result = 1;
            } else if (gSelectPhase != SELECT_PHASE_STAGE) {
                gSelectPhase = SELECT_PHASE_STAGE;
                if (gSelectStageIndex < 0) {
                    gSelectStageIndex = 0;
                }
                StartScreenFade(SCREEN_FADE_IN_FROM_WHITE_FAST);
            }
        }
    }
    camFrame = gSelectCamSegmentFrame[0];
    keyIndex = gSelectCamSegment->firstKey + camFrame;
    gCameraNode.localMatrix.t[0] = -gSelectCameraTrack[keyIndex].x;
    gCameraNode.localMatrix.t[1] = gSelectCameraTrack[keyIndex].y;
    gCameraNode.localMatrix.t[2] = -gSelectCameraTrack[keyIndex].z;
    gCameraNode.rotation.vx = gSelectCameraTrack[keyIndex].rotX;
    gCameraNode.rotation.vy = gSelectCameraTrack[keyIndex].rotY + ANGLE_HALF;
    gCameraNode.rotation.vz = gSelectCameraTrack[keyIndex].rotZ;
    gSelectCamSegmentFrame[0] = camFrame + 1;
    advance = 0;
    switch (gSelectCamPhase[0]) {
    case 0:
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 2 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].timer >= 25) {
            advance = 1;
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 7 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].timer >= 5) {
            gSelectCamSegmentFrame[0] = 0;
            gSelectCamPhase[0] = 3;
            gSelectCamSegment += 3;
            advance = 0;
        }
        break;
    case 1:
        if (gSelectAnimations[MENU_ANIM_SLOT_P1].clipIndex == 0x26) {
            if (gSelectTwoPlayer == 0 || gSelectCharIndex[1] == SELECT_SLOT_KATZE) {
                gSelectCamSegmentFrame[0] = 0;
                gSelectCamPhase[0] = 6;
                gSelectCamSegment++;
                advance = 0;
            } else {
                advance = 1;
            }
        }
        break;
    case 2:
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 7 &&
            gSelectAnimations[MENU_ANIM_SLOT_HOST].timer >= 5) {
            advance = 1;
        }
        break;
    case 3:
        if (gSelectAnimations[MENU_ANIM_SLOT_P2].clipIndex == 0x25) {
            advance = 1;
        }
        break;
    case 4:
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 0xa) {
            StartScreenFade(SCREEN_FADE_OUT_TO_WHITE_SLOW);
            advance = 1;
        }
        break;
    case 5:
        if (gSelectPhase < SELECT_PHASE_STAGE) {
            UpdateAndDrawScreenFade();
        }
        break;
    case 6:
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 5) {
            StartScreenFade(SCREEN_FADE_OUT_TO_WHITE_SLOW);
            advance = 1;
        }
        break;
    case 7:
        if (gSelectPhase < SELECT_PHASE_STAGE) {
            UpdateAndDrawScreenFade();
        }
        if (gSelectCharIndex[1] == SELECT_SLOT_KATZE) {
            gSelectCamSegmentFrame[0] = gSelectCamSegment->length;
        }
        break;
    }
    if (advance != 0) {
        gSelectCamSegmentFrame[0] = 0;
        gSelectCamSegment++;
        gSelectCamPhase[0]++;
    }
    BuildNegatedNodeRotation(&gCameraNode);
    UpdateAspectCorrectedCameraMatrix();
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P1]);
    RebuildLightDirectionMatrix(&gLightSlots[LIGHT_SLOT_P2]);
    if (gSelectPhase < SELECT_PHASE_STAGE) {
        if (gSelectCharIndex[0] != SELECT_SLOT_KATZE) {
            UpdateAndEnqueueFighterPose(MENU_ANIM_SLOT_P1, FIGHTER_POSE_MODE_MENU_SHOWCASE, &position.vx,
                                        (PoseAngles *)&rotation, 0, 0);
        }
        if (gSelectTwoPlayer != 0) {
            if (gSelectCharIndex[1] != -1 && gSelectCharIndex[1] != SELECT_SLOT_KATZE) {
                UpdateAndEnqueueFighterPose(MENU_ANIM_SLOT_P2, FIGHTER_POSE_MODE_MENU_SHOWCASE, &position.vx,
                                            (PoseAngles *)&rotation, 0, 0);
            }
        }
        UpdateAndEnqueueFighterPose(MENU_ANIM_SLOT_HOST, FIGHTER_POSE_MODE_MENU_SHOWCASE, &position.vx,
                                    (PoseAngles *)&rotation, 0, 0);
        EnqueueSelectAuxShadows();
        RebuildFighterClothIfDirty(0);
        if (gSelectTwoPlayer != 0) {
            RebuildFighterClothIfDirty(1);
        }
        ResetClothWindWithJitter();
        UpdateAllFighterCloth();
        UpdateAndDrawBillboardEffects();
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 3) {
            gSelectWeaponAttach[0] |= SELECT_WEAPON_FROM_HOST;
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_HOST].clipIndex == 8) {
            gSelectWeaponAttach[1] |= SELECT_WEAPON_FROM_HOST;
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_P1].clipIndex == 0x26 &&
            gSelectAnimations[MENU_ANIM_SLOT_P1].timer == 0x13) {
            gSelectWeaponAttach[0] |= SELECT_WEAPON_IN_HAND;
        }
        if (gSelectAnimations[MENU_ANIM_SLOT_P2].clipIndex == 0x25 &&
            gSelectAnimations[MENU_ANIM_SLOT_P2].timer == 0x11) {
            gSelectWeaponAttach[1] |= SELECT_WEAPON_IN_HAND;
        }
        player = 0;
        p1Weapon = gSelectSlotToWeapon[gSelectWeaponSlotP1];
        gSelectWeaponNodes[0].meshIndex = p1Weapon;
        p2Weapon = gSelectSlotToWeapon[gSelectWeaponSlotP2];
        gSelectWeaponShadows[0].meshIndex = p1Weapon + SELECT_WEAPON_SLOT_COUNT;
        gSelectWeaponNodes[1].meshIndex = p2Weapon;
        gSelectWeaponShadows[1].meshIndex = p2Weapon + SELECT_WEAPON_SLOT_COUNT;
        do {
            shown = 1;
            weaponNode = &gSelectWeaponNodes[player];
            switch (gSelectWeaponAttach[player]) {
            default:
                shown = 0;
                break;
            case SELECT_WEAPON_FROM_HOST:
                weaponNode->rotation.vx = 0;
                weaponNode->rotation.vy = 0;
                weaponNode->rotation.vz = 0;
                weaponNode->localMatrix.t[0] = 0;
                weaponNode->localMatrix.t[1] = 0;
                weaponNode->localMatrix.t[2] = 0;
                weaponNode->parent = &gSelectAuxParts[FIGHTER_NODE_RIGHT_HAND];
                break;
            case SELECT_WEAPON_ATTACHED:
                if (player == 0) {
                    hand = &gSelectFighterParts[0][FIGHTER_NODE_RIGHT_HAND];
                    weaponNode->rotation.vx = 0x32;
                    weaponNode->rotation.vy = 0x2d;
                    weaponNode->rotation.vz = 0x858;
                    weaponNode->localMatrix.t[0] = -59;
                    weaponNode->localMatrix.t[1] = 0x8e;
                    weaponNode->localMatrix.t[2] = 0xc3;
                } else {
                    hand = &gSelectFighterParts[1][FIGHTER_NODE_RIGHT_HAND];
                    weaponNode->rotation.vx = -58;
                    weaponNode->rotation.vy = -61;
                    weaponNode->rotation.vz = 0x7c7;
                    weaponNode->localMatrix.t[0] = 7;
                    weaponNode->localMatrix.t[1] = 0x43;
                    weaponNode->localMatrix.t[2] = 0x149;
                }
                weaponNode->parent = hand;
                break;
            }
            if (shown != 0) {
                weaponNode->matrixState = SCENE_MATRIX_DIRTY;
                UpdateNodeWorldMatrix(weaponNode);
                shadowNode = &gSelectWeaponShadows[player];
                *gRenderQueueCursor++ = weaponNode;
                gSelectWeaponShadows[player].matrixState = SCENE_MATRIX_DIRTY;
                UpdateNodeWorldMatrix(shadowNode);
                *gRenderQueueCursor++ = shadowNode;
            }
            ++player;
        } while (player < 2);
    }
    if (result != 0) {
        return result;
    }
    if (gSelectShownPhase != gSelectPhase) {
        gSelectShownPhase = gSelectPhase;
    }
    switch (gSelectShownPhase) {
    case SELECT_PHASE_CHARACTER:
        p1Value = gSelectCharIndex[0];
        p1Chosen = gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_CHAR_CONFIRMED;
        if (p1Chosen != 0) {
            p1Value |= 0x10000;
        }
        p2Value = gSelectCharIndex[1];
        if ((gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_CHAR_CONFIRMED) != 0) {
            p2Value |= 0x10000;
        }
        if (gSelectTwoPlayer == 0) {
            p2Value = -1;
        }
        if ((unsigned int)(gSelectGameMode - SELECT_MODE_POV_VS) < 2) {
            if (p1Chosen == 0) {
                p2Value = 0xffff;
            } else {
                p2Value += 7;
            }
        }
        UpdateAndDrawSelectScreenUi(SELECT_UI_CHARACTER, p1Value, p2Value);
        break;
    case SELECT_PHASE_WEAPON:
        p1Value = gSelectSlotToWeapon[gSelectWeaponSlotP1];
        p2Value = gSelectSlotToWeapon[gSelectWeaponSlotP2];
        if (gSelectTwoPlayer == 0) {
            p2Value = -1;
        }
        if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
            UpdateAndDrawWeaponConfirmStamp(0);
        }
        if (gSelectTwoPlayer != 0 &&
            (gSelectAnimations[MENU_ANIM_SLOT_P2].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) != 0) {
            UpdateAndDrawWeaponConfirmStamp(1);
        }
        UpdateAndDrawSelectScreenUi(SELECT_UI_WEAPON, p1Value, p2Value);
        break;
    case SELECT_PHASE_CONFIRMED:
        break;
    case SELECT_PHASE_STAGE:
        UpdateAndDrawScreenFade();
        gSelectStageIndex = UpdateAndDrawSelectScreenUi(
            SELECT_UI_STAGE, (gSelectGameMode << 8) | (unsigned char)gSelectStageIndex, gSelectStageSubState);
        break;
    }
    return result;
}

void UpdateMenuAnimScript(int slot, int twoPlayer)
{
    struct MenuAnimState *anim;
    int running;
    int resetTimer;
    short cmd;
    int opcode;
    int value;

    anim = &gSelectAnimations[slot];
    anim->timer++;
    if (anim->duration > 0 && anim->duration != anim->timer) {
        return;
    }

    running = 1;
    resetTimer = 1;
    do {
        cmd = *anim->script;
        if (cmd & MENU_ANIM_OP_MASK) {
            opcode = (cmd >> 8) & 0x7f;
            cmd &= 0xff;
            switch (opcode) {
            case MENU_ANIM_OP_WAIT_FLAG_LOOP:
                if ((anim->flags >> cmd) & 1) {
                    anim->script++;
                    anim->step++;
                } else {
                    running = 0;
                }
                break;
            case MENU_ANIM_OP_WAIT_FLAG_HOLD:
                if ((anim->flags >> cmd) & 1) {
                    anim->script++;
                    anim->step++;
                } else {
                    running = 0;
                    resetTimer = 0;
                    anim->timer--;
                }
                break;
            case MENU_ANIM_OP_HALT:
                running = 0;
                resetTimer = 0;
                anim->timer--;
                break;
            case MENU_ANIM_OP_PLAY_CHAR_CLIP:
                value = GetFighterCharacterId(slot);
                if (value >= PLAYABLE_CHARACTER_COUNT * 2) {
                    anim->duration = MENU_ANIM_DURATION_HOLD;
                    anim->startFrame = 0;
                    anim->script++;
                    anim->step++;
                } else {
                    if (value >= PLAYABLE_CHARACTER_COUNT) {
                        value -= PLAYABLE_CHARACTER_COUNT;
                    }
                    cmd += value * 4;
                    anim->clipIndex = cmd;
                    anim->startFrame = gMenuAnimClips[cmd].startFrame;
                    anim->duration = gMenuAnimClips[cmd].duration;
                    running = 0;
                    anim->script++;
                    anim->step++;
                }
                break;
            case MENU_ANIM_OP_BRANCH_MODE:
                if (twoPlayer != 0) {
                    value = cmd & 0xf;
                } else {
                    value = (cmd >> 4) & 0xf;
                }
                if (value & 8) {
                    value |= -16;
                }
                anim->script += value + 1;
                anim->step += value + 1;
                break;
            case MENU_ANIM_OP_BRANCH_COND:
                value = 0;
                switch ((cmd >> 4) & 0xf) {
                case 0:
                    if ((gSelectAnimations[MENU_ANIM_SLOT_P1].flags & MENU_ANIM_FLAG_WEAPON_CONFIRMED) == 0) {
                        value = 1;
                    }
                    break;
                case 1:
                    if (gSelectCharIndex[1] == SELECT_SLOT_KATZE) {
                        value = 1;
                    }
                    break;
                }
                if (value != 0) {
                    value = cmd & 0xf;
                    anim->script += value + 1;
                    anim->step += value + 1;
                } else {
                    anim->script++;
                    anim->step++;
                }
                break;
            case MENU_ANIM_OP_JUMP:
                if (cmd & 0x80) {
                    cmd |= 0xff00;
                }
                anim->script += cmd + 1;
                anim->step += cmd + 1;
                break;
            case MENU_ANIM_OP_NOP:
                anim->script++;
                anim->step++;
                break;
            }
        } else {
            anim->clipIndex = cmd;
            anim->startFrame = gMenuAnimClips[cmd].startFrame;
            running = 0;
            anim->duration = gMenuAnimClips[cmd].duration;
            anim->script++;
            anim->step++;
        }
    } while (running != 0);

    if (resetTimer != 0) {
        anim->timer = 0;
    }
}

/* returns no value */
int ReloadSelectFighterModels(int reloadP1, int reloadP2)
{
    int handMesh;
    int modelId;
    struct SceneNode *parts;

    if (reloadP1 || (reloadP2 && gSelectTwoPlayer))
        DrawSync(0);

    if (reloadP1 && gSelectCharIndex[0] != SELECT_SLOT_KATZE) {
        modelId = gSelectSlotToCharacter[gSelectCharIndex[0]];
        modelId += gSelectCostumeBank * PLAYABLE_CHARACTER_COUNT;
        parts = gSelectFighterParts[0] = LoadSingleFighterSlot(0, modelId, WEAPON_ID_NONE, 0, 0, 0);
        parts[FIGHTER_NODE_RIGHT_HAND].modelSlot = SELECT_HAND_MODEL_SLOT;
        parts[FIGHTER_NODE_LEFT_HAND].modelSlot = SELECT_HAND_MODEL_SLOT;
        handMesh = (modelId % PLAYABLE_CHARACTER_COUNT) * 2;
        parts[FIGHTER_NODE_RIGHT_HAND].meshIndex = handMesh;
        parts[FIGHTER_NODE_LEFT_HAND].meshIndex = handMesh + 1;
    }

    if (reloadP2 && gSelectTwoPlayer && gSelectCharIndex[1] != SELECT_SLOT_KATZE) {
        modelId = gSelectSlotToCharacter[gSelectCharIndex[1]];
        modelId += gSelectCostumeBank * PLAYABLE_CHARACTER_COUNT;
        parts = gSelectFighterParts[1] = LoadSingleFighterSlot(1, modelId, WEAPON_ID_NONE, 0, 0, 0);
        parts[FIGHTER_NODE_RIGHT_HAND].modelSlot = SELECT_HAND_MODEL_SLOT;
        parts[FIGHTER_NODE_LEFT_HAND].modelSlot = SELECT_HAND_MODEL_SLOT;
        handMesh = (modelId % PLAYABLE_CHARACTER_COUNT) * 2;
        parts[FIGHTER_NODE_RIGHT_HAND].meshIndex = handMesh;
        parts[FIGHTER_NODE_LEFT_HAND].meshIndex = handMesh + 1;
    }
}

unsigned int *LoadSelectAuxModel(unsigned int *cursor)
{
    unsigned int *result;
    struct SceneNode *parts;
    struct SceneNode *shadows;
    int i;
    int status;

    result = LoadCharacterArchive(cursor, MENU_ANIM_SLOT_HOST, CHAR_ARCHIVE_SELECT_HOST, SELECT_HOST_MODEL_SLOT,
                                  SELECT_HOST_SHADOW_SLOT, &status);
    SetupFighterSkeletonAndModel(MENU_ANIM_SLOT_HOST, CHAR_ARCHIVE_SELECT_HOST, &gSelectAuxParts[1],
                                 SELECT_HOST_MODEL_SLOT);
    parts = gSelectAuxParts;
    shadows = gSelectAuxShadows;
    parts[1].parent = parts;
    parts->rotOrder = SCENE_ROT_ORDER_FAST;
    parts->parent = 0;
    InitFighterShadowParts(&parts[1], shadows, SELECT_HOST_SHADOW_SLOT);
    for (i = SELECT_AUX_PART_COUNT - 1; i >= 0; i--) {
        gSelectAuxParts[i].lightSlot = LIGHT_SLOT_P1;
    }
    return result;
}

struct SceneNode *GetSelectAuxModelParts(void)
{
    return gSelectAuxParts;
}

MATRIX **GetSelectAuxMatrixTable(void)
{
    return gSelectAuxMatrixTable;
}

void EnqueueSelectAuxShadows(void)
{
    struct SceneNode *node;

    node = gSelectAuxShadows;
    while (node->meshIndex != SCENE_MESH_NONE) {
        *gRenderQueueCursor++ = node;
        node++;
    }
}

/* The first 17 entries of gAnimToSkeletonJointMap again:
 * skeleton joint i takes pose joint gShowcaseBoneOrder[i - 1]. */
int gShowcaseBoneOrder[17] = { 8, 9, 14, 15, 16, 17, 10, 11, 12, 13, 1, 2, 3, 4, 5, 6, 7 };

void ApplyMenuShowcasePose(int slot, struct SceneNode *parts)
{
    int yaw, radius;
    int cosineProduct;
    int i;
    PoseAngles pose[18];
    SVECTOR jointAngles;
    PoseAngles *decodedPose;

    DecodePoseFrame(gSelectAnimations[slot].startFrame + gSelectAnimations[slot].timer, pose, POSE_DECODE_WORK);
    decodedPose = pose;
    switch (slot) {
    case MENU_ANIM_SLOT_P1:
        parts[0].rotation.vx = 0;
        parts[0].rotation.vy = 0;
        parts[0].rotation.vz = 0;
        parts[0].localMatrix.t[0] = 900;
        parts[0].localMatrix.t[1] = 0;
        parts[0].localMatrix.t[2] = 900;
        parts[0].matrixState = SCENE_MATRIX_DIRTY;
        break;
    case MENU_ANIM_SLOT_P2:
        parts[0].localMatrix.t[0] = -900;
        parts[0].rotation.vx = 0;
        parts[0].rotation.vy = 0;
        parts[0].rotation.vz = 0;
        parts[0].localMatrix.t[1] = 0;
        parts[0].localMatrix.t[2] = 900;
        parts[0].matrixState = SCENE_MATRIX_DIRTY;
        break;
    case MENU_ANIM_SLOT_HOST:
        parts[0].rotation.vx = 0;
        parts[0].rotation.vy = 0;
        parts[0].rotation.vz = 0;
        parts[0].localMatrix.t[0] = 0;
        parts[0].localMatrix.t[1] = 0;
        parts[0].localMatrix.t[2] = 0;
        parts[0].matrixState = SCENE_MATRIX_DIRTY;
        break;
    }
    /* pose[0] stores root (height, yaw, radius); pose[1..17] store joint angles. */
    yaw = decodedPose->y;
    radius = decodedPose->z;
    parts[1].rotation.vx = 0;
    parts[1].rotation.vy = 0;
    parts[1].rotation.vz = 0;
    parts[1].localMatrix.t[0] = (rsin(yaw) * radius) >> 12;
    parts[1].localMatrix.t[1] = -decodedPose->x;
    cosineProduct = rcos(yaw) * radius;
    parts[1].matrixState = SCENE_MATRIX_DIRTY;
    parts[1].localMatrix.t[2] = -(cosineProduct >> 12);
    for (i = 1; i < 18; ++i) {
        jointAngles.vx = decodedPose[gShowcaseBoneOrder[i - 1]].x;
        jointAngles.vy = -decodedPose[gShowcaseBoneOrder[i - 1]].y;
        jointAngles.vz = -decodedPose[gShowcaseBoneOrder[i - 1]].z;
#ifdef VERSION_JP
        BuildRotMatrixZYX(&jointAngles, &parts[i + 1].localMatrix);
#else
        BuildRotationMatrixFast(&jointAngles, &parts[i + 1].localMatrix);
#endif
        parts[i + 1].matrixState = SCENE_MATRIX_LOCAL_PRECOMPUTED;
    }
}

void FinalizeCharacterWeaponSelect(int *p1CharacterId, int *p2CharacterId, int *p1WeaponId, int *p2WeaponId)
{
    DrawSync(0);
    *p1CharacterId = gSelectSlotToCharacter[gSelectCharIndex[0]];
    *p2CharacterId = gSelectSlotToCharacter[gSelectCharIndex[1]];
    *p1WeaponId = gSelectSlotToWeapon[gSelectWeaponSlotP1];
    *p2WeaponId = gSelectSlotToWeapon[gSelectWeaponSlotP2];
    if (IsCharFogSlotActive(0) || IsCharFogSlotActive(1)) {
        InvalidateArenaGroup(ARENA_GROUP_P1_CHARACTER);
        InvalidateArenaGroup(ARENA_GROUP_P2_CHARACTER);
        ReloadSelectFighterModels(1, 1);
    }
    ClearCachedResources();
    SetFighterClothDirty(0, 1);
    SetFighterClothDirty(1, 1);
    SetRendererDaytime(GetDaytimeOption());
}

int GetSelectedStageIndex(void)
{
    int stageIndex;

    stageIndex = gSelectStageIndex;
    if (stageIndex < 0) {
        stageIndex = -1;
    }
    return stageIndex;
}

void SetDefaultSelectIndices(int characterId, int weaponId)
{
    int charSlot;
    int weaponSlot;

    for (charSlot = 0; charSlot < PLAYABLE_CHARACTER_COUNT; charSlot++) {
        if (gSelectSlotToCharacter[charSlot] == characterId) {
            break;
        }
    }
    if (charSlot == PLAYABLE_CHARACTER_COUNT) {
        TriggerDebugTrap();
    }
    gDefaultSelection[0][0] = charSlot;

    for (weaponSlot = 0; weaponSlot < SELECT_WEAPON_SLOT_COUNT; weaponSlot++) {
        if (gSelectSlotToWeapon[weaponSlot] == weaponId) {
            break;
        }
    }
    if (weaponSlot == SELECT_WEAPON_SLOT_COUNT) {
        TriggerDebugTrap();
    }
    gDefaultSelection[0][1] = weaponSlot;
}
