/* Game-state handlers for the front end: Main Menu, Character Select, Options Menu and the
 * attract-mode Demo Battle (gGameState 5-12, 23-24). */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <rand.h>

#include "fighter.h"
#include "game_state.h"
#include "link.h"
#include "menu_state.h"
#include "motion_file.h"
#include "options_menu.h"
#include "camera.h"
#include "fog.h"
#include "cd.h"
#include "pose_stream.h"
#include "fighter_pose.h"
#include "stage_cull.h"
#include "fighter_cloth.h"
#include "billboard_driver.h"
#include "collision.h"
#include "combat_state.h"
#include "fighter_ai.h"
#include "font_hud.h"
#include "main.h"
#include "main_menu_ui.h"
#include "match_setup.h"
#include "memcard.h"
#include "model_arena.h"
#include "options_ui.h"
#include "scene.h"
#include "sightline.h"
#include "sound.h"
#include "subweapon.h"
#include "select_scene.h"
#include "load_area.h"
#include "stage_id.h"
#include "weapon_id.h"
#include "globals.h"
#include "pad.h"

#ifdef VERSION_JP
#define BOTH_PADS_OPTIONS_EXIT BOTH_PADS_CANCEL
#else
#define BOTH_PADS_OPTIONS_EXIT BOTH_PADS(MENU_CANCEL | PADRup)
#endif
#define PAD_P2_START _PAD(1, PADstart)

#define MENU_SOUND_NONE 0xff
#define MENU_SOUND_VOLUME 0x60
#define CHAR_SELECT_SOUND_BASE 0x71

#define HUD_BANNER_CHALLENGER_SELECT 2
#define MAIN_MENU_ATTRACT_TIMEOUT 900
#define CHALLENGER_SELECT_BANNER_FRAMES 91

/* Return values from UpdateCharacterSelectFrame. */
typedef enum SelectFrameResult {
    SELECT_RESULT_DONE = 0,
    SELECT_RESULT_ACTIVE = 1,
    SELECT_RESULT_P1_CONFIRMED = 2
} SelectFrameResult;

void PlayMenuSound(int soundIndex)
{
    unsigned char soundId;

    soundId = gMenuSoundIds[soundIndex];
    if (soundId != MENU_SOUND_NONE) {
        QueueSoundEffect(soundId, MENU_SOUND_VOLUME, MENU_SOUND_VOLUME);
    }
}

void PlayCharacterSelectSound(int soundIndex)
{
    QueueSoundEffect(soundIndex + CHAR_SELECT_SOUND_BASE, MENU_SOUND_VOLUME, MENU_SOUND_VOLUME);
}

/* The cursor wraps after entry 7 (8 with a Japanese analog pad): entries 9..11 are never selected. */
MainMenuEntry gMainMenuEntries[MAIN_MENU_ENTRY_COUNT] = {
    { PLAY_MODE_STORY, 0, GAME_STATE_INIT_CHARACTER_SELECT, 1, 1 },
    { PLAY_MODE_VS, 0, GAME_STATE_INIT_CHARACTER_SELECT, 2, 1 },
    { PLAY_MODE_SLASH, 0, GAME_STATE_INIT_CHARACTER_SELECT, 1, 1 },
    { PLAY_MODE_PRACTICE, 0, GAME_STATE_INIT_CHARACTER_SELECT, 1, 1 },
    { PLAY_MODE_VS, 1, GAME_STATE_INIT_CHARACTER_SELECT, 1, 1 },
    { PLAY_MODE_LINK, 0, GAME_STATE_INIT_LINK_LOBBY, 2, 0 },
    { PLAY_MODE_STORY, 0, GAME_STATE_INIT_MOVIE_THEATER, 2, 1 },
    { PLAY_MODE_STORY, 0, GAME_STATE_INIT_OPTIONS_MENU, 2, 1 },
    { PLAY_MODE_STORY, 0, GAME_STATE_INIT_ANALOG_MENU, 2, 1 },
    { PLAY_MODE_VS, 1, GAME_STATE_INIT_CHARACTER_SELECT, 2, 0 },
    { PLAY_MODE_PRACTICE, 0, GAME_STATE_INIT_CHARACTER_SELECT, 2, 0 },
    { PLAY_MODE_STORY, 0, GAME_STATE_INIT_CHARACTER_SELECT, 1, 0 },
};
/* Indexed by play mode (0..4): 0 = Story/Slash costume bank, 1 = VS/Practice/Link; nothing reads entries 5 to 7. */
unsigned char gModeCharSelectLayout[] = { 0, 1, 1, 0, 1, 0, 0, 0 };
short gCharSelectMenuType;        /* SelectGameMode (0..7) */
short gCharSelectLayout;          /* 0 = Story/Slash costume bank, 1 = VS/Practice/Link */

short gMainMenuSelection = 0;

void InitMainMenuState(void)
{
    int heldButtons;

    SetGeomScreen(CalcGeomScreenFromFov(DEFAULT_SCENE_FOV));
    if (gAllMenuModesEnabled < 0) { /* never true: InitSystem sets it to 1 and only this branch writes it again */
        heldButtons = gPadState.held;
        gAllMenuModesEnabled = (heldButtons & BOTH_PADS(PADL1)) && (heldButtons & BOTH_PADS(PADR1)) &&
                               (heldButtons & BOTH_PADS(PADR2));
    }
    RequestLinkDisconnect();
    StopAndWaitCdIdle();
    gIsContinueRestart = 0;
    gContinueCount = 0;
    gStoryChallengerActive = 0;
    gStoryChallengerTimer = 0;
    gStoryProloguePlayed = 0;
    gStoryEventResult = 0;
    gPracticeFirstBoutFlag = 1;
    gPlayMode = PLAY_MODE_STORY;
    LoadMenuSoundBanks();
    DrawSync(0);
    InitMainMenu(gMainMenuSelection);
    gStateFrameTimer = 0;
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_MAIN_MENU;
}

void UpdateMainMenuState(void)
{
    int move;
    int confirm;
    int port;
    int confirmMask;
    MainMenuEntry *entry;
#ifdef VERSION_JP
    int analog;
#endif

    gDisplayPresentEnabled = 1;
    rand();
    gStateFrameTimer++;
    if (gPadState.held != 0) {
        gStateFrameTimer = 0;
    }

    confirm = 0;
    move = 0;
    for (port = 0; port < 2; port++) {
        if (port == 0 || move == 0) {
            move = 0;
            if (gPadState.pressed & (PADLdown << (port << 4))) {
                move++;
            }
            if (gPadState.pressed & (PADLup << (port << 4))) {
                move--;
            }
        }
        confirm |= (gPadState.pressed & (MENU_CONFIRM << (port << 4))) != 0;
    }

    if (move == -1) {
        PlayMenuSound(MAIN_MENU_SOUND_CURSOR_UP);
    }
    if (move == 1) {
        PlayMenuSound(MAIN_MENU_SOUND_CURSOR_DOWN);
    }
#ifdef VERSION_JP
    analog = 0;
    if (IsPadAnalog(0) || IsPadAnalog(1)) {
        analog = 1;
    }
    gMainMenuSelection = UpdateAndDrawMainMenu(move, confirm, analog);
#else
    gMainMenuSelection = UpdateAndDrawMainMenu(move, confirm, 0);
#endif
    entry = &gMainMenuEntries[gMainMenuSelection];

    if (gPadState.pressed & BOTH_PADS_CONFIRM) {
        if (entry->playMode == PLAY_MODE_LINK) {
            confirmMask = MENU_CONFIRM;
#ifndef VERSION_JP
        } else if (entry->playMode == PLAY_MODE_VS && entry->povMode == 0) {
            confirmMask = BOTH_PADS_CONFIRM;
            if (gPadState.connected[0] == 0 || gPadState.connected[1] == 0) {
                confirmMask = 0;
            }
#endif
        } else if (entry->playerCount == 1) {
            confirmMask = MENU_CONFIRM;
        } else {
            confirmMask = BOTH_PADS_CONFIRM;
        }

        if ((gPadState.pressed & confirmMask) && (entry->alwaysUnlocked != 0 || gAllMenuModesEnabled != 0)) {
            PlayMenuSound(MAIN_MENU_SOUND_CONFIRM);
            gPlayMode = entry->playMode;
            gPovModeEnabled = entry->povMode;
            gHumanPlayerCount = entry->playerCount;
            gGameState = entry->nextState;
            if (gPlayMode == PLAY_MODE_VS || gPlayMode == PLAY_MODE_PRACTICE) {
                gVsWinCounts[0] = 0;
                gVsWinCounts[1] = 0;
                gPracticeWinRecorded = 0;
            }
            gAiMoveCycleMode = (gMainMenuSelection == MAIN_MENU_ENTRY_AI_CYCLE);
        } else {
            PlayMenuSound(MAIN_MENU_SOUND_REJECT);
        }
    } else {
        if (gStateFrameTimer > MAIN_MENU_ATTRACT_TIMEOUT) {
            gGameState = GAME_STATE_INIT_OPENING;
        }
    }
    FlushQueuedSounds();
}

void InitCharacterSelect(void)
{
    int playMode;

    BeginLinkSyncPause();
    WaitGpuIdle();
    ShowNowLoadingOverlay();
    playMode = gPlayMode;
    gCharSelectLayout = gModeCharSelectLayout[playMode];
    switch (playMode) {
    case PLAY_MODE_STORY:
        if (gStoryChallengerTimer != 0) {
            LoadMenuSoundBanks();
            gStoryChallengerTimer = 0;
            gStoryChallengerActive = 1;
            gHumanPlayerCount = 2;
            SetDefaultSelectIndices((short)(gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT), gSelectedWeapon[0]);
        }
        gCharSelectMenuType = (gStoryChallengerActive != 0) ? SELECT_MODE_STORY_CHALLENGER : SELECT_MODE_STORY;
        break;
    case PLAY_MODE_VS:
        if (gPovModeEnabled == 1)
            gCharSelectMenuType = SELECT_MODE_POV_VS;
        else
            gCharSelectMenuType = gKatzeUnlocked ? SELECT_MODE_VS_KATZE : SELECT_MODE_VS;
        break;
    case PLAY_MODE_PRACTICE:
        gCharSelectMenuType = SELECT_MODE_PRACTICE;
        break;
    case PLAY_MODE_LINK:
        gCharSelectMenuType = SELECT_MODE_LINK_VS;
        break;
    case PLAY_MODE_SLASH:
        gCharSelectMenuType = SELECT_MODE_SLASH;
        break;
    }
    InitCharacterSelectScene(gCharSelectLayout, gCharSelectMenuType, DYNAMIC_LOAD_BUFFER,
                             (struct SceneNode *)gSharedWorkBuffer);
    EnableCdSpuAudio();
    StartXaTrack(XA_TRACK_SELECT);
    SetCdAudioRepeat(1);
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_CHARACTER_SELECT;
    WaitLinkSyncBarrier();
}

static void ResetStoryProgress(void)
{
    gCurrentStageId = 0;
    gPrevStageId = 0;
    gBushidoHonorIntact = 1;
    gStoryNoDamageFlag = 1;
    gStoryBoutIndex = 0;
    gStoryFallCount = 0;
    gReachedStage9Flag = 0;
    gStoryDefeatedMask = 0;
    gFighterInjuredRegions[0] = 0;
    gFighterInjuredRegions[1] = 0;
    gFighterAltTextureLoaded[0] = 0;
    gFighterAltTextureLoaded[1] = 0;
    gStoryStage9FallSkip = 0;
}

short gOptionsMemoryCardMsg;    /* active Options memory-card message ID (-1 = hidden) */
short gOptionsSubScreen;        /* Options sub-screen (0 = main, 1 = card status, 2 = confirm) */
short gOptionsMemoryCardAction; /* Options row 6 Save/Load selection */
short gOptionsCursor;           /* active Options row (0..7) */

/* Stage IDs selectable in Link, VS, POV VS, and Practice character select. */
unsigned char gLinkStageSelectTable[8] = {
    STAGE_ID_WARRIORS_DWELLING, STAGE_ID_YAHIRO_ROAD, STAGE_ID_BAMBOO_THICKET, STAGE_ID_CHERRY_BLOSSOM_GROVE, 7,
    STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN, 0
};
unsigned char gVsStageSelectTable[8] = {
    STAGE_ID_YAGURA_POINT, STAGE_ID_YAHIRO_ROAD, STAGE_ID_UNDERGROUND_GROTTO, STAGE_ID_BAMBOO_THICKET,
    STAGE_ID_CHERRY_BLOSSOM_GROVE, STAGE_ID_DOZAEMON_MOAT, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN
};
unsigned char gPovStageSelectTable[8] = {
    STAGE_ID_YAGURA_POINT, STAGE_ID_YAHIRO_ROAD, STAGE_ID_UNDERGROUND_GROTTO, STAGE_ID_BAMBOO_THICKET,
    STAGE_ID_CHERRY_BLOSSOM_GROVE, 7, STAGE_ID_EXECUTIONERS_COVE, STAGE_ID_MEIKYOKAN
};
unsigned char gPracticeStageSelectTable[4] = {
    STAGE_ID_MEIKYOKAN, STAGE_ID_CHERRY_BLOSSOM_GROVE, STAGE_ID_EXECUTIONERS_COVE, 0
};

void UpdateCharacterSelect(void)
{
    int i;
    int shift;
    int horizontal;
#ifndef VERSION_JP
    int vertical;
#endif
    int selectResult;
    struct {
        int horizontal[2];
        int confirm[2];
#ifndef VERSION_JP
        int vertical[2];
#endif
    } padInput;
    int p1SelectedChar;
    int p2SelectedChar;
    int p1SelectedWeapon;
    int p2SelectedWeapon;

    gDisplayPresentEnabled = 1;
    rand();
    if (gStoryChallengerTimer > 0) {
        padInput.confirm[1] = 0;
        padInput.confirm[0] = 0;
        padInput.horizontal[1] = 0;
        padInput.horizontal[0] = 0;
#ifndef VERSION_JP
        padInput.vertical[1] = 0;
        padInput.vertical[0] = 0;
#endif
        DrawHudBanner(HUD_BANNER_CHALLENGER_SELECT);
    } else {
        for (i = 0; i < 2; i++) {
            shift = i << 4;
            horizontal = 0;
            if (gPadState.pressed & (PADLright << shift))
                horizontal++;
            if (gPadState.pressed & ((unsigned int)PADLleft << shift))
                horizontal--;
            padInput.horizontal[i] = horizontal;
#ifndef VERSION_JP
            vertical = 0;
            if (gPadState.pressed & (PADLdown << shift))
                vertical++;
            if (gPadState.pressed & (PADLup << shift))
                vertical--;
            padInput.vertical[i] = vertical;
#endif
            padInput.confirm[i] = (gPadState.pressed & (MENU_CONFIRM << shift)) != 0;
        }
    }
#ifdef VERSION_JP
    selectResult = UpdateCharacterSelectFrame(padInput.horizontal[0], padInput.horizontal[1],
                                              padInput.confirm[0], padInput.confirm[1]);
#else
    selectResult = UpdateCharacterSelectFrame(padInput.horizontal[0], padInput.horizontal[1],
                                              padInput.confirm[0], padInput.confirm[1],
                                              padInput.vertical[0], padInput.vertical[1]);
#endif
    if (selectResult == SELECT_RESULT_DONE) {
        if (gPovModeEnabled == 1) {
            if (gPlayMode == PLAY_MODE_LINK) {
                EnableCharFogSlot(gLinkRole == LINK_ROLE_HOST);
            } else {
                EnableCharFogSlot(1);
            }
        } else {
            DisableCharFogSlot();
        }
        FinalizeCharacterWeaponSelect(&p1SelectedChar, &p2SelectedChar, &p1SelectedWeapon, &p2SelectedWeapon);
        i = 1 - gCharSelectLayout;
        gSelectedFighter[0] = (short)(p1SelectedChar + i * PLAYABLE_CHARACTER_COUNT);
        gSelectedWeapon[0] = p1SelectedWeapon;
        if (gPlayMode == PLAY_MODE_STORY) {
            if (gStoryChallengerActive == 0) {
                ResetStoryProgress();
                gGameState = GAME_STATE_INIT_STORY_PROLOGUE;
            } else {
                gGameState = GAME_STATE_INIT_COMBAT;
                gSelectedFighter[1] = (short)(p2SelectedChar + i * PLAYABLE_CHARACTER_COUNT);
                gSelectedWeapon[1] = p2SelectedWeapon;
            }
        } else if (gPlayMode == PLAY_MODE_SLASH) {
            gSlashLevelIndex = 0;
            gSlashNeedsEnemyPreload = 1;
            gSelectedWeapon[0] = WEAPON_ID_KATANA;
            gCurrentStageId = STAGE_ID_SLASH;
            gPrevStageId = STAGE_ID_SLASH;
            gGameState = GAME_STATE_INIT_COMBAT;
        } else if (gPlayMode == PLAY_MODE_PRACTICE) {
            int stage;

            gSelectedFighter[1] = (short)(p2SelectedChar + i * PLAYABLE_CHARACTER_COUNT);
            gSelectedWeapon[1] = p2SelectedWeapon;
            stage = gPracticeStageSelectTable[GetSelectedStageIndex()];
            gGameState = GAME_STATE_INIT_COMBAT;
            gCurrentStageId = stage;
            gPrevStageId = stage;
        } else if (gPlayMode == PLAY_MODE_VS) {
            int stage;

            if (gPovModeEnabled == 1) {
                stage = gPovStageSelectTable[GetSelectedStageIndex()];
            } else {
                stage = gVsStageSelectTable[GetSelectedStageIndex()];
            }
            gCurrentStageId = stage;
            gPrevStageId = stage;
            gGameState = GAME_STATE_INIT_COMBAT;
            gSelectedFighter[1] = (short)(p2SelectedChar + i * PLAYABLE_CHARACTER_COUNT);
            gSelectedWeapon[1] = p2SelectedWeapon;
        } else if (gPlayMode == PLAY_MODE_LINK) {
            int stage;

            gSelectedFighter[1] = (short)(p2SelectedChar + i * PLAYABLE_CHARACTER_COUNT);
            gSelectedWeapon[1] = p2SelectedWeapon;
            stage = gLinkStageSelectTable[GetSelectedStageIndex()];
            gGameState = GAME_STATE_INIT_COMBAT;
            gCurrentStageId = stage;
            gPrevStageId = stage;
        }
    } else if (gPlayMode == PLAY_MODE_STORY && gStoryChallengerActive == 0) {
        if (gStoryChallengerTimer > 0) {
            if (++gStoryChallengerTimer >= CHALLENGER_SELECT_BANNER_FRAMES) {
                ResetStoryProgress();
                gStoryChallengerActive = 1;
                gStoryChallengerTimer = 0;
                gHumanPlayerCount = 2;
                WaitGpuIdle();
                ResetCharacterSelectState(SELECT_PLAYERS_2P);
                gDisplayPresentEnabled = 0;
            }
        } else if ((gPadState.pressed & PAD_P2_START) != 0 && selectResult != SELECT_RESULT_P1_CONFIRMED) {
            gStoryChallengerTimer = 1;
        }
    }
    FlushQueuedSounds();
    if (gGameState != GAME_STATE_UPDATE_CHARACTER_SELECT) {
        StopAndWaitCdIdle();
    }
}

/* No code enters states 11 and 12. */
void GameState11NoOp(void)
{
}

void GameState12NoOp(void)
{
}

void InitOptionsMenu(void)
{
    gOptionsSubScreen = OPTIONS_SCREEN_MAIN;
    gOptionsCursor = OPTIONS_ROW_DIFFICULTY;
    gOptionsMemoryCardAction = 0;
    gOptionsMemoryCardMsg = OPTIONS_MSG_NONE;
    DrawSync(0);
    LoadOptionsMenuAssets();
    gStateFrameTimer = 0;
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_OPTIONS_MENU;
}

int GetMonochromeOption(void)
{
    return gOptionMonochrome;
}

int GetDaytimeOption(void)
{
    int daytime;

    if (gPlayMode == PLAY_MODE_LINK && gLinkRole == LINK_ROLE_CLIENT) {
        daytime = GetLinkPeerDaytimeOption();
    } else {
        daytime = gOptionDaytime;
    }
    return daytime;
}

/* The options menu rows: each points at its option variable. */
short *gOptionValuePtrs[OPTIONS_ROW_COUNT] = {
    &gOptionDifficulty, &gOptionWallBump, &gOptionBgmEnabled, &gOptionMonochrome,
    &gOptionContinueEnabled, &gOptionDaytime, &gOptionsMemoryCardAction, &gOptionAutoSave,
};
unsigned char gMemoryCardStatusToMsg[MEMORY_CARD_STATUS_COUNT] = {
    OPTIONS_MSG_NONE_BYTE,
    OPTIONS_MSG_SAVING,
    OPTIONS_MSG_SAVE_OK,
    OPTIONS_MSG_SAVE_ERROR,
    OPTIONS_MSG_LOADING,
    OPTIONS_MSG_LOAD_OK,
    OPTIONS_MSG_LOAD_ERROR,
    OPTIONS_MSG_NO_FREE_BLOCKS,
    OPTIONS_MSG_NO_CARD,
    OPTIONS_MSG_FORMATTING,
    OPTIONS_MSG_FORMAT_CONFIRM,
    OPTIONS_MSG_FORMAT_OK,
    OPTIONS_MSG_FORMAT_ERROR,
    OPTIONS_MSG_OVERWRITE_CONFIRM,
    OPTIONS_MSG_NO_DATA,
    OPTIONS_MSG_CORRUPT_DATA,
};
short gOptionsConfirmCursor;  /* OptionsConfirmChoice */
short gOptionsCardSoundState;

void UpdateOptionsMenu(void)
{
#ifndef VERSION_JP
    int row;
    short *options[OPTIONS_ROW_COUNT];
    short nightMode;
#endif
    int screen;
    int cardStatus;

    gDisplayPresentEnabled = 1;
    gStateFrameTimer++;
#ifdef VERSION_JP
    DrawOptionsMenu(gOptionsCursor, gOptionValuePtrs,
                    gOptionsMemoryCardMsg, gOptionsConfirmCursor);
#else
    for (row = 0; row < OPTIONS_ROW_COUNT; row++) {
        options[row] = gOptionValuePtrs[row];
    }
    /* DrawOptionsMenu uses gOptionsInvertMask[5] == 0, so invert Daytime for its Day/Night sprite pair. */
    nightMode = (gOptionDaytime == 0);
    options[OPTIONS_ROW_DAYTIME] = &nightMode;
    DrawOptionsMenu(gOptionsCursor, options,
                    gOptionsMemoryCardMsg, gOptionsConfirmCursor);
#endif

    screen = gOptionsSubScreen;
    if (screen == OPTIONS_SCREEN_MAIN) {
#ifdef VERSION_JP
        /* The Japanese menu steps rows with left and right and toggles a row with down. */
        if (gPadState.pressed & BOTH_PADS(PADLright)) {
            ++gOptionsCursor;
            PlayMenuSound(OPTIONS_SOUND_CURSOR_DOWN);
        } else if (gPadState.pressed & BOTH_PADS(PADLleft)) {
            --gOptionsCursor;
            PlayMenuSound(OPTIONS_SOUND_CURSOR_UP);
        }
        gOptionsCursor &= OPTIONS_ROW_COUNT - 1;
        if (gPadState.pressed & BOTH_PADS(PADLdown)) {
            *gOptionValuePtrs[gOptionsCursor] ^= 1;
            PlayMenuSound(OPTIONS_SOUND_TOGGLE);
        }
#else
        if (gPadState.pressed & BOTH_PADS(PADLdown)) {
            ++gOptionsCursor;
            PlayMenuSound(OPTIONS_SOUND_CURSOR_DOWN);
        } else if (gPadState.pressed & BOTH_PADS(PADLup)) {
            --gOptionsCursor;
            PlayMenuSound(OPTIONS_SOUND_CURSOR_UP);
        }
        gOptionsCursor &= OPTIONS_ROW_COUNT - 1;
        if (gPadState.pressed & BOTH_PADS(PADLleft)) {
            *gOptionValuePtrs[gOptionsCursor] =
                (OPTIONS_LEFT_VALUE_MASK >> gOptionsCursor) & 1;
            PlayMenuSound(OPTIONS_SOUND_TOGGLE);
        }
        if (gPadState.pressed & BOTH_PADS(PADLright)) {
            *gOptionValuePtrs[gOptionsCursor] =
                (~OPTIONS_LEFT_VALUE_MASK >> gOptionsCursor) & 1;
            PlayMenuSound(OPTIONS_SOUND_TOGGLE);
        }
#endif
        if ((gPadState.pressed & BOTH_PADS_CONFIRM) &&
            gOptionsCursor == OPTIONS_ROW_MEMORY_CARD) {
            gOptionsCardSoundState = OPTIONS_CARD_SOUND_PENDING;
            if (gOptionsMemoryCardAction == OPTIONS_CARD_ACTION_LOAD) {
                PlayMenuSound(OPTIONS_SOUND_START_LOAD);
                gOptionsSubScreen = OPTIONS_SCREEN_CARD_STATUS;
                BeginMemoryCardLoad();
            } else {
                PlayMenuSound(OPTIONS_SOUND_START_SAVE);
                gOptionsSubScreen = OPTIONS_SCREEN_CARD_STATUS;
                BeginMemoryCardSave();
            }
        }
        if (gPadState.pressed & BOTH_PADS_OPTIONS_EXIT) {
            PlayMenuSound(OPTIONS_SOUND_EXIT);
            if (gOptionAutoSave != 0) {
                gAutoSaveReturnState = GAME_STATE_INIT_MAIN_MENU;
                gGameState = GAME_STATE_INIT_AUTO_SAVE;
            } else {
                gGameState = GAME_STATE_INIT_MAIN_MENU;
            }
        }
    } else if (screen == OPTIONS_SCREEN_CARD_STATUS) {
        cardStatus = UpdateMemoryCard();
        gOptionsMemoryCardMsg = gMemoryCardStatusToMsg[cardStatus];
        switch (cardStatus) {
        case MEMORY_CARD_STATUS_SAVE_OK:
        case MEMORY_CARD_STATUS_LOAD_OK:
        case MEMORY_CARD_STATUS_FORMAT_OK:
            if (gOptionsCardSoundState == OPTIONS_CARD_SOUND_PENDING) {
                gOptionsCardSoundState = OPTIONS_CARD_SOUND_OK_PLAYED;
                PlayMenuSound(OPTIONS_SOUND_CARD_SUCCESS);
            }
            /* fall through */
        case MEMORY_CARD_STATUS_SAVE_ERROR:
        case MEMORY_CARD_STATUS_LOAD_ERROR:
        case MEMORY_CARD_STATUS_NO_FREE_BLOCKS:
        case MEMORY_CARD_STATUS_NO_CARD:
        case MEMORY_CARD_STATUS_FORMAT_ERROR:
        case MEMORY_CARD_STATUS_NO_DATA:
        case MEMORY_CARD_STATUS_CORRUPT_DATA:
            if (gOptionsCardSoundState == OPTIONS_CARD_SOUND_PENDING) {
                gOptionsCardSoundState = OPTIONS_CARD_SOUND_ERROR_PLAYED;
                PlayMenuSound(OPTIONS_SOUND_CARD_ERROR);
            }
            if (gPadState.pressed & BOTH_PADS_CONFIRM) {
                PlayMenuSound(OPTIONS_SOUND_CONFIRM_DISMISS);
                gOptionsSubScreen = OPTIONS_SCREEN_MAIN;
                gOptionsMemoryCardMsg = OPTIONS_MSG_NONE;
            }
            break;
        case MEMORY_CARD_STATUS_UNFORMATTED:
        case MEMORY_CARD_STATUS_EXISTS:
            PlayMenuSound(OPTIONS_SOUND_CARD_PROMPT);
            gOptionsConfirmCursor = OPTIONS_CONFIRM_YES;
            gOptionsSubScreen = OPTIONS_SCREEN_CARD_CONFIRM;
            break;
        case MEMORY_CARD_STATUS_SAVING:
            break;
        }
    } else if (screen == OPTIONS_SCREEN_CARD_CONFIRM) {
        if (gPadState.pressed & BOTH_PADS(PADLleft)) {
            PlayMenuSound(OPTIONS_SOUND_CONFIRM_YES);
            gOptionsConfirmCursor = OPTIONS_CONFIRM_YES;
        } else if (gPadState.pressed & BOTH_PADS(PADLright)) {
            PlayMenuSound(OPTIONS_SOUND_CONFIRM_NO);
            gOptionsConfirmCursor = OPTIONS_CONFIRM_NO;
        }
        if (gPadState.pressed & BOTH_PADS_CONFIRM) {
            PlayMenuSound(OPTIONS_SOUND_CONFIRM_DISMISS);
            if (gOptionsConfirmCursor == OPTIONS_CONFIRM_NO) {
                gOptionsSubScreen = OPTIONS_SCREEN_MAIN;
                gOptionsMemoryCardMsg = OPTIONS_MSG_NONE;
            } else if (gOptionsMemoryCardMsg == OPTIONS_MSG_FORMAT_CONFIRM) {
                PlayMenuSound(OPTIONS_SOUND_START_FORMAT);
                gOptionsSubScreen = OPTIONS_SCREEN_CARD_STATUS;
                BeginMemoryCardFormat();
            } else if (gOptionsMemoryCardMsg == OPTIONS_MSG_OVERWRITE_CONFIRM) {
                PlayMenuSound(OPTIONS_SOUND_START_OVERWRITE);
                gOptionsSubScreen = OPTIONS_SCREEN_CARD_STATUS;
                BeginMemoryCardOverwriteSave();
            }
        }
    }
    FlushQueuedSounds();
}

#define DEMO_FIGHTER_PAIR_COUNT 6
#define DEMO_STAGE_COUNT 11
#define DEMO_WEAPON_MASK 7
#define DEMO_CAMERA_INIT_PITCH 0x200
#define DEMO_CAMERA_INIT_YAW_OFFSET 0xc00 /* from P1's heading */
#define DEMO_CAMERA_INIT_DISTANCE 8000
#define DEMO_POST_KO_LIMIT 106
#define DEMO_MAX_FRAMES 901

typedef struct DemoFighterPair {
    unsigned char p1;
    unsigned char p2;
} DemoFighterPair;

/* Stage IDs and fighter pairs cycled by attract-mode Demo Battles (11 stages, then an entry nothing reads; 6 pairs). */
unsigned char gDemoStageOrder[12] = {
    0, 1, 2, 3, STAGE_ID_CHERRY_BLOSSOM_GROVE, 4, 5, STAGE_ID_YAGURA_POINT, STAGE_ID_DOZAEMON_MOAT, 8, 10, 0
};
DemoFighterPair gDemoFighterPairs[DEMO_FIGHTER_PAIR_COUNT] = {
    { 10, 7 }, { 11, 9 }, { 8, 6 }, { 4, 1 }, { 5, 3 }, { 2, 0 },
};
int gDemoFighterP1;
int gDemoWeaponP1;

void InitDemoBattle(void)
{
    int stage;

    WaitGpuIdle();
    ShowNowLoadingOverlay();
    StopAndWaitCdIdle();
    SetGeomScreen(CalcGeomScreenFromFov(DEFAULT_SCENE_FOV));
    LoadVisibilityMaskPreset(0);

    gPlayMode = PLAY_MODE_VS;
    gDemoFighterP1 = gDemoFighterPairs[gDemoFighterPairIndex].p1;
    gDemoFighterP2 = gDemoFighterPairs[gDemoFighterPairIndex].p2;
    gDemoWeaponP1 = rand() & DEMO_WEAPON_MASK;
    gDemoWeaponP2 = rand() & DEMO_WEAPON_MASK;
    stage = gDemoStageOrder[gDemoStageIndex];
    gDemoFighterPairIndex++;
    gCurrentStageId = stage;
    if (gDemoFighterPairIndex == DEMO_FIGHTER_PAIR_COUNT)
        gDemoFighterPairIndex = 0;
    if (++gDemoStageIndex == DEMO_STAGE_COUNT)
        gDemoStageIndex = 0;

    InitFighterForScene(stage, stage, 0, 1, gDemoFighterP1, gDemoWeaponP1, 0);
    InitFighterForScene(gCurrentStageId, gCurrentStageId, 1, 1, gDemoFighterP2, gDemoWeaponP2, 0);
    AssignFighterPairSlots();
    SetPracticeWeaponTextureMode(0);

    gCameraState.angles.vx = DEMO_CAMERA_INIT_PITCH;
    gPovModeEnabled = 0;
    gCameraState.angles.vz = 0;
    gCameraState.distance = DEMO_CAMERA_INIT_DISTANCE;
    gCameraOffset.target.vx = 0;
    gCameraOffset.target.vy = 0;
    gCameraOffset.target.vz = 0;
    gCameraOffset.angles.vx = 0;
    gCameraOffset.angles.vy = 0;
    gCameraOffset.angles.vz = 0;
    gCameraOffset.distance = 0;
    gCameraState.target.vx = (gFighters[0].position.vx + gFighters[1].position.vx) / 2;
    gCameraState.target.vy = (gFighters[0].position.vy + gFighters[1].position.vy) / 2;
    gCameraState.target.vz = (gFighters[0].position.vz + gFighters[1].position.vz) / 2;
    gCameraState.angles.vy = gFighters[0].angles.heading + DEMO_CAMERA_INIT_YAW_OFFSET;
    InitBillboardEffects();
    LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId,
                      gFighters[0].weaponId, gFighters[1].weaponId);
#ifndef VERSION_JP
    LoadCombatSoundBanks();
#endif
    LoadWeaponPairMotions();
    while (PollCdReady() == 0)
        VSync(0);
    LoadCombatStage(gCurrentStageId);
    InitPoseStream(MOTION_FILE->poseStream);

    SetFighterMotionPlacement(&gFighters[0], MOTION_FILE->startPlacements[gFighters[0].weaponPairSlot]);
    SetFighterMotionPlacement(&gFighters[1], MOTION_FILE->startPlacements[gFighters[1].weaponPairSlot]);
    gStateFrameTimer = 0;
    gPrevStageId = gCurrentStageId;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_DEMO_BATTLE;
}

void UpdateDemoBattle(void)
{
    int isRunning = 0;
    WorldVector *p1CameraTarget = &gFighters[0].cameraTargetPos;
    WorldVector *p2CameraTarget = &gFighters[1].cameraTargetPos;
    struct PadInputFrame input;

    ++gStateFrameTimer;
    if ((gFighters[0].action == FIGHTER_ACTION_NEUTRAL && (gFighters[0].animInfo->flags & ANIM_FLAG_RUN)) ||
        (gFighters[1].action == FIGHTER_ACTION_NEUTRAL && (gFighters[1].animInfo->flags & ANIM_FLAG_RUN))) {
        isRunning = 1;
    }
    UpdateBattleCamera(BATTLE_CAMERA_LIVE, (VECTOR *)p1CameraTarget,
                       (VECTOR *)p2CameraTarget, 0, isRunning, gBladeLockState);
    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();
    BuildFighterInput(0, &input);
    UpdateFighterFrame(0, &input);
    BuildFighterInput(1, &input);
    UpdateFighterFrame(1, &input);
    UpdateCombatCollisions();
    UpdateAndDrawSubWeapons();
    ++gCombatFrameCounter;
    TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
    TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    EndSceneFrame();
#ifndef VERSION_JP
    FlushQueuedSounds();
    DrawDemoOverlayText();
#endif
    if (gPostKoTimer >= DEMO_POST_KO_LIMIT) {
        StopAndWaitCdIdle();
        gGameState = GAME_STATE_INIT_OPENING;
    }
    if (gStateFrameTimer >= DEMO_MAX_FRAMES) {
        StopAndWaitCdIdle();
        gGameState = GAME_STATE_INIT_OPENING;
    }
    if ((gPadState.pressed & BOTH_PADS_START) != 0) {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}
