/*
 * Story and post-match game states: victory orbit, Story dialogue and castle-approach cutscenes, the
 * Continue countdown, endings, prologues, credits, the Slash-clear cutscene, the Japanese-only Analog
 * Controller menu, and the Movie Theater.
 */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <libcd.h>
#include <rand.h>

#include "gpu_code.h"
#include "cd.h"
#include "display_buffer.h"
#include "fighter.h"
#include "world_vector.h"
#include "game_state.h"
#include "menu_state.h"
#include "motion_file.h"
#include "movie.h"
#include "camera.h"
#include "subtitle.h"
#include "analog_menu.h"
#include "story_slideshow.h"
#include "billboard.h"
#include "collision.h"
#include "combat_state.h"
#include "decal.h"
#include "effect.h"
#include "fighter_cloth.h"
#include "fighter_pose.h"
#include "font_hud.h"
#include "link.h"
#include "main.h"
#include "match_setup.h"
#include "point_light.h"
#include "pose_stream.h"
#include "scene.h"
#include "sightline.h"
#include "sound.h"
#include "stage_callbacks.h"
#include "story_cutscene.h"
#include "stage_id.h"
#include "story_state.h"
#include "theater_ui.h"
#include "voice_state.h"
#include "tile_clip.h"
#include "weapon_trail.h"
#include "load_area.h"
#include "credits.h"
#include "globals.h"
#include "pad.h"

#define MOVIE_ID_OPENING 0
#define MOVIE_ID_NORMAL_ENDING_BASE 1
#define MOVIE_ID_TRUE_ENDING_BASE 7

void InitVictorySceneState(void)
{
    gBattleCameraLocked = 0;
    gCameraState.target = gFighters[gMatchWinnerSide].position;
    gCameraState.angles.vx = 0x200;
    gCameraState.angles.vy = gFighters[gMatchWinnerSide].angles.heading;
    gCameraState.angles.vz = 0;
    gCameraState.distance = 5000;
    gCameraOffset.target.vx = 0;
    gCameraOffset.target.vy = 0;
    gCameraOffset.target.vz = 0;
    gCameraOffset.angles.vx = 0;
    gCameraOffset.angles.vy = 0;
    gCameraOffset.angles.vz = 0;
    gCameraOffset.distance = 0;
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    StartWinnerVictoryMotion();
    gStateFrameTimer = 0;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_VICTORY_SCENE;
}

#define VICTORY_SCENE_FRAMES 150
#define HUD_BANNER_SLASH_VICTORY 3
#define STORY_APPROACH_BOUT_INDEX 4

void UpdateVictorySceneState(void)
{
    PadInputFrame input;
    int playMode;
    unsigned int pressed;

    ++gStateFrameTimer;
    SetCameraTransitionFromFighter(&gFighters[gMatchWinnerSide], (gStateFrameTimer << 12) / VICTORY_SCENE_FRAMES);
    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();
    input.released = 0;
    input.pressed = 0;
    input.held = 0;
    input.notHeld = 0xffff;
    UpdateFighterFrame(0, &input);
    UpdateFighterFrame(1, &input);
    UpdateCombatCollisions();
    TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
    TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    EndSceneFrame();
    UpdateAmbientSounds();
    FlushQueuedSounds();
    if (gPlayMode == PLAY_MODE_SLASH)
        DrawHudBanner(HUD_BANNER_SLASH_VICTORY);

    if (gStateFrameTimer < VICTORY_SCENE_FRAMES) {
        pressed = gPadState.pressed;
        if (!(pressed & (MENU_CANCEL << (gMatchWinnerSide * 16))) &&
            (gHumanPlayerCount != 1 || !(pressed & MENU_CANCEL)))
            return;
    }

    playMode = gPlayMode;
    if (playMode == PLAY_MODE_SLASH) {
        if (TryUnlockKatze()) {
            gDisplayPresentEnabled = 0;
            gGameState = GAME_STATE_INIT_SLASH_CLEAR_CUTSCENE;
        } else {
            gGameState = GAME_STATE_INIT_MAIN_MENU;
        }
    } else if (playMode == PLAY_MODE_STORY) {
        if (gStoryChallengerActive) {
            if (gMatchWinnerSide != 0) {
                gGameState = (gOptionContinueEnabled != 0)
                    ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
            } else if (gOptionContinueEnabled != 0) {
                gGameState = GAME_STATE_INIT_CONTINUE_SCREEN;
            } else {
                gStoryChallengerActive = 0;
                gHumanPlayerCount = 1;
                gGameState = (gStoryProloguePlayed != 0) ? GAME_STATE_INIT_COMBAT : GAME_STATE_INIT_STORY_PROLOGUE;
                gDisplayPresentEnabled = 0;
                StopCombatRumbleAndAudio();
            }
        } else {
            int nextState;
            int boutIndex = gStoryBoutIndex;

            gStoryEventResult = STORY_EVENT_NONE;
            if (boutIndex == STORY_APPROACH_BOUT_INDEX) {
                PrepareApproachCutscene();
                nextState = GAME_STATE_INIT_APPROACH_CUTSCENE;
            } else {
                nextState = GAME_STATE_INIT_COMBAT;
            }
            gGameState = nextState;
            gDisplayPresentEnabled = 0;
        }
    } else if (playMode == PLAY_MODE_LINK) {
        gGameState = GAME_STATE_INIT_CONTINUE_SCREEN;
    } else {
        gGameState = (gOptionContinueEnabled != 0) ? GAME_STATE_INIT_CONTINUE_SCREEN : GAME_STATE_INIT_MAIN_MENU;
    }
}

short gStoryCutsceneVoiceIndex;

void InitStoryCutsceneState(void)
{
    StopCombatRumbleAndAudio();
    WaitGpuIdle();
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    LoadDialogueSoundBank();
    InitStoryCutscene(gStoryBoutIndex - 1, gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT, DYNAMIC_LOAD_BUFFER);
    StartStageAmbientSounds();
    gStoryCutsceneVoiceIndex = 0;
    gStateFrameTimer = 0;
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_STORY_CUTSCENE;
}

int IsTrueEndingEligible(void)
{
    int eligible;

    eligible = 0;
    if (gStoryNoDamageFlag != 0 && gContinueCount == 0) {
        eligible = gReachedStage9Flag != 0;
    }
    return eligible;
}

#define STORY_DIALOGUE_SOUND_BASE 0x6c

void UpdateStoryCutsceneState(void)
{
    int finished = 0;
    int cutsceneStatus;
    int boutIndex;

    if (gStateFrameTimer == 0)
        gDisplayPresentEnabled = 1;

    cutsceneStatus = UpdateStoryCutscene();
    if (cutsceneStatus == STORY_CUTSCENE_FINISHED) {
        finished = 1;
    } else if (cutsceneStatus == STORY_CUTSCENE_VOICE_CUE) {
        if (gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT != FIGHTER_CLASS_UTSUSEMI ||
            gStoryBoutIndex != 6 || gStoryCutsceneVoiceIndex != 1) {
            StopPriorityVoice0();
            QueuePriorityVoice0(gStoryCutsceneVoiceIndex + STORY_DIALOGUE_SOUND_BASE, SOUND_MAX_VOLUME,
                                SOUND_MAX_VOLUME);
        }
        gStoryCutsceneVoiceIndex++;
    }

    UpdateAmbientSounds();
    FlushQueuedSounds();
    if (gPadState.pressed & MENU_CANCEL)
        finished = 1;

    if (finished) {
        boutIndex = gStoryBoutIndex;
        if (boutIndex < STORY_APPROACH_BOUT_INDEX) {
            if (gStoryStage9FallSkip != 0) {
                gStoryBoutIndex = STORY_APPROACH_BOUT_INDEX;
                gStoryStage9FallSkip = 0;
            }
            gDisplayPresentEnabled = 0;
            gGameState = GAME_STATE_INIT_COMBAT;
        } else if (boutIndex == STORY_APPROACH_BOUT_INDEX) {
            gDisplayPresentEnabled = 0;
            if (gCurrentStageId != STAGE_ID_UNDERGROUND_GROTTO) {
                PrepareApproachCutscene();
                gGameState = GAME_STATE_INIT_APPROACH_CUTSCENE;
            } else {
                gGameState = GAME_STATE_INIT_COMBAT;
            }
        } else if (boutIndex < STORY_BOUT_COUNT) {
            if (gBushidoHonorIntact == 0) {
                gGameState = GAME_STATE_INIT_BAD_ENDING_SLIDE;
            } else {
                gDisplayPresentEnabled = 0;
                gGameState = GAME_STATE_INIT_COMBAT;
            }
        } else if (boutIndex == STORY_TRUE_ENDING_WON) {
            if (gBushidoHonorIntact == 0) {
                gGameState = GAME_STATE_INIT_BAD_ENDING_SLIDE;
            } else {
                gGameState = GAME_STATE_PLAY_STORY_ENDING_MOVIE;
            }
        }
        StopCombatRumbleAndAudio();
    }
    gStateFrameTimer++;
}

short gContinueOrbitYaw, gContinueConfirmTimer;

#define CONTINUE_COUNTDOWN_FRAMES 300
#define CONTINUE_FRAMES_PER_SECOND 30 /* a second on NTSC; 1.2 s in the European build, which runs 25 frames a second */
#define CONTINUE_CONFIRM_FRAMES 60
#define HUD_BANNER_CONTINUE 0
#define SOUND_CONTINUE_TICK 0x12
#define SOUND_CONTINUE_CONFIRM 0x13

#define CONTINUE_PAD_P1 1
#define CONTINUE_PAD_P2 2

#define IS_FIGHTER_KO(fighter) \
    ((fighter).action == FIGHTER_ACTION_DEAD || (fighter).action == FIGHTER_ACTION_FATAL_KO || \
     (fighter).action == FIGHTER_ACTION_DEFEATED || ((fighter).placement->flags & MOTION_FLAG_FATAL))

void InitContinueScreenState(void)
{
    Fighter *defeated = gFighters;
    short orbitYaw;
    unsigned short yaw;

    if (gMatchWinnerSide == 0)
        defeated++;
    if (gFighterDistanceXZ < 2500) {
        orbitYaw = -(unsigned short)defeated->angleToOpponent;
    } else {
        orbitYaw = (defeated->isSupine ? 0x800 : 0) - defeated->angles.heading;
    }
    gContinueOrbitYaw = orbitYaw;
    gBattleCameraLocked = 0;
    gCameraState.target = defeated->position;
    yaw = gContinueOrbitYaw;
    gCameraState.angles.vx = 0x200;
    gCameraState.angles.vz = 0;
    gCameraState.distance = 1500;
    gCameraOffset.target.vx = 0;
    gCameraOffset.target.vy = 0;
    gCameraOffset.target.vz = 0;
    gCameraOffset.angles.vx = 0;
    gCameraOffset.angles.vy = 0;
    gCameraOffset.angles.vz = 0;
    gCameraOffset.distance = 0;
    gCameraState.angles.vy = yaw - 0x400;
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    gContinueConfirmTimer = 0;
    gStateFrameTimer = CONTINUE_COUNTDOWN_FRAMES;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_CONTINUE_SCREEN;
}

void UpdateContinueScreenState(void)
{
    PadInputFrame emptyInput;
    Fighter *defeated;
    int orbitProgress;
    int remainingFrames;
    int subSecondFrames;
    int activePadsMask;
    unsigned int pressed;

    if (gMatchWinnerSide == 0) {
        defeated = &gFighters[1];
    } else {
        defeated = &gFighters[0];
    }
    if (gContinueConfirmTimer <= 0) {
        orbitProgress = ((CONTINUE_COUNTDOWN_FRAMES - gStateFrameTimer) << 12) / CONTINUE_COUNTDOWN_FRAMES;
    } else {
        orbitProgress = 0;
    }
    UpdateContinueOrbitCamera(defeated, orbitProgress, gContinueOrbitYaw);
    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();
    emptyInput.released = 0;
    emptyInput.pressed = 0;
    emptyInput.held = 0;
    emptyInput.notHeld = 0xffff;
    UpdateFighterFrame(0, &emptyInput);
    UpdateFighterFrame(1, &emptyInput);
    UpdateCombatCollisions();
    TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
    TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    DisableStageTileNearClip();
    EndSceneFrame();
    UpdateAmbientSounds();
    FlushQueuedSounds();

    if (gContinueConfirmTimer > 0) {
        if (gStateFrameTimer < 16) {
            gStateFrameTimer++;
        }
        gStateFrameTimer = gStateFrameTimer * 110 / 100;
        if (gStateFrameTimer >= CONTINUE_COUNTDOWN_FRAMES) {
            gStateFrameTimer = CONTINUE_COUNTDOWN_FRAMES - 1;
        }
        SetCdAudioMix((gStateFrameTimer << 7) / CONTINUE_COUNTDOWN_FRAMES, 0,
                      (gStateFrameTimer << 7) / CONTINUE_COUNTDOWN_FRAMES, 0);
        DrawHudBanner(HUD_BANNER_CONTINUE);
        if (++gContinueConfirmTimer >= CONTINUE_CONFIRM_FRAMES + 1) {
            gIsContinueRestart = 1;
            gContinueCount++;
            if (gPlayMode == PLAY_MODE_VS || gPlayMode == PLAY_MODE_LINK) {
                IncrementVsWinCounts();
            }
            SetCdAudioMix(0x7f, 0, 0x7f, 0);
            gGameState = GAME_STATE_INIT_COMBAT;
        }
        return;
    }

    remainingFrames = --gStateFrameTimer;
    subSecondFrames = remainingFrames % CONTINUE_FRAMES_PER_SECOND;
    SetCdAudioMix((remainingFrames << 7) / CONTINUE_COUNTDOWN_FRAMES, 0,
                  (remainingFrames << 7) / CONTINUE_COUNTDOWN_FRAMES, 0);
    if (subSecondFrames == 0) {
        QueueSoundEffect(SOUND_CONTINUE_TICK, 0x7f, 0x7f);
    }
    DrawContinueCountdown(remainingFrames / CONTINUE_FRAMES_PER_SECOND);

    if (gHumanPlayerCount == 1) {
        activePadsMask = CONTINUE_PAD_P1;
    } else if (IS_FIGHTER_KO(gFighters[0]) && IS_FIGHTER_KO(gFighters[1])) {
        activePadsMask = CONTINUE_PAD_P1 | CONTINUE_PAD_P2;
    } else if (gMatchWinnerSide == 0) {
        activePadsMask = CONTINUE_PAD_P2;
    } else {
        activePadsMask = CONTINUE_PAD_P1;
    }
    pressed = gPadState.pressed;
    if (!(activePadsMask & CONTINUE_PAD_P1)) {
        pressed &= 0xffff0000;
    }
    if (!(activePadsMask & CONTINUE_PAD_P2)) {
        pressed &= 0xffff;
    }
    if (pressed & BOTH_PADS_CONFIRM) {
        StopPriorityVoice0();
        QueuePriorityVoice0(SOUND_CONTINUE_CONFIRM, 0x7f, 0x7f);
        gContinueConfirmTimer = 1;
        gFighters[gMatchWinnerSide == 0].faceExpression = FIGHTER_FACE_LOCKED_NEUTRAL;
    }
    if (pressed & BOTH_PADS_CANCEL) {
        if (subSecondFrames == 0) {
            gStateFrameTimer -= CONTINUE_FRAMES_PER_SECOND - 1;
        } else {
            gStateFrameTimer += 1 - subSecondFrames;
        }
    }
    if (gStateFrameTimer >= 0) {
        return;
    }
    StopCombatRumbleAndAudio();
    StopAndWaitCdIdle();
    if (gPlayMode == PLAY_MODE_STORY && gStoryChallengerActive != 0 && gMatchWinnerSide == 0) {
        gStoryChallengerActive = 0;
        gHumanPlayerCount = 1;
        gGameState = gStoryProloguePlayed ? GAME_STATE_INIT_COMBAT : GAME_STATE_INIT_STORY_PROLOGUE;
    } else {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}

WorldVector gApproachCutsceneSpawns[2][2] = {
    { { -20736, 0, 4096 }, { 17408, 0, -4096 } },
    { { 6144, 0, 12632 }, { -10483, 0, 14806 } },
};
short gApproachCutscenePart;

void GetApproachCutscenePositions(int side, WorldVector *first, WorldVector *second)
{
    WorldVector *other;

    *first = gApproachCutsceneSpawns[gApproachCutscenePart][side];

    other = gApproachCutsceneSpawns[gApproachCutscenePart];
    if (side == 0) {
        other++;
    }
    *second = *other;
}

#define APPROACH_STAGE_OUTER 3
#define APPROACH_STAGE_INNER 9
#define APPROACH_PART_FRAMES 150

WorldVector gApproachCutsceneCameras[2] = {
    { -5000, -500, 0 },
    { -10656, -500, 1555 },
};
/* Indexed by the stage bout 3 ends on; 255 only for stages bouts 0 to 3 cannot reach. */
unsigned char gApproachCutsceneStartPart[STAGE_COUNT] = {
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 0, 255, 255, 255,
};
short gApproachCutsceneLoaded;

void PrepareApproachCutscene(void)
{
    unsigned char startPart = gApproachCutsceneStartPart[gCurrentStageId];

    gApproachCutsceneLoaded = 0;
    gApproachCutscenePart = startPart;
}

void InitApproachCutsceneState(void)
{
    short stageId = APPROACH_STAGE_INNER;
    WorldVector *cameraTarget;

    if (gApproachCutscenePart == 0)
        stageId = APPROACH_STAGE_OUTER;
    gCurrentStageId = stageId;
    gPrevStageId = stageId;
    InitFighterForScene(stageId, stageId, 0, 0, gSelectedFighter[0], gSelectedWeapon[0], 0);
    InitFighterForScene(gCurrentStageId, gPrevStageId, 1, 0, gSelectedFighter[1], gSelectedWeapon[1], 0);
    AssignFighterPairSlots();

    cameraTarget = (WorldVector *)&gCameraState.target;
    gBattleCameraLocked = 0;
    *cameraTarget = gApproachCutsceneCameras[gApproachCutscenePart];
    gCameraState.angles.vy = -ratan2(gFighters[0].position.vx - cameraTarget->x,
                                   gFighters[0].position.vz - gCameraState.target.vz);
    gCameraOffset.target.vx = 0;
    gCameraOffset.target.vy = 0;
    gCameraOffset.target.vz = 0;
    gCameraOffset.angles.vx = 0;
    gCameraOffset.angles.vy = 0;
    gCameraOffset.angles.vz = 0;
    gCameraOffset.distance = 0;
    if (gApproachCutsceneLoaded == 0) {
        gApproachCutsceneLoaded = 1;
        StopCombatRumbleAndAudio();
        LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId,
                          gFighters[0].weaponId, gFighters[1].weaponId);
        LoadWeaponPairMotions();
        while (PollCdReady() == 0)
            VSync(0);
        InitPoseStream(MOTION_FILE->poseStream);
    }
    LoadApproachSoundBanks();
    LoadCombatStage(gCurrentStageId);
    SetFighterMotionPlacement(gFighters, MOTION_FILE->startPlacements[gFighters[0].weaponPairSlot]);
    SetFighterMotionPlacement(&gFighters[1], MOTION_FILE->startPlacements[gFighters[1].weaponPairSlot]);
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    StartStageAmbientSounds();
    gStateFrameTimer = 0;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_APPROACH_CUTSCENE;
}

void UpdateApproachCutsceneState(void)
{
    PadInputFrame input;
    int frame;

    AimSceneCameraAtFighter(gFighters, ((APPROACH_PART_FRAMES - gStateFrameTimer) << 12) / APPROACH_PART_FRAMES);
    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();
    input.released = 0;
    input.pressed = 0;
    input.held = 0;
    if (gApproachCutscenePart == 0) {
        frame = gStateFrameTimer;
        if (frame < 2) {
            input.held = PADLright | PADL1;
            input.pressed = PADLright | PADL1;
        } else if (frame < 8) {
            input.held = PADLright | PADL1;
        } else if (frame == 8) {
            input.held = PADL1;
            input.released = PADLright;
        } else {
            input.held = PADL1;
        }
    } else {
        frame = gStateFrameTimer;
        if (frame < 2) {
            input.held = PADLup | PADL1;
            input.pressed = PADLup | PADL1;
        } else if (frame < 60) {
            input.held = PADLup | PADL1;
        } else if (frame == 60) {
            input.held = PADLright | PADL1;
            input.pressed = PADLright;
            input.released = PADLup;
        } else if (frame < 84) {
            input.held = PADLright | PADL1;
        } else if (frame == 84) {
            input.held = PADLright | PADL1 | PADR2;
            input.pressed = PADR2;
        } else if (frame < 88) {
            input.held = PADLright | PADL1 | PADR2;
        } else if (frame == 88) {
            input.held = PADLright | PADR1 | PADL1 | PADR2;
            input.pressed = PADR1;
        } else {
            input.held = PADLright | PADR1 | PADL1 | PADR2;
        }
    }
    input.notHeld = ~input.held & 0xffff;
    UpdateFighterFrame(0, &input);
    input.released = 0;
    input.pressed = 0;
    input.held = 0;
    input.notHeld = 0xffff;
    UpdateFighterFrame(1, &input);
    UpdateCombatCollisions();
    TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
    TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    EndSceneFrame();
    UpdateAmbientSounds();
    FlushQueuedSounds();
    gStateFrameTimer++;
    if (gPadState.pressed & BOTH_PADS(MENU_CANCEL)) {
        StopCombatRumbleAndAudio();
        gDisplayPresentEnabled = 0;
        gGameState = GAME_STATE_INIT_COMBAT;
    }
    if (gStateFrameTimer >= APPROACH_PART_FRAMES + 1) {
        if (gApproachCutscenePart == 0) {
            gApproachCutscenePart++;
            gGameState = GAME_STATE_INIT_APPROACH_CUTSCENE;
        } else {
            gGameState = GAME_STATE_INIT_COMBAT;
        }
        gDisplayPresentEnabled = 0;
    }
}

WorldVector gSlashClearSpawns[2] = {
    { 0, 0, 0 },
    { 1536, 0, 0 },
};

void GetSlashClearCutscenePositions(int side, WorldVector *first, WorldVector *second)
{
    WorldVector *other;

    *first = gSlashClearSpawns[side];
    other = gSlashClearSpawns;
    if (side == 0) {
        other = &gSlashClearSpawns[1];
    }
    *second = *other;
}

#define FIGHTER_ID_KATZE 14

void InitSlashClearCutsceneState(void)
{
    MotionFileHeader *motionFile;

    StopCombatRumbleAndAudio();
    StopAndWaitCdIdle();
    WaitGpuIdle();
    InitFighterForScene(gCurrentStageId, gPrevStageId, 0, 0, gSelectedFighter[0], gSelectedWeapon[0], 0);
    InitFighterForScene(gCurrentStageId, gPrevStageId, 1, 0, FIGHTER_ID_KATZE,
                        gBossDefaultWeapons[FIGHTER_ID_KATZE - FIGHTER_PLAYABLE_ID_COUNT], 0);
    AssignFighterPairSlots();
    LoadCutsceneFighterSoundBanks();
    LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId,
                      gFighters[0].weaponId, gFighters[1].weaponId);
    LoadWeaponPairMotions();
    while (PollCdReady() == 0)
        VSync(0);
    motionFile = MOTION_FILE;
    InitPoseStream(motionFile->poseStream);
    LoadCombatStage(gCurrentStageId);
    SetFighterMotionPlacement(&gFighters[0],
                              motionFile->startPlacements[gFighters[0].weaponPairSlot]);
    SetFighterMotionPlacement(&gFighters[1],
                              motionFile->storyIntroPlacements[gFighters[1].characterClass]
                                                              [gFighters[1].weaponPairSlot]);
    ResetFighterWeaponTrails(0);
    ResetFighterWeaponTrails(1);
    ClearBillboardEffects();
    ClearGroundDecals();
    StopPointLightFlash();
    ClearEffectPool();
    InitStageSnowIfEnabled();
    gStateFrameTimer = 0;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_SLASH_CLEAR_CUTSCENE;
}

#define SLASH_CLEAR_CUTSCENE_FRAMES 240
#define SLASH_CLEAR_FADE_START_FRAME 208
#define SLASH_CLEAR_FADE_STEP 8
#define SLASH_CLEAR_FADE_TPAGE GET_TPAGE(0, 2, 0, 0)
#define HUD_BANNER_SLASH_CLEAR 4

static POLY_F4 sSlashClearFadeQuads[2];
static DR_TPAGE sSlashClearFadeTPages[2];

void UpdateSlashClearCutsceneState(void)
{
    PadInputFrame input;
    POLY_F4 *fadeQuad;
    DR_TPAGE *fadeTPage;
    int fade;

    SetSlashClearCamera(&gFighters[1],
                        ((SLASH_CLEAR_CUTSCENE_FRAMES - gStateFrameTimer) << 12) / SLASH_CLEAR_CUTSCENE_FRAMES);
    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();

    input.released = 0;
    input.pressed = 0;
    input.held = 0;
    input.notHeld = (~input.held) & 0xffff;
    UpdateFighterFrame(0, &input);

    input.released = 0;
    input.pressed = 0;
    input.held = 0;
    if (gStateFrameTimer < 90) {
    } else if (gStateFrameTimer == 90) {
        input.held = PADLleft;
        input.pressed = PADLleft;
    } else if (gStateFrameTimer < 92) {
        input.held = PADLleft;
    } else if (gStateFrameTimer == 92) {
        input.released = PADLleft;
    } else if (gStateFrameTimer < 94) {
    } else if (gStateFrameTimer == 94) {
        input.held = PADLleft;
        input.pressed = PADLleft;
    } else if (gStateFrameTimer < 96) {
        input.held = PADLleft;
    } else if (gStateFrameTimer == 96) {
        input.released = PADLleft;
    } else if (gStateFrameTimer < 150) {
    } else if (gStateFrameTimer == 150) {
        input.held = PADRup;
        input.pressed = PADRup;
    }
    input.notHeld = (~input.held) & 0xffff;
    UpdateFighterFrame(1, &input);

    UpdateCombatCollisions();
    TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
    TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    EndSceneFrame();
    FlushQueuedSounds();
    DrawHudBanner(HUD_BANNER_SLASH_CLEAR);

    fadeQuad = &sSlashClearFadeQuads[gFrameCounter & 1];
    fadeTPage = &sSlashClearFadeTPages[gFrameCounter & 1];
    SetPolyF4(fadeQuad);
    SetSemiTrans(fadeQuad, 1);
    setXY4(fadeQuad, 0, 0, DISPLAY_WIDTH, 0, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    fade = (gStateFrameTimer - SLASH_CLEAR_FADE_START_FRAME) * SLASH_CLEAR_FADE_STEP;
    if (fade < 0) {
        fade = 0;
    } else if (fade >= 256) {
        fade = 255;
    }
    setRGB0(fadeQuad, fade, fade, fade);
    AddPrim(gCurrentOt, fadeQuad);
    SetDrawTPage(fadeTPage, 0, 1, SLASH_CLEAR_FADE_TPAGE);
    AddPrim(gCurrentOt, fadeTPage);

    if (++gStateFrameTimer > SLASH_CLEAR_CUTSCENE_FRAMES) {
        StopCombatRumbleAndAudio();
        gMainMenuSelection = MAIN_MENU_ENTRY_VS;
        gAutoSaveReturnState = GAME_STATE_INIT_MAIN_MENU;
        gDisplayPresentEnabled = 0;
        gGameState = GAME_STATE_INIT_AUTO_SAVE;
    }
}

/* No code enters states 15 and 16. */
void GameState15NoOp(void)
{
}

void GameState16NoOp(void)
{
}

void PlayStoryEndingMovieState(void)
{
    unsigned short characterClass;
    int boutIndex;
    int ending;
    int movieEnding;

    characterClass = gFighters[0].characterClass;
    boutIndex = gStoryBoutIndex;
    gPendingMovieId = characterClass;
    if (boutIndex == STORY_TRUE_ENDING_WON) {
        gPendingMovieId = characterClass + PLAYABLE_CHARACTER_COUNT;
    }
    ending = gPendingMovieId;
    gUnlockedMoviesMask |= 1 << ending;
    StopCombatRumbleAndAudio();
#ifndef VERSION_JP
    LoadMovieSubtitleBank(gSharedWorkBuffer);
#endif
    movieEnding = gPendingMovieId;
    PlayFmvMovie(movieEnding + MOVIE_ID_NORMAL_ENDING_BASE);
    SetDispMask(0);
    DrawSync(0);
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_INIT_CREDITS;
}

/* No code enters this state. */
void WaitCancelToCreditsState(void)
{
    if ((gPadState.pressed & MENU_CANCEL) != 0) {
        gGameState = GAME_STATE_INIT_CREDITS;
    }
}

#define BAD_ENDING_SLIDE_COUNT 5

void InitBadEndingSlideState(void)
{
    StopCombatRumbleAndAudio();
    DrawSync(0);
    InitStorySlideshow(SLIDESHOW_MODE_BAD_ENDING, DYNAMIC_LOAD_BUFFER, rand() % BAD_ENDING_SLIDE_COUNT);
    EnableCdSpuAudio();
    StartXaTrack(XA_TRACK_BAD_ENDING);
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_BAD_ENDING_SLIDE;
}

void UpdateBadEndingSlideState(void)
{
    gDisplayPresentEnabled = 1;
    UpdateStorySlideshow();
    if ((gPadState.pressed & MENU_CANCEL) != 0) {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}

typedef enum PrologueXaSwitchState {
    PROLOGUE_XA_IDLE = 0,
    PROLOGUE_XA_STOPPING_INTRO = 1,
    PROLOGUE_XA_CHARACTER_CUED = 2
} PrologueXaSwitchState;

short gPrologueXaSwitchState;
short gPrologueSlideStatus;
static unsigned char sPrologueCharacterXaTracks[] = {
    XA_TRACK_PROLOGUE_BASE + 0,
    XA_TRACK_PROLOGUE_BASE + 1,
    XA_TRACK_PROLOGUE_BASE + 2,
    XA_TRACK_PROLOGUE_BASE + 3,
    XA_TRACK_PROLOGUE_BASE + 4,
    XA_TRACK_PROLOGUE_BASE + 5
};

void InitStoryPrologueState(void)
{
    gPrologueXaSwitchState = PROLOGUE_XA_IDLE;
    gPrologueSlideStatus = PROLOGUE_STATUS_RUNNING;
    DrawSync(0);
    InitStorySlideshow(SLIDESHOW_MODE_PROLOGUE, DYNAMIC_LOAD_BUFFER,
                       gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT);
    gStoryProloguePlayed = 1;
    gDisplayPresentEnabled = 0;
    gStateFrameTimer = 0;
    gGameState = GAME_STATE_UPDATE_STORY_PROLOGUE;
}

void UpdateStoryPrologueState(void)
{
    int characterIndex = gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT;
    int status;

    gDisplayPresentEnabled = 1;
    gStateFrameTimer++;
    status = UpdateStorySlideshow();
    switch (status) {
    case PROLOGUE_STATUS_FINISHED:
        StopAndWaitCdIdle();
        gGameState = GAME_STATE_INIT_COMBAT;
        break;
    case PROLOGUE_STATUS_CUE_INTRO_XA:
        EnableCdSpuAudio();
        CueXaTrackPaused(XA_TRACK_PROLOGUE_INTRO);
        break;
    case PROLOGUE_STATUS_STOP_INTRO_XA:
        StopCdAudioStream();
        gPrologueXaSwitchState = PROLOGUE_XA_STOPPING_INTRO;
        break;
    case PROLOGUE_STATUS_START_INTRO_XA:
    case PROLOGUE_STATUS_START_CHAR_XA:
        ReleaseHeldCdStream();
        break;
    }
    if (status != PROLOGUE_STATUS_RUNNING) {
        gPrologueSlideStatus = status;
    }
    if (gPrologueXaSwitchState == PROLOGUE_XA_STOPPING_INTRO && IsCdIdle()) {
        gPrologueXaSwitchState = PROLOGUE_XA_CHARACTER_CUED;
        CueXaTrackPaused(sPrologueCharacterXaTracks[characterIndex]);
    }
    if (gPadState.pressed & MENU_CANCEL) {
        if (gPrologueSlideStatus >= PROLOGUE_STATUS_STOP_INTRO_XA) {
            StopAndWaitCdIdle();
            gGameState = GAME_STATE_INIT_COMBAT;
        } else {
            RequestSkipStoryPrologue();
        }
    }
}

void InitCreditsState(void)
{
    DrawSync(0);
    InitCreditsRoll();
    EnableCdSpuAudio();
    StartXaTrack(XA_TRACK_CREDITS);
    WaitCdStreamPlaying();
    gDisplayPresentEnabled = 0;
    gStateFrameTimer = 0;
    gGameState = GAME_STATE_UPDATE_CREDITS;
}

#define CREDITS_REVEAL_FRAME 3

void UpdateCreditsState(void)
{
    int frame;

    gDisplayPresentEnabled = 1;
    frame = ++gStateFrameTimer;
    if (frame >= CREDITS_REVEAL_FRAME) {
        if (frame == CREDITS_REVEAL_FRAME) {
            SetDispMask(1);
        }
        if (UpdateAndDrawCreditsRoll() == 0 || (gPadState.pressed & BOTH_PADS(MENU_CANCEL)) != 0) {
            if (gOptionAutoSave != 0) {
                gAutoSaveReturnState = GAME_STATE_INIT_OPENING;
                gGameState = GAME_STATE_INIT_AUTO_SAVE;
            } else {
                gGameState = GAME_STATE_INIT_OPENING;
            }
        }
    }
}

void HandleLinkAbortState(void)
{
    gLinkErrorFlag = 0;
    gGameState = GAME_STATE_INIT_MAIN_MENU;
}

/* No code enters this state. */
void WaitCrossToMenuState(void)
{
    if ((gPadState.pressed & PADRdown) != 0) {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}

int TryUnlockKatze(void)
{
    int unlocked;

    if (gContinueCount == 0 && gOptionDifficulty != 0) {
        if (gOptionWallBump == 0) {
            gKatzeUnlocked = 1;
            unlocked = 1;
        } else {
            unlocked = 0;
        }
    } else {
        unlocked = 0;
    }
    return unlocked;
}

/* No code enters this state, the only way into UpdateWaitCancelToMenuState. */
void InitWaitCancelToMenuState(void)
{
    gStateFrameTimer = 0;
    gGameState = GAME_STATE_UPDATE_WAIT_CANCEL_TO_MENU;
}

void UpdateWaitCancelToMenuState(void)
{
    gStateFrameTimer++;
    if ((gPadState.pressed & MENU_CANCEL) != 0) {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}

#ifdef VERSION_JP
#define ANALOG_MENU_ROW_RUMBLE 0
#define ANALOG_MENU_ROW_STICKS 1

enum {
    ANALOG_MENU_SOUND_CURSOR_UP = 0x15,
    ANALOG_MENU_SOUND_CURSOR_DOWN = 0x16,
    ANALOG_MENU_SOUND_TOGGLE = 0x17,
    ANALOG_MENU_SOUND_CALIBRATE = 0x18,
    ANALOG_MENU_SOUND_EXIT = 0x19
};

short gAnalogMenuCursor;

void InitAnalogControllerMenuState(void)
{
    gAnalogMenuCursor = ANALOG_MENU_ROW_RUMBLE;
    LoadAnalogControllerAssets(DYNAMIC_LOAD_BUFFER);
    gGameState = GAME_STATE_UPDATE_ANALOG_MENU;
}
#else
void InitAnalogControllerMenuState(void)
{
}
#endif

#ifdef VERSION_JP
void UpdateAnalogControllerMenuState(void)
{
    int flags;
    int sticks[8];

    flags = (gAnalogMenuCursor == ANALOG_MENU_ROW_RUMBLE) |
            ((gAnalogMenuCursor == ANALOG_MENU_ROW_STICKS) ? ANALOG_MENU_FLAG_ROW_STICKS : 0) |
            (gOptionRumbleEnabled << 2) |
            ((gOptionRumbleEnabled == 0) ? ANALOG_MENU_FLAG_RUMBLE_OFF : 0);
    if (gAnalogMenuCursor == ANALOG_MENU_ROW_STICKS) {
        flags |= (IsPadAnalog(0) << 4) | (IsPadAnalog(1) << 5);
    }
    if (flags & ANALOG_MENU_FLAG_P1_ANALOG) {
        sticks[0] = gPadState.analog[0][2];
        sticks[1] = gPadState.analog[0][3];
        sticks[2] = gPadState.analog[0][0];
        sticks[3] = gPadState.analog[0][1];
    }
    if (flags & ANALOG_MENU_FLAG_P2_ANALOG) {
        sticks[4] = gPadState.analog[1][2];
        sticks[5] = gPadState.analog[1][3];
        sticks[6] = gPadState.analog[1][0];
        sticks[7] = gPadState.analog[1][1];
    }
    DrawAnalogControllerMenu(flags, sticks[0], sticks[1], sticks[2], sticks[3],
                             sticks[4], sticks[5], sticks[6], sticks[7]);
    if (gPadState.pressed & BOTH_PADS(PADLup)) {
        PlayMenuSound(ANALOG_MENU_SOUND_CURSOR_UP);
        gAnalogMenuCursor--;
    }
    if (gPadState.pressed & BOTH_PADS(PADLdown)) {
        PlayMenuSound(ANALOG_MENU_SOUND_CURSOR_DOWN);
        gAnalogMenuCursor++;
    }
    gAnalogMenuCursor &= 1;
    if (gAnalogMenuCursor == ANALOG_MENU_ROW_RUMBLE) {
        if (gPadState.pressed & BOTH_PADS(PADLright)) {
            if (gOptionRumbleEnabled != 0) {
                PlayMenuSound(ANALOG_MENU_SOUND_TOGGLE);
            }
            gOptionRumbleEnabled = 0;
        }
        if (gPadState.pressed & BOTH_PADS(PADLleft)) {
            if (gOptionRumbleEnabled == 0) {
                PlayMenuSound(ANALOG_MENU_SOUND_TOGGLE);
            }
            gOptionRumbleEnabled = 1;
        }
    } else if (gAnalogMenuCursor == ANALOG_MENU_ROW_STICKS) {
        if (gPadState.pressed & BOTH_PADS(MENU_CONFIRM)) {
            PlayMenuSound(ANALOG_MENU_SOUND_CALIBRATE);
            CapturePadStickCentres(0);
            CapturePadStickCentres(1);
        }
    }
    if (gPadState.pressed & BOTH_PADS(MENU_CANCEL)) {
        PlayMenuSound(ANALOG_MENU_SOUND_EXIT);
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
    FlushQueuedSounds();
}
#else
void UpdateAnalogControllerMenuState(void)
{
}
#endif

short gMovieTheaterFadeTimer;
short gMovieTheaterSelection;

void InitMovieTheaterState(void)
{
    gMovieTheaterSelection = 0;
    gMovieTheaterFadeTimer = 0;
    gStateFrameTimer = 0;
    DrawSync(0);
    LoadMovieTheaterAssets();
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_MOVIE_THEATER;
}

#ifdef VERSION_JP
#define THEATER_EXIT MENU_CANCEL
#define THEATER_PLAY_ALT PADRup
#else
#define THEATER_EXIT (MENU_CANCEL | PADRup)
#define THEATER_PLAY_ALT PADRleft
#endif

#define MOVIE_THEATER_SLOT_OPENING 0
#define MOVIE_THEATER_SLOT_LAST 6
#define MOVIE_THEATER_SLOT_COUNT 7
#define MOVIE_THEATER_REVEAL_FRAME 3
#define MOVIE_THEATER_FADE_FRAMES 32
#define MOVIE_THEATER_FADE_STEP 8
#define MOVIE_THEATER_FADE_TPAGE GET_TPAGE(0, 2, 0, 0)

#define MOVIE_ENDING_TRUE_BIT_OFFSET 6
#define MOVIE_ENDING_NORMAL_MASK 0x01U
#define MOVIE_ENDING_PAIR_MASK 0x41
#define MOVIE_ENDING_TRUE_MASK 0x40

enum {
    MOVIE_THEATER_SOUND_PREV = 0x1a,
    MOVIE_THEATER_SOUND_NEXT = 0x1b,
    MOVIE_THEATER_SOUND_CONFIRM = 0x1c,
    MOVIE_THEATER_SOUND_LOCKED = 0x1d,
    MOVIE_THEATER_SOUND_EXIT = 0x1e
};

unsigned char gMovieTheaterSlotToCharacter[8] = { 0, 4, 3, 1, 2, 5, 0, 0 };
static POLY_F4 sMovieTheaterFadeQuads[2];
static DR_TPAGE sMovieTheaterFadeTPages[2];

#ifndef VERSION_JP
signed char gMovieSubtitleTracks[16] = { -1, 0, 2, 4, 6, 8, 10, 1, 3, 5, 7, 9, 11, 0, 0, 0 };
#endif
void PlayFmvMovie(int movieId)
{
#ifdef VERSION_JP
    DISPENV savedDisp;
    DISPENV *disp = &db[gFrameCounter & 1].disp;
#else
    DRAWENV savedDraw[2];
    DISPENV savedDisp[2];
    int subtitleTrack;

    gSubtitleActive = 0;
    subtitleTrack = gMovieSubtitleTracks[movieId];
    if (subtitleTrack >= 0) {
        StartSubtitleById(subtitleTrack + SUBTITLE_ID_MOVIE_BASE);
        gSubtitleActive = 1;
    }
    CdInit();
    VSync(2);
#endif
    EnableCdSpuAudio();
    DrawSync(0);
#ifdef VERSION_JP
    savedDisp = *disp;
    do {
        PrepareMoviePlayback(movieId, DYNAMIC_LOAD_BUFFER);
    } while (StartMoviePlayback() != 0 && RunMoviePlaybackLoop(disp) == 0);
    *disp = savedDisp;
#else
    savedDisp[0] = db[0].disp;
    savedDisp[1] = db[1].disp;
    savedDraw[0] = db[0].draw;
    savedDraw[1] = db[1].draw;
    do {
        PrepareMoviePlayback(movieId, DYNAMIC_LOAD_BUFFER);
    } while (StartMoviePlayback() != 0 && RunMoviePlaybackLoop() == 0);
    db[0].disp = savedDisp[0];
    db[1].disp = savedDisp[1];
    db[0].draw = savedDraw[0];
    db[1].draw = savedDraw[1];
#endif
}

void UpdateMovieTheaterState(void)
{
    int options, enabled;
    int characterClass;

    gDisplayPresentEnabled = 1;
    if (++gStateFrameTimer == MOVIE_THEATER_REVEAL_FRAME) {
        SetDispMask(1);
    }
    if (gMovieTheaterFadeTimer == 0) {
        int step, slot;

        if (gPadState.pressed & BOTH_PADS(PADLright)) {
            PlayMenuSound(MOVIE_THEATER_SOUND_NEXT);
            step = 1;
        } else if (gPadState.pressed & BOTH_PADS(PADLleft)) {
            PlayMenuSound(MOVIE_THEATER_SOUND_PREV);
            step = -1;
        } else {
            step = 0;
        }
        do {
            gMovieTheaterSelection += step;
            if (gMovieTheaterSelection == -1) {
                gMovieTheaterSelection = MOVIE_THEATER_SLOT_LAST;
            }
            if (gMovieTheaterSelection == MOVIE_THEATER_SLOT_COUNT) {
                gMovieTheaterSelection = MOVIE_THEATER_SLOT_OPENING;
            }
        } while (gMovieTheaterSelection != MOVIE_THEATER_SLOT_OPENING &&
                 !(gUnlockedMoviesMask &
                   (MOVIE_ENDING_PAIR_MASK << gMovieTheaterSlotToCharacter[gMovieTheaterSelection])));

        characterClass = gMovieTheaterSlotToCharacter[gMovieTheaterSelection];
        if (gMovieTheaterSelection == MOVIE_THEATER_SLOT_OPENING) {
            options = 0;
        } else {
            options = ((gUnlockedMoviesMask >> characterClass) & 1) << 1 |
                       ((gUnlockedMoviesMask >> (characterClass + MOVIE_ENDING_TRUE_BIT_OFFSET)) & 1);
        }
        for (slot = 0, enabled = 0; slot < PLAYABLE_CHARACTER_COUNT; slot++) {
            if (gUnlockedMoviesMask & (MOVIE_ENDING_PAIR_MASK << gMovieTheaterSlotToCharacter[slot + 1])) {
                enabled |= 1 << (PLAYABLE_CHARACTER_COUNT - 1 - slot);
            }
        }
        DrawMovieTheaterMenu(gMovieTheaterSelection, options, enabled);

        if ((gPadState.pressed & BOTH_PADS(THEATER_EXIT)) && gStateFrameTimer >= MOVIE_THEATER_REVEAL_FRAME) {
            PlayMenuSound(MOVIE_THEATER_SOUND_EXIT);
            gGameState = GAME_STATE_INIT_MAIN_MENU;
        }
        if (gPadState.pressed & BOTH_PADS(MENU_CONFIRM)) {
            if (gMovieTheaterSelection == MOVIE_THEATER_SLOT_OPENING ||
                (gUnlockedMoviesMask & (MOVIE_ENDING_NORMAL_MASK << characterClass)) > 0) {
                PlayMenuSound(MOVIE_THEATER_SOUND_CONFIRM);
                gMovieTheaterFadeTimer = 1;
                gPendingMovieId = (gMovieTheaterSelection == MOVIE_THEATER_SLOT_OPENING)
                                 ? MOVIE_ID_OPENING
                                 : characterClass + MOVIE_ID_NORMAL_ENDING_BASE;
            } else {
                PlayMenuSound(MOVIE_THEATER_SOUND_LOCKED);
            }
        } else if (gPadState.pressed & BOTH_PADS(THEATER_PLAY_ALT)) {
            if (gMovieTheaterSelection != MOVIE_THEATER_SLOT_OPENING &&
                (gUnlockedMoviesMask & (MOVIE_ENDING_TRUE_MASK << characterClass))) {
                PlayMenuSound(MOVIE_THEATER_SOUND_CONFIRM);
                gMovieTheaterFadeTimer = 1;
                gPendingMovieId = characterClass + MOVIE_ID_TRUE_ENDING_BASE;
            } else {
                PlayMenuSound(MOVIE_THEATER_SOUND_LOCKED);
            }
        }
    } else if (++gMovieTheaterFadeTimer == MOVIE_THEATER_FADE_FRAMES) {
        PlayFmvMovie(gPendingMovieId);
        SetDispMask(0);
        DrawSync(0);
        LoadMovieTheaterAssets();
        LoadMenuSoundBanks();
        gDisplayPresentEnabled = 0;
        gStateFrameTimer = 0;
        gMovieTheaterFadeTimer = 0;
    } else {
        int bufferIndex = gFrameCounter & 1;
        POLY_F4 *fadeQuad = &sMovieTheaterFadeQuads[bufferIndex];
        DR_TPAGE *fadeTPage = &sMovieTheaterFadeTPages[bufferIndex];
        int slot;
        int fadeShade;

        characterClass = gMovieTheaterSlotToCharacter[gMovieTheaterSelection];
        if (gMovieTheaterSelection == MOVIE_THEATER_SLOT_OPENING) {
            options = 0;
        } else {
            options = ((gUnlockedMoviesMask >> characterClass) & 1) << 1 |
                       ((gUnlockedMoviesMask >> (characterClass + MOVIE_ENDING_TRUE_BIT_OFFSET)) & 1);
        }
        for (slot = 0, enabled = 0; slot < PLAYABLE_CHARACTER_COUNT; slot++) {
            if (gUnlockedMoviesMask & (MOVIE_ENDING_PAIR_MASK << gMovieTheaterSlotToCharacter[slot + 1])) {
                enabled |= 1 << (PLAYABLE_CHARACTER_COUNT - 1 - slot);
            }
        }
        SetPolyF4(fadeQuad);
        SetSemiTrans(fadeQuad, 1);
        fadeShade = gMovieTheaterFadeTimer * MOVIE_THEATER_FADE_STEP;
        fadeQuad->x0 = 0;
        fadeQuad->y0 = 0;
        fadeQuad->x1 = DISPLAY_WIDTH;
        fadeQuad->y1 = 0;
        fadeQuad->x2 = 0;
        fadeQuad->x3 = DISPLAY_WIDTH;
        fadeQuad->r0 = fadeShade;
        fadeQuad->g0 = fadeShade;
        fadeQuad->b0 = fadeShade;
        fadeQuad->y2 = DISPLAY_HEIGHT;
        fadeQuad->y3 = DISPLAY_HEIGHT;
        AddPrim(gCurrentOt, fadeQuad);
        SetDrawTPage(fadeTPage, 0, 1, MOVIE_THEATER_FADE_TPAGE);
        AddPrim(gCurrentOt, fadeTPage);
        DrawMovieTheaterMenu(gMovieTheaterSelection, options, enabled);
    }
    FlushQueuedSounds();
}
