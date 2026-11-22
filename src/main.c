/* System boot, hardware/SDK initialization, default game options, VSync rumble hook, and the main loop. */

#include <sys/types.h>
#include <stdio.h>
#include <kernel.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <rand.h>

#include "api_int.h"
#include "cd.h"
#include "combat_state.h"
#include "display_buffer.h"
#include "game_state.h"
#include "link.h"
#include "main.h"
#include "memcard.h"
#include "memcard_io.h"
#include "menu_state.h"
#include "camera.h"
#include "opening_state.h"
#include "pad.h"
#include "replay.h"
#include "scene.h"
#include "sound.h"
#include "stage_tables.h"
#include "story_state.h"
#include "timer.h"
#include "load_area.h"
#include "globals.h"
#include "splash.h"

void SetBackgroundClear(int clearBackground, int red, int green, int blue)
{
    int i;

    for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
        db[i].draw.isbg = clearBackground;
        setRGB0(&db[i].draw, red, green, blue);
    }
}

short gGameState;

void RunBootVersionState(void)
{
#if defined(VERSION_JP)
    printf("--- BUSHIDO BLADE VER.1.1 ---\n");
#elif defined(VERSION_EU)
    printf("--- BUSHIDO BLADE VER.1.1d ---\n");
    printf("EUROPE VERSION\n");
#else
    printf("--- BUSHIDO BLADE VER.1.1c ---\n");
    printf("USA VERSION\n");
#endif
    gGameState = GAME_STATE_INIT_BOOT_AUTO_LOAD;
}

void RumbleVSyncCallback(void)
{
    UpdateControllerRumble();
}

short gOptionDaytime; /* 1=day, 0=night; save bit 5, Options row 5 */
short gOptionContinueEnabled; /* 1=on, 0=off; save bit 4, Options row 4 */
short gKatzeUnlocked; /* 0=locked, 1=unlocked; save bit 8 */
short gOptionMonochrome; /* 0=off, 1=on; save bit 3, Options row 3 */
short gOptionAutoSave; /* 0=off, 1=on; save bit 6, Options row 7 */
short gOptionRumbleEnabled; /* 1=on, 0=off; save bit 7 */
short gOptionWallBump; /* 0=off, 1=on; save bit 1, Options row 1 */
short gDemoFighterPairIndex; /* 0..5 cycle index into gDemoFighterPairs */
int gUnlockedMoviesMask;   /* 12-bit ending mask (bits 0..5 normal, 6..11 true); save bits 16..31 */
short gOptionBgmEnabled; /* 1=on, 0=off; save bit 2, Options row 2 */
short gOptionDifficulty; /* 1=hard, 0=easy; save bit 0, Options row 0 */
short gDemoStageIndex; /* 0..10 cycle index into gDemoStageOrder */

static void ResetGameOptions(void)
{
    gOptionDifficulty = 1;
    gOptionWallBump = 0;
    gOptionBgmEnabled = 1;
    gOptionMonochrome = 0;
    gOptionContinueEnabled = 1;
#ifdef VERSION_JP
    gOptionDaytime = 0;
#else
    gOptionDaytime = 1;
#endif
    gOptionAutoSave = 0;
    gKatzeUnlocked = 0;
    gUnlockedMoviesMask = 0;
#ifdef VERSION_JP
    gOptionRumbleEnabled = 1;
#else
    gOptionRumbleEnabled = 0;
#endif
    gDemoFighterPairIndex = 0;
    gDemoStageIndex = 0;
}

#ifdef VERSION_JP
#define SEED_CHECKSUM_QUADS 0xb400

/* Seeds the random generator with a checksum of the fighters' load area. Nothing calls it. */
static void SeedRandomFromLoadArea(void)
{
    unsigned long sum = 0;
    unsigned long *wordPtr = DYNAMIC_LOAD_BUFFER;
    unsigned int blockIndex;

    for (blockIndex = 0; blockIndex < SEED_CHECKSUM_QUADS; blockIndex++) {
        sum += *wordPtr++;
        sum ^= *wordPtr++;
        sum -= *wordPtr++;
        sum ^= *wordPtr++;
    }
    srand(sum);
}
#endif

/* 16 KB work buffer shared by the replay, memory card I/O, character select and subtitles. */
#define SHARED_WORK_BUFFER_ADDR ((unsigned char *)0x80148000)
#define PAD_TRANSFER_LEN 8

RECT gDisplayClearRect = { 0, 0, DISPLAY_WIDTH, DISPLAY_VRAM_HEIGHT };
/* 4 bytes and a pointer to them; nothing reads either. */
char gUnusedStringFormat[4] = "%s";
char *gUnusedStringFormatPtr = gUnusedStringFormat;
StageTransition *gStageTransitionsPtr;
unsigned char *gSharedWorkBuffer;
short gDisplayPresentEnabled;
short gAllMenuModesEnabled;

static void InitSystem(void)
{
    ResetCallback();
    InitPAD(gPadReceiveBuffers[0], PAD_TRANSFER_LEN, gPadReceiveBuffers[1], PAD_TRANSFER_LEN);
    gOptionRumbleEnabled = 0;
    gRumbleOutputPackets[0][0] = gRumbleOutputPackets[1][0] = 0;
    SendPAD(gRumbleOutputPackets[0], PAD_TRANSFER_LEN, gRumbleOutputPackets[1], PAD_TRANSFER_LEN);
    StartPAD();
    ChangeClearPAD(0);
    ResetGraph(0);
    SetGraphDebug(0);
    SetDispMask(0);
    InitCdSubsystem();
    InitGeom();
    SetGeomOffset(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2);
    SetGeomScreen(CalcGeomScreenFromFov(DEFAULT_SCENE_FOV));
    SetDefDrawEnv(&db[0].draw, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#ifdef VERSION_EU
    db[1].disp.screen.x = db[0].disp.screen.x = 0;
    db[1].disp.screen.y = db[0].disp.screen.y = DISPLAY_SCREEN_Y;
#endif
    DrawSync(0);
    ClearImage(&gDisplayClearRect, 0, 0, 0);
    DrawSync(0);
    gDisplayPresentEnabled = 1;
    InitMemoryCard();
    InitControllerState();
    SetBackgroundClear(1, 0, 0, 0);
    Init3dEngine();
    InitHBlankRootCounter();
    gSharedWorkBuffer = SHARED_WORK_BUFFER_ADDR;
    gStageTransitionsPtr = GetStageTransitionTable();
    ResetGameOptions();
    gAllMenuModesEnabled = 1;
}

void WaitGpuIdle(void)
{
    DrawSync(0);
}

void (*gGameStateHandlers[GAME_STATE_COUNT])() = {
    RunBootVersionState, InitCombatState, UpdateCombatState,
    InitOpeningState, UpdateOpeningState, InitDemoBattle,
    UpdateDemoBattle, InitMainMenuState, UpdateMainMenuState,
    InitCharacterSelect, UpdateCharacterSelect, GameState11NoOp,
    GameState12NoOp, InitSlashClearCutsceneState, UpdateSlashClearCutsceneState,
    GameState15NoOp, GameState16NoOp, InitBattleReplay,
    UpdateBattleReplay, InitVictorySceneState, UpdateVictorySceneState,
    InitLinkLobbyState, UpdateLinkLobbyState, InitOptionsMenu,
    UpdateOptionsMenu, InitStoryCutsceneState, UpdateStoryCutsceneState,
    InitContinueScreenState, UpdateContinueScreenState, PlayStoryEndingMovieState,
    WaitCancelToCreditsState, InitAutoSaveState, UpdateAutoSaveState,
    InitBootAutoLoadState, UpdateBootAutoLoadState, InitBadEndingSlideState,
    UpdateBadEndingSlideState, InitStoryPrologueState, UpdateStoryPrologueState,
    InitCreditsState, UpdateCreditsState, HandleLinkAbortState,
    WaitCrossToMenuState, InitWaitCancelToMenuState, UpdateWaitCancelToMenuState,
    InitAnalogControllerMenuState, UpdateAnalogControllerMenuState, InitApproachCutsceneState,
    UpdateApproachCutsceneState, InitMovieTheaterState, UpdateMovieTheaterState,
};

int gFrameCounter;
u_long *gCurrentOt;

void main(void)
{
    DisplayBuffer *displayBuffer;

#ifdef VERSION_JP
    SetSp(0x801fff00);
    SetMem(2);
#endif
#ifdef VERSION_EU
    SetVideoMode(MODE_PAL);
#endif
    InitSystem();
#ifndef VERSION_JP
    PlayBootSplashScreen();
#endif
    VSyncCallback(RumbleVSyncCallback);
    SetDispMask(1);
    gGameState = GAME_STATE_BOOT_VERSION;
    gFrameCounter = 0;
    for (;;) {
        PollControllers();
        displayBuffer = &db[gFrameCounter & 1];
        ClearOTagR(displayBuffer->ot, DISPLAY_OT_LEN);
        gCurrentOt = displayBuffer->ot;
        UpdateCdSubsystem();
        TickSoundVoiceCooldowns();
        if (gLinkErrorFlag != 0) {
            gGameState = GAME_STATE_LINK_ABORT;
        }
        gGameStateHandlers[gGameState]();
        while (GetRCnt(RCntCNT1) < FRAME_PACING_HBLANKS) {
            rand();
        }
        VSync(1);
        DrawSync(0);
        VSync(0);
        ResetRCnt(RCntCNT1);
        if (gDisplayPresentEnabled != 0) {
            PutDispEnv(&displayBuffer->disp);
            PutDrawEnv(&displayBuffer->draw);
            DrawOTag(&displayBuffer->ot[DISPLAY_OT_LEN - 1]);
            gFrameCounter++;
        }
    }
}
