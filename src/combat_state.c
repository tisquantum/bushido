/* The combat game state: its sound banks, stage ambient sounds and music, Slash mode opponents,
 * fighter input, and the InitCombatState and UpdateCombatState handlers. */

#include <sys/types.h>
#include <stdio.h>
#include <rand.h>
#include <libetc.h>
#include <libgte.h>
#include <libsn.h>

#include "pad.h"
#include "stage_ambient_sound.h"
#include "cd.h"
#include "collision_scratch.h"
#include "fighter.h"
#include "fighter_cloth.h"
#include "world_vector.h"
#include "game_state.h"
#include "link.h"
#include "model_arena.h"
#include "motion_file.h"
#include "cloth.h"
#include "rumble_pulse.h"
#include "camera.h"
#include "stage_callbacks.h"
#include "stage_tables.h"
#include "stage_id.h"
#include "combat_state.h"
#include "voice_state.h"
#include "menu_state.h"
#include "subtitle.h"
#include "asset_load.h"
#include "billboard_driver.h"
#include "collision.h"
#include "combat_event.h"
#include "fighter_ai.h"
#include "fighter_hit.h"
#include "fighter_pose.h"
#include "font_hud.h"
#include "main.h"
#include "match_setup.h"
#include "pose_stream.h"
#include "pov_overlay.h"
#include "scene.h"
#include "sightline.h"
#include "sound.h"
#include "stage_cull.h"
#include "subweapon.h"
#include "load_area.h"
#include "story_state.h"
#include "weapon_id.h"
#include "globals.h"

#define AMBIENT_FRAMES_PER_SEC                30 /* a second on NTSC;
                                                    1.2 s in the European build, which runs 25 frames a second */
#define YAHIRO_ROAD_AMBIENT_MIN_FIRST_FRAMES  30
#define YAHIRO_ROAD_AMBIENT_MAX_FIRST_FRAMES  151
#define YAHIRO_ROAD_AMBIENT_SECOND_FRAMES     91

#define SLASH_ENEMY_PRELOAD_BUFFER    ((int *)0x801b3000)
#define COMBAT_FOV_POV                80
#define COMBAT_CAMERA_INIT_PITCH      0x200
#define COMBAT_CAMERA_INIT_YAW_OFFSET 0x400
#define COMBAT_CAMERA_INIT_DISTANCE   8000
#define POV_MODEL_ID_OFFSET           26
#define SUBTITLE_ID_INTRO_TAUNT_BASE  0x48
#define POV_AI_SCALE_BASE             0xc00
#define SLASH_AI_SCALE_BASE           0xa00
#define SLASH_AI_FINAL_BOUT_BONUS     0x100
#define SLASH_BOUTS_PER_LEVEL         10
#define SLASH_FINAL_BOUT_INDEX        9
#define SLASH_LEVEL_COUNT             10
#define SLASH_FINAL_LEVEL_INDEX       9
#define SLASH_ENEMY_ID_BASE           19
#define SLASH_ALT_SOUND_VARIANT_ID    22
#define STORY_PRE_LINEAR_BOUT         4
#define STORY_LINEAR_BOUT_START       5

#define IS_KO_ACTION(action) \
    ((action) == FIGHTER_ACTION_DEAD || (action) == FIGHTER_ACTION_FATAL_KO || \
     (action) == FIGHTER_ACTION_DEFEATED)

#define FIGHTER_INPUT_HISTORY_LEN 8
#define REVERSE_DPAD(buttons) ( \
    ((buttons) & (~PAD_DPAD_MASK & 0xffff)) | \
    (((buttons) & PADLleft) >> 2) | (((buttons) & PADLright) << 2) | \
    (((buttons) & PADLdown) >> 2) | (((buttons) & PADLup) << 2))

#define FACE_BUTTONS (PADRup | PADRright | PADRdown | PADRleft)
#ifdef VERSION_JP
#define PAUSE_QUIT_BUTTONS (PADstart | PADselect)
#else
#define PAUSE_QUIT_BUTTONS (PADstart | PADselect | PADL1 | PADL2 | PADR1 | PADR2)
#endif
#define HUD_BANNER_CHALLENGER 1
#define COMBAT_PAUSE_MIN_FRAMES 5
#define POST_KO_DELAY_FRAMES 106
#define PRACTICE_WIN_RECORD_FRAME 105
#define SLASH_INTER_BOUT_DELAY_FRAMES 61
#define CHALLENGER_BANNER_FRAMES 61
#define RING_OUT_P2_LEAD_FRAMES 150
#define RING_OUT_P1_LEAD_FRAMES 210
#define SOUND_COMBAT_PAUSE 17
#define SOUND_CHALLENGER_JOIN 19

struct AmbientSoundTimer gAmbientSoundTimers[STAGE_AMBIENT_TIMER_COUNT];

unsigned char gStoryBoutStageIds[5] = {
    11, STAGE_ID_EXECUTIONERS_COVE, 13, STAGE_ID_MEIKYOKAN, STAGE_ID_YAGURA_POINT
};
unsigned char gCombatRoundCounter = 0;
short gMenuSoundBanksLoaded = 0;
int gSoundVabHeaderSize;
char gSoundVabHeaderWork[SOUND_VAB_HEADER_WORK_SIZE];

void LoadCombatSoundBanks(void)
{
    SoundLoadRequest request;
    int headerSize;

    gMenuSoundBanksLoaded = 0;
    WaitCdIdle();
    request.cdBuffer = DYNAMIC_LOAD_BUFFER;
    request.headerWork = gSoundVabHeaderWork;
    InitSoundSystem(gCurrentStageId);

    request.mode = SOUND_LOAD_WEAPON_PAIR;
    if (gPlayMode == PLAY_MODE_PRACTICE) {
        request.secondId = SOUND_WEAPON_PRACTICE_ID;
        request.firstId = SOUND_WEAPON_PRACTICE_ID;
    } else {
        if (gFighters[0].weaponPairSlot == 0) {
            request.firstId = gFighters[0].weaponId;
            request.secondId = gFighters[1].weaponId;
        } else {
            request.firstId = gFighters[1].weaponId;
            request.secondId = gFighters[0].weaponId;
        }
        if (request.firstId == 9) {
            request.firstId = WEAPON_ID_NAGINATA;
        }
        if (request.secondId == 9) {
            request.secondId = WEAPON_ID_NAGINATA;
        }
    }
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.firstId = gFighters[0].fighterId;
    request.secondId = gFighters[1].fighterId;
    if (request.firstId >= SOUND_FIGHTER_BANK_COUNT) {
        request.firstId = 0;
    }
    if (request.secondId >= SOUND_FIGHTER_BANK_COUNT) {
        request.secondId = 0;
    }
    request.mode = SOUND_LOAD_FIRST_FIGHTER;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_SECOND_FIGHTER;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_STAGE_AMBIENT;
    request.stageId = gCurrentStageId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    headerSize = CommitSoundBankLoad(&request);
    StartSoundSystem(gCurrentStageId);
    gSoundVabHeaderSize = headerSize;
    if (headerSize > SOUND_VAB_HEADER_WORK_SIZE) {
        printf("SE HEADER SIZR:%08x\n", headerSize);
        printf("SE HEADER SIZE OVER FLOW\n");
        for (;;) {
            pollhost();
        }
    }
}

void LoadCutsceneFighterSoundBanks(void)
{
    SoundLoadRequest request;
    int headerSize;

    gMenuSoundBanksLoaded = 0;
    WaitCdIdle();
    request.cdBuffer = DYNAMIC_LOAD_BUFFER;
    request.headerWork = gSoundVabHeaderWork;
    InitSoundSystem(-1);

    request.mode = SOUND_LOAD_WEAPON_PAIR;
    request.firstId = gFighters[0].weaponId;
    request.secondId = gFighters[1].weaponId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_FIRST_FIGHTER;
    request.firstId = gFighters[0].fighterId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_SECOND_FIGHTER;
    request.secondId = gFighters[1].fighterId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    headerSize = CommitSoundBankLoad(&request);
    StartSoundSystem(-1);
    gSoundVabHeaderSize = headerSize;
}

static void LoadSlashModeSoundBanks(void)
{
    SoundLoadRequest request;
    int headerSize;

    gMenuSoundBanksLoaded = 0;
    WaitCdIdle();
    request.cdBuffer = DYNAMIC_LOAD_BUFFER;
    request.headerWork = SLASH_SOUND_VAB_HEADER_WORK;
    InitSoundSystem(gCurrentStageId);

    request.mode = SOUND_LOAD_WEAPON_PAIR;
    request.secondId = 0;
    request.firstId = 0;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_FIRST_FIGHTER;
    request.firstId = gFighters[0].fighterId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_SLASH_MODE;
    request.stageId = gCurrentStageId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    headerSize = CommitSoundBankLoad(&request);
    StartSoundSystem(gCurrentStageId);
    gSoundVabHeaderSize = headerSize;
}

void LoadApproachSoundBanks(void)
{
    SoundLoadRequest request;
    int headerSize;

    gMenuSoundBanksLoaded = 0;
    WaitCdIdle();
    request.cdBuffer = GetGpuPacketBuffer(0);
    request.headerWork = gSoundVabHeaderWork;
    InitSoundSystem(gCurrentStageId);

    request.mode = SOUND_LOAD_FIRST_FIGHTER;
    request.firstId = gFighters[0].fighterId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);

    request.mode = SOUND_LOAD_STAGE_AMBIENT;
    request.stageId = gCurrentStageId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    headerSize = CommitSoundBankLoad(&request);
    StartSoundSystem(gCurrentStageId);
    gSoundVabHeaderSize = headerSize;
}

static void ReloadStageAmbientSoundBank(void)
{
    SoundLoadRequest request;

    request.cdBuffer = (unsigned int *)GetGpuPacketBuffer(0);
    request.headerWork = gSoundVabHeaderWork;
    request.mode = SOUND_LOAD_STAGE_AMBIENT_RELOAD;
    request.stageId = gCurrentStageId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    CommitSoundBankLoad(&request);
}

void LoadDialogueSoundBank(void)
{
    SoundLoadRequest request;

    gMenuSoundBanksLoaded = 0;
    WaitCdIdle();
    request.cdBuffer = DYNAMIC_LOAD_BUFFER;
    request.headerWork = gSoundVabHeaderWork;
    request.mode = SOUND_LOAD_DIALOGUE;
    request.firstId = gFighters[0].fighterId;
    request.secondId = gFighters[1].fighterId;
    RequestSoundBankLoad(&request);
    WaitCdIdle();
    gSoundVabHeaderSize = CommitSoundBankLoad(&request);
}

void LoadMenuSoundBanks(void)
{
    SoundLoadRequest request;

    if (gMenuSoundBanksLoaded != 1) {
        WaitCdIdle();
        request.cdBuffer = DYNAMIC_LOAD_BUFFER;
        request.headerWork = gSoundVabHeaderWork;
        InitSoundSystem(-1);
        request.mode = SOUND_LOAD_MENU;
        RequestSoundBankLoad(&request);
        WaitCdIdle();
        CommitSoundBankLoad(&request);
        request.mode = SOUND_LOAD_LINK_LOBBY;
        RequestSoundBankLoad(&request);
        WaitCdIdle();
        gSoundVabHeaderSize = CommitSoundBankLoad(&request);
        StartSoundSystem(-1);
        gMenuSoundBanksLoaded = 1;
    }
}

/* By fighter id - 12: the six boss classes (12 to 17), then ids 18 to 23, of which the Slash enemies are 19 to 22. */
unsigned char gBossDefaultWeapons[12] = {
    WEAPON_ID_NODACHI, WEAPON_ID_LONG_SWORD, WEAPON_ID_GUN, WEAPON_ID_KATANA, 9, WEAPON_ID_RAPIER,
    0, WEAPON_ID_KATANA, WEAPON_ID_KATANA, WEAPON_ID_KATANA, WEAPON_ID_KATANA, 0,
};

/* Opponent fighter id minus 6, by [character class][story bout]. */
unsigned char gStoryOpponentTable[6][10] = {
    { 4, 5, 2, 3, 1, 8, 10, 11, 9, 2 },
    { 0, 5, 2, 3, 4, 8, 10, 11, 9, 6 },
    { 1, 4, 5, 0, 3, 8, 10, 11, 9, 7 },
    { 2, 1, 5, 4, 0, 8, 10, 11, 9, 7 },
    { 1, 2, 3, 0, 5, 8, 10, 11, 9, 6 },
    { 3, 1, 4, 0, 2, 8, 10, 11, 9, 6 },
};

unsigned char gStoryOpponentWeapons[6][5] = {
    { WEAPON_ID_NODACHI, WEAPON_ID_SLEDGEHAMMER, WEAPON_ID_BROADSWORD, WEAPON_ID_KATANA, WEAPON_ID_NAGINATA },
    { WEAPON_ID_LONG_SWORD, WEAPON_ID_BROADSWORD, WEAPON_ID_RAPIER, WEAPON_ID_KATANA, WEAPON_ID_SABER },
    { WEAPON_ID_KATANA, WEAPON_ID_SABER, WEAPON_ID_BROADSWORD, WEAPON_ID_SLEDGEHAMMER, WEAPON_ID_NODACHI },
    { WEAPON_ID_SABER, WEAPON_ID_RAPIER, WEAPON_ID_KATANA, WEAPON_ID_LONG_SWORD, WEAPON_ID_NAGINATA },
    { WEAPON_ID_SABER, WEAPON_ID_SLEDGEHAMMER, WEAPON_ID_KATANA, WEAPON_ID_KATANA, WEAPON_ID_LONG_SWORD },
    { WEAPON_ID_SLEDGEHAMMER, WEAPON_ID_LONG_SWORD, WEAPON_ID_KATANA, WEAPON_ID_NODACHI, WEAPON_ID_RAPIER },
};

StageAmbientSound gStage0To2AmbientSounds[9] = {
    /* Stage 0 */
    { STAGE_AMBIENT_RANDOM, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM_PAIR, 5, 10, 127 },
    { STAGE_AMBIENT_PAIR_FOLLOWUP, 0, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_END, 5, 10, 127 },
    /* Stage 1 */
    { STAGE_AMBIENT_LOOP, 0, 0, 5 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
    /* Stage 2 */
    { STAGE_AMBIENT_RANDOM_PAIR, 5, 10, 127 },
    { STAGE_AMBIENT_PAIR_FOLLOWUP | STAGE_AMBIENT_END, 0, 10, 127 },
};

StageAmbientSound gStage3To8AmbientSounds[15] = {
    /* Stage 3 */
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_MANUAL, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 63 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 63 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
    /* Stage 4 */
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT | STAGE_AMBIENT_END, 5, 10, 127 },
    /* Stage 5 */
    { STAGE_AMBIENT_RANDOM, 2, 5, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_END, 5, 10, 127 },
    /* Stage 6 */
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
    /* Stage 7 */
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
    /* Stage 8 */
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
};

StageAmbientSound gStage9AmbientSounds[3] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 95 },
};

StageAmbientSound gStage10AmbientSounds[4] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT | STAGE_AMBIENT_END, 4, 10, 127 },
};

StageAmbientSound gStage11AmbientSounds[3] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
};

StageAmbientSound gCoveAmbientSounds[5] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY, 5, 10, 63 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_END, 5, 10, 63 },
};

StageAmbientSound gStage13AmbientSounds[5] = {
    { STAGE_AMBIENT_RANDOM_PAIR, 10, 10, 127 },
    { STAGE_AMBIENT_PAIR_FOLLOWUP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 111 },
    { STAGE_AMBIENT_RANDOM, 5, 10, 63 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_END, 5, 10, 127 },
};

StageAmbientSound gBambooThicketAmbientSounds[5] = {
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_MANUAL, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_NIGHT, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_NIGHT, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 95 },
};

StageAmbientSound gMeikyokanAmbientSounds[3] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 95 },
};

StageAmbientSound gCherryGroveAmbientSounds[4] = {
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 63 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_END, 5, 10, 127 },
};

StageAmbientSound gYaguraPointAmbientSounds[4] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 63 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_END, 5, 10, 63 },
};

StageAmbientSound gMoatAmbientSounds[5] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
};

StageAmbientSound gWarriorsDwellingAmbientSounds[7] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 127 },
    { STAGE_AMBIENT_RANDOM_PAIR, 5, 10, 127 },
    { STAGE_AMBIENT_PAIR_FOLLOWUP, 0, 0, 127 },
    { STAGE_AMBIENT_RANDOM_PAIR, 5, 10, 127 },
    { STAGE_AMBIENT_PAIR_FOLLOWUP, 0, 0, 127 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY | STAGE_AMBIENT_END, 0, 0, 95 },
};

StageAmbientSound gStage22AmbientSounds[4] = {
    { STAGE_AMBIENT_LOOP, 0, 0, 95 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_NIGHT, 5, 10, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_END, 5, 10, 127 },
};

StageAmbientSound gYahiroRoadAmbientSounds[4] = {
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 95 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_DAY, 0, 0, 95 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_MANUAL, 0, 0, 127 },
    { STAGE_AMBIENT_RANDOM | STAGE_AMBIENT_DAY | STAGE_AMBIENT_MANUAL | STAGE_AMBIENT_END, 0, 0, 127 },
};

StageAmbientSound gGrottoAmbientSounds[6] = {
    { STAGE_AMBIENT_RANDOM, 3, 10, 127 },
    { STAGE_AMBIENT_RANDOM, 3, 10, 127 },
    { STAGE_AMBIENT_RANDOM, 3, 10, 127 },
    { STAGE_AMBIENT_RANDOM, 1, 10, 127 },
    { STAGE_AMBIENT_LOOP, 0, 0, 111 },
    { STAGE_AMBIENT_LOOP | STAGE_AMBIENT_END, 0, 0, 127 },
};

StageAmbientSound *gStageAmbientSoundLists[STAGE_COUNT] = {
    gStage0To2AmbientSounds, gStage0To2AmbientSounds + 5,
    gStage0To2AmbientSounds + 7, gStage3To8AmbientSounds,
    gStage3To8AmbientSounds + 5, gStage3To8AmbientSounds + 7,
    gStage3To8AmbientSounds + 9, gStage3To8AmbientSounds + 11,
    gStage3To8AmbientSounds + 13, gStage9AmbientSounds,
    gStage10AmbientSounds, gStage11AmbientSounds,
    gCoveAmbientSounds, gStage13AmbientSounds,
    gBambooThicketAmbientSounds, gMeikyokanAmbientSounds,
    gCherryGroveAmbientSounds, gYaguraPointAmbientSounds,
    0, gMoatAmbientSounds,
    0, gWarriorsDwellingAmbientSounds,
    gStage22AmbientSounds, 0,
    gYahiroRoadAmbientSounds, gGrottoAmbientSounds,
};
unsigned short gActiveAmbientMask = 0;
/* 2 bytes; nothing reads or writes them. */
unsigned char gActiveAmbientMaskUnused0 = 0;
unsigned char gActiveAmbientMaskUnused1 = 0;
short gYahiroRoadAmbientState;

void StartStageAmbientSounds(void)
{
    int volumeScale;
    int isDaytime;
    StageAmbientSound *entry;
    int timerIndex;
    int slotIndex;
    unsigned char flags;
    int volume;

    volumeScale = STAGE_AMBIENT_VOLUME_NORMAL;
    isDaytime = GetDaytimeOption();
    if (gGameState == GAME_STATE_INIT_STORY_CUTSCENE || gGameState == GAME_STATE_UPDATE_STORY_CUTSCENE) {
        volumeScale = STAGE_AMBIENT_VOLUME_CUTSCENE;
    }
    entry = gStageAmbientSoundLists[gCurrentStageId];
    gActiveAmbientMask = 0;
    if (entry != 0) {
        for (timerIndex = STAGE_AMBIENT_TIMER_COUNT - 1; timerIndex >= 0; timerIndex--) {
            struct AmbientSoundTimer *timer = &gAmbientSoundTimers[timerIndex];
            timer->elapsedFrames = 0;
        }
        slotIndex = 0;
        for (;;) {
            flags = entry->flags;
            if ((flags & STAGE_AMBIENT_MODE_MASK) == STAGE_AMBIENT_LOOP &&
                (((flags & STAGE_AMBIENT_NIGHT) != 0 && isDaytime == 0) ||
                 ((flags & STAGE_AMBIENT_DAY) != 0 && isDaytime != 0) ||
                 (flags & STAGE_AMBIENT_TIME_MASK) == 0)) {
                if (gCurrentStageId == 1 && slotIndex == 1) {
                    if ((gAmbientSoundTimers[1].elapsedFrames & 0x1f) == 0) {
                        QueueSoundOnVoice(STAGE_AMBIENT_SOUND_BASE + 1, 0, 0, STAGE_AMBIENT_VOICE_BASE + 1);
                    }
                } else {
                    volume = (entry->volume * volumeScale) >> 12;
                    QueueSoundOnVoice(slotIndex + STAGE_AMBIENT_SOUND_BASE, volume, volume,
                        slotIndex + STAGE_AMBIENT_VOICE_BASE);
                }
                gActiveAmbientMask |= 1 << slotIndex;
            }
            if (entry->flags & STAGE_AMBIENT_END) {
                break;
            }
            entry++;
            slotIndex++;
        }
        gYahiroRoadAmbientState = YAHIRO_ROAD_AMBIENT_IDLE;
    }
}

static void StopStageAmbientSounds(void)
{
    int slot;

    for (slot = 0; slot < STAGE_AMBIENT_VOICE_COUNT; slot++) {
        StopSoundVoice(slot + STAGE_AMBIENT_VOICE_BASE);
    }
    gActiveAmbientMask = 0;
    for (slot = STAGE_AMBIENT_TIMER_COUNT - 1; slot >= 0; slot--) {
        struct AmbientSoundTimer *timer = &gAmbientSoundTimers[slot];
        timer->elapsedFrames = 0;
    }
}

WorldVector gStage0AmbientSoundPos = { 3827, 0, 36557 };
WorldVector gStage1AmbientSoundPos = { -11112, 0, 7568 };

void UpdateAmbientSounds(void)
{
    int volumeScale, isDaytime;
    int jitterOrStage;
    StageAmbientSound *entry;
    StageAmbientSound *roadSounds;
    struct AmbientSoundTimer *timer;
    int slotIndex, timerIndex, mode, chanceFrames, triggerThreshold;
    int soundId, voiceId, voiceBits;
    int leftPan, rightPan, monoVolume;

    volumeScale = STAGE_AMBIENT_VOLUME_NORMAL;
    isDaytime = GetDaytimeOption();
    if (gGameState == GAME_STATE_INIT_STORY_CUTSCENE || gGameState == GAME_STATE_UPDATE_STORY_CUTSCENE)
        volumeScale = STAGE_AMBIENT_VOLUME_CUTSCENE;
    entry = gStageAmbientSoundLists[gCurrentStageId];
    if (entry == 0)
        return;
    for (timerIndex = 0; timerIndex < STAGE_AMBIENT_TIMER_COUNT; timerIndex++) {
        struct AmbientSoundTimer *slot = &gAmbientSoundTimers[timerIndex];
        slot->elapsedFrames++;
    }
    slotIndex = 0;
    timer = gAmbientSoundTimers;
    soundId = STAGE_AMBIENT_SOUND_BASE;
    voiceId = STAGE_AMBIENT_VOICE_BASE;
    voiceBits = STAGE_AMBIENT_VOICE_BASE << 8;
    for (;; entry++, soundId++, voiceId++, timer++, voiceBits += 0x100, slotIndex++) {
        leftPan = entry->flags; /* reused, as in the original */
        if (!(leftPan & STAGE_AMBIENT_MANUAL)) {
            mode = entry->flags & STAGE_AMBIENT_MODE_MASK;
            if (mode == STAGE_AMBIENT_RANDOM || mode == STAGE_AMBIENT_RANDOM_PAIR) {
                if (((entry->flags & STAGE_AMBIENT_NIGHT) && !isDaytime)
                    || ((entry->flags & STAGE_AMBIENT_DAY) && isDaytime)
                    || !(entry->flags & STAGE_AMBIENT_TIME_MASK)) {
                    if (timer->elapsedFrames >= entry->minDelaySec * AMBIENT_FRAMES_PER_SEC) {
                        chanceFrames = entry->chanceDivisor * AMBIENT_FRAMES_PER_SEC;
                        if (chanceFrames == 0)
                            chanceFrames = 0xffff;
                        triggerThreshold = 0x1000 / chanceFrames;
                        leftPan = rand();
                        if ((leftPan & 0xfff) < triggerThreshold) {
                            leftPan = 0x100;
                            rand();
                            jitterOrStage = rand() & 0x3f;
                            rightPan = leftPan - jitterOrStage;
                            jitterOrStage = gCurrentStageId;
                            if (jitterOrStage == 0 && slotIndex < 2) {
                                PlaySpatialSound(SPATIAL_SOUND_TRIGGER_VOICE | voiceBits | soundId,
                                    (VECTOR *)&gStage0AmbientSoundPos);
                            } else {
                                StopSoundVoice(voiceId);
                                QueueSoundOnVoice(soundId,
                                    (u_int)(entry->volume * volumeScale * leftPan) >> 20,
                                    entry->volume * volumeScale * rightPan >> 20, voiceId);
                            }
                            gActiveAmbientMask |= 1 << slotIndex;
                            timer->elapsedFrames = 0;
                            if (mode == STAGE_AMBIENT_RANDOM_PAIR) {
                                StopSoundVoice(slotIndex + (STAGE_AMBIENT_VOICE_BASE + 1));
                                QueueSoundOnVoice(slotIndex + (STAGE_AMBIENT_SOUND_BASE + 1),
                                    (u_int)(entry[1].volume * volumeScale * leftPan) >> 20,
                                    entry[1].volume * volumeScale * rightPan >> 20,
                                    slotIndex + (STAGE_AMBIENT_VOICE_BASE + 1));
                            }
                        }
                    }
                }
            } else if (mode == STAGE_AMBIENT_LOOP) {
                if (gCurrentStageId == 1 && slotIndex == 1)
                    PlaySpatialSound(SPATIAL_SOUND_UPDATE_VOICE | ((STAGE_AMBIENT_VOICE_BASE + 1) << 8)
                            | (STAGE_AMBIENT_SOUND_BASE + 1),
                        (VECTOR *)&gStage1AmbientSoundPos);
            }
        }
        if (entry->flags & STAGE_AMBIENT_END)
            break;
    }
    if (gCurrentStageId != STAGE_ID_YAHIRO_ROAD || !isDaytime)
        return;
    roadSounds = gStageAmbientSoundLists[STAGE_ID_YAHIRO_ROAD];
    switch (gYahiroRoadAmbientState) {
    case YAHIRO_ROAD_AMBIENT_IDLE:
        rightPan = rand();
        if (rightPan & 0xff)
            return;
        monoVolume = roadSounds[2].volume * volumeScale >> 12;
        QueueSoundOnVoice(STAGE_AMBIENT_SOUND_BASE + 2, monoVolume, monoVolume, STAGE_AMBIENT_VOICE_BASE + 2);
        gYahiroRoadAmbientState = YAHIRO_ROAD_AMBIENT_PLAY_FIRST;
        gAmbientSoundTimers[2].elapsedFrames = 0;
        break;
    case YAHIRO_ROAD_AMBIENT_PLAY_FIRST:
        if (gAmbientSoundTimers[2].elapsedFrames < YAHIRO_ROAD_AMBIENT_MIN_FIRST_FRAMES)
            return;
        if ((rand() & 0xfff) < 27 || gAmbientSoundTimers[2].elapsedFrames >= YAHIRO_ROAD_AMBIENT_MAX_FIRST_FRAMES) {
            StopSoundVoice(STAGE_AMBIENT_VOICE_BASE + 2);
            monoVolume = roadSounds[2].volume * volumeScale >> 12;
            QueueSoundOnVoice(STAGE_AMBIENT_SOUND_BASE + 3, monoVolume, monoVolume, STAGE_AMBIENT_VOICE_BASE + 2);
            gYahiroRoadAmbientState = YAHIRO_ROAD_AMBIENT_PLAY_SECOND;
            gAmbientSoundTimers[2].elapsedFrames = 0;
        }
        break;
    case YAHIRO_ROAD_AMBIENT_PLAY_SECOND:
        if (gAmbientSoundTimers[2].elapsedFrames >= YAHIRO_ROAD_AMBIENT_SECOND_FRAMES) {
            StopSoundVoice(STAGE_AMBIENT_VOICE_BASE + 2);
            gYahiroRoadAmbientState = YAHIRO_ROAD_AMBIENT_IDLE;
        }
        break;
    }
}

void TriggerPositionalAmbientSound(int slotIndex, VECTOR *position)
{
    PlaySpatialSound(SPATIAL_SOUND_TRIGGER_VOICE | ((slotIndex + STAGE_AMBIENT_VOICE_BASE) << 8)
        | STAGE_AMBIENT_SOUND_BASE, position);
}

#ifndef VERSION_JP
short gSubtitleActive;
#endif
CVECTOR gSlashEnemyTintColor;

static void StartCombatBgm(void)
{
    if (gOptionBgmEnabled == 0)
        return;

    switch (gPlayMode) {
    case PLAY_MODE_SLASH:
        if (gSlashNeedsEnemyPreload == 0)
            return;
        EnableCdSpuAudio();
        StartXaTrack(XA_TRACK_SLASH_BGM);
        break;

    case PLAY_MODE_VS:
        if (gPovModeEnabled == 1)
            break;
        EnableCdSpuAudio();
        StartXaTrack((gCombatRoundCounter & 1) ? XA_TRACK_VS_BGM_A : XA_TRACK_VS_BGM_B);
        break;

    case PLAY_MODE_PRACTICE:
        EnableCdSpuAudio();
        StartXaTrack(XA_TRACK_PRACTICE_BGM);
        break;

    case PLAY_MODE_STORY:
    case PLAY_MODE_LINK:
        break;
    }

    SetCdAudioRepeat(1);
}

static void DrawWinCountHud(void)
{
    if (gPovModeEnabled != 1) {
        DrawVersusWinCounts(gVsWinCounts[0], gVsWinCounts[1]);
    }
}

void IncrementVsWinCounts(void)
{
    Fighter *fighters = gFighters;
    int side;
    int action;

    for (side = 0; side < 2; ++side) {
        action = fighters[side].action;
        if (IS_KO_ACTION(action) || (fighters[side].placement->flags & MOTION_FLAG_FATAL)) {
            ++gVsWinCounts[1 - side];
        }
    }
}

int CalcPovAiAnimSpeedScale(void)
{
    int scale;

    scale = (gVsWinCounts[0] << 10) / 10 + POV_AI_SCALE_BASE;
    if (scale > ONE) {
        scale = ONE;
    }
    return scale;
}

int CalcPovAiClashPowerScale(void)
{
    int scale;

    scale = (gVsWinCounts[0] << 10) / 10 + POV_AI_SCALE_BASE;
    if (scale > ONE) {
        scale = ONE;
    }
    return scale;
}

int CalcSlashAiAnimSpeed(void)
{
    int level;
    int bout;
    int bonus;
    int scale;

    level = gSlashLevelIndex;
    bout = gSlashBoutIndex;
    bonus = level << 7;
    scale = bonus + SLASH_AI_SCALE_BASE;
    if (bout == SLASH_FINAL_BOUT_INDEX) {
        scale = bonus + SLASH_AI_SCALE_BASE + SLASH_AI_FINAL_BOUT_BONUS;
    }
    if (scale > ONE) {
        scale = ONE;
    }
    return scale;
}

int CalcSlashAiClashPower(void)
{
    int level;
    int bonus;
    int scale;

    level = gSlashLevelIndex;
    bonus = level * 192;
    scale = bonus + SLASH_AI_SCALE_BASE;
    if (gSlashBoutIndex == SLASH_FINAL_BOUT_INDEX) {
        scale = bonus + SLASH_AI_SCALE_BASE + SLASH_AI_FINAL_BOUT_BONUS;
    }
    if (scale > ONE) {
        scale = ONE;
    }
    return scale;
}

CVECTOR gSlashEnemyTints[SLASH_LEVEL_COUNT + 1] = {
    { 0xe0, 0x70, 0x30, 0x00 },
    { 0x00, 0xc0, 0x00, 0x00 },
    { 0x60, 0x60, 0x60, 0x00 },
    { 0xff, 0xff, 0xff, 0x00 },
    { 0x20, 0xc0, 0xe0, 0x00 },
    { 0xff, 0x80, 0x00, 0x00 },
    { 0xff, 0x60, 0xc0, 0x00 },
    { 0x20, 0x40, 0xff, 0x00 },
    { 0xff, 0xe0, 0x00, 0x00 },
    { 0xff, 0x00, 0x00, 0x00 },
    { 0x80, 0x00, 0xff, 0x00 },
};

unsigned char gSlashBoutEnemyOffsets[SLASH_LEVEL_COUNT] = { 0, 0, 0, 0, 3, 3, 3, 0, 0, 3 };
unsigned char gSlashFinalBoutEnemyOffsets[SLASH_LEVEL_COUNT] = { 3, 3, 3, 2, 2, 2, 2, 1, 1, 1 };

unsigned char gSlashEnemyCharacterIds[] = { 19, 20, 21, 21 };

#ifndef VERSION_JP
signed char gStoryIntroTauntScripts[12] = { 0, 1, 2, 3, 4, 5, 11, 10, 6, 9, 7, 8 };
#endif

StageWind gStageWindTable[STAGE_COUNT] = {
    { 1024, 1024 },
    { 2048, 0 },
    { 4096, 3072 },
    { 1024, 2048 },
    { 3072, 2048 },
    { 1024, 3072 },
    { 4096, 0 },
    { 1024, 2048 },
    { 8192, 1536 },
    { 4096, 1536 },
    { 4096, 0 },
    { 4096, 3072 },
    { 1024, 0 },
    { 1024, 2048 },
    { 1024, 2048 },
    { 256, 0 },
    { 1024, 3072 },
    { 8192, 512 },
    { 4096, 0 },
    { 1024, 0 },
    { 256, 0 },
    { 1024, 2048 },
    { 4096, 0 },
    { 4096, 0 },
    { 1024, 1024 },
    { 1024, 2048 },
};

StageWind gStageWindUnused[] = { { 257, 257 }, { -255, 0 } }; /* 8 bytes nothing reads */

static int SelectSlashOpponent(void)
{
    int boutIndex = gSlashBoutIndex;
    int enemyOffset;

    if (boutIndex == SLASH_FINAL_BOUT_INDEX) {
        enemyOffset = gSlashFinalBoutEnemyOffsets[gSlashLevelIndex];
        gSlashEnemyTintColor = gSlashEnemyTints[gSlashLevelIndex + 1];
    } else if (gSlashLevelIndex == SLASH_FINAL_LEVEL_INDEX && boutIndex == 7) {
        gSlashEnemyTintColor = gSlashEnemyTints[SLASH_FINAL_LEVEL_INDEX];
        enemyOffset = 0;
    } else if (gSlashLevelIndex == SLASH_FINAL_LEVEL_INDEX && boutIndex == 8) {
        enemyOffset = 2;
        gSlashEnemyTintColor = gSlashEnemyTints[SLASH_FINAL_LEVEL_INDEX];
    } else {
        enemyOffset = gSlashBoutEnemyOffsets[gSlashLevelIndex];
        gSlashEnemyTintColor = gSlashEnemyTints[gSlashLevelIndex];
    }

    if (GetMonochromeOption()) {
        gSlashEnemyTintColor.r = gSlashEnemyTintColor.g = gSlashEnemyTintColor.b =
            (gSlashEnemyTintColor.r + gSlashEnemyTintColor.g + gSlashEnemyTintColor.b) / 3;
    }
    return enemyOffset + SLASH_ENEMY_ID_BASE;
}

static void AdvanceSlashWave(void)
{
    gSlashBoutIndex++;
    if (gSlashBoutIndex == SLASH_BOUTS_PER_LEVEL) {
        gSlashBoutIndex = 0;
        gSlashLevelIndex++;
    }
}

static int GetSlashEnemySoundVariant(void)
{
    int opponent = gFighters[1].fighterId;

    if (opponent == SLASH_ENEMY_ID_BASE)
        return (gSlashBoutIndex & 1) ? SLASH_ENEMY_ID_BASE : SLASH_ALT_SOUND_VARIANT_ID;
    return gSlashEnemyCharacterIds[opponent - SLASH_ENEMY_ID_BASE];
}

char gPovOverlayModelBuffer[POV_OVERLAY_BUFFER_SIZE];

void InitCombatState(void)
{
    int isCpu[2];
    int stageChanged;
    int playMode, exitPlayMode;
    int humanPlayerCount;
    int povPreset;
    int stageId;
    int storyBout;
    int boutIndex;
    short charClass;
#ifndef VERSION_JP
    int tauntOffset;
#endif
    int p1FighterId;
    int p2FighterId;
    int p1ModelId, p2ModelId, p1WeaponId, p2WeaponId;
    StageTransition *transition;
    int reverse;
    WorldVector delta;
    MotionFileHeader *motionFile;

    BeginLinkSyncPause();
    StopStageAmbientSounds();
    WaitGpuIdle();
    if (gIsContinueRestart == 0 && gPrevStageId == gCurrentStageId)
        ShowNowLoadingOverlay();

    if (gPovModeEnabled == 1) {
        SetGeomScreen(CalcGeomScreenFromFov(COMBAT_FOV_POV));
        povPreset = 1;
    } else {
        SetGeomScreen(CalcGeomScreenFromFov(DEFAULT_SCENE_FOV));
        povPreset = 0;
    }
    LoadVisibilityMaskPreset(povPreset);
    humanPlayerCount = gHumanPlayerCount;
    isCpu[1] = 0;
    isCpu[0] = 0;
    if (humanPlayerCount == 1)
        isCpu[1] = humanPlayerCount;

    playMode = gPlayMode;
#ifndef VERSION_JP
    gSubtitleActive = 0;
#endif
    if (playMode == PLAY_MODE_STORY) {
        if (gPrevStageId == gCurrentStageId) {
            boutIndex = gStoryBoutIndex;
            if (boutIndex >= STORY_LINEAR_BOUT_START) {
                stageId = gStoryBoutStageIds[boutIndex - STORY_LINEAR_BOUT_START];
                gCurrentStageId = stageId;
                gPrevStageId = stageId;
            } else if (boutIndex == STORY_PRE_LINEAR_BOUT ||
                       (unsigned)(unsigned short)gStoryEventResult - STORY_EVENT_STAGE9_BOTH_FALL < 2) {
                stageId = STAGE_ID_UNDERGROUND_GROTTO;
                gCurrentStageId = stageId;
                gPrevStageId = stageId;
            } else {
                gStoryBoutStartPos = *(WorldVector *)&gFighters[0].position;
            }
            gStoryHonorWindowActive = 1;
            gStoryHonorWindowTimer = 5;
            gFighterAltTextureLoaded[1] = 0;
            if (gStoryChallengerActive != 0 && gFighterAltTextureLoaded[0] == 1)
                gFighterAltTextureLoaded[0] = 2;
        }

        if (gStoryChallengerActive == 0) {
            if (gAiMoveCycleMode == 0) {
                charClass = gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT;
                storyBout = gStoryBoutIndex;
                gSelectedFighter[1] = gStoryOpponentTable[charClass][storyBout] + PLAYABLE_CHARACTER_COUNT;
                if (storyBout == STORY_TRUE_ENDING_BOUT && charClass == 0) {
                    gSelectedWeapon[1] = WEAPON_ID_LONG_SWORD;
                } else if (gStoryBoutIndex < STORY_LINEAR_BOUT_START) {
                    charClass = gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT;
                    gSelectedWeapon[1] = gStoryOpponentWeapons[charClass][gStoryBoutIndex];
                }
            } else {
                charClass = gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT;
                gSelectedFighter[1] = gStoryOpponentTable[charClass][gStoryBoutIndex] + PLAYABLE_CHARACTER_COUNT;
            }
#ifndef VERSION_JP
            tauntOffset = gStoryIntroTauntScripts[gSelectedFighter[1] - PLAYABLE_CHARACTER_COUNT];
            if (tauntOffset >= 0 && gPrevStageId == gCurrentStageId) {
                StartSubtitleById(tauntOffset + SUBTITLE_ID_INTRO_TAUNT_BASE);
                gSubtitleActive = 1;
            }
#endif
        }
        if (gStoryBoutIndex == STORY_LINEAR_BOUT_START && gFighterAltTextureLoaded[0] != 0) {
            gFighterAltTextureLoaded[0] = 0;
            InvalidateArenaGroup(0);
        }
        if (gCurrentStageId == 9)
            gReachedStage9Flag = 1;

        if (gStoryChallengerActive != 0) {
            if (((gStoryDefeatedMask >> (gSelectedFighter[1] % PLAYABLE_CHARACTER_COUNT)) & 1))
                /* a boss reads class id % 6's bit, not its own */
                gFighterInjuredRegions[1] = INJURY_OVERLAY_ALL;
            else
                gFighterInjuredRegions[1] = 0;
        } else {
            if (((gSelectedFighter[0] % PLAYABLE_CHARACTER_COUNT) == 0 && gStoryBoutIndex == STORY_TRUE_ENDING_BOUT))
                gFighterInjuredRegions[1] = INJURY_OVERLAY_ALL;
            else if (((gStoryDefeatedMask >> (gSelectedFighter[1] % PLAYABLE_CHARACTER_COUNT)) & 1))
                /* a boss reads class id % 6's bit, not its own */
                gFighterInjuredRegions[1] = INJURY_OVERLAY_ALL;
            else
                gFighterInjuredRegions[1] = 0;
        }
        SetFighterInjuryOverlayMask(0, gFighterInjuredRegions[0]);
        SetFighterInjuryOverlayMask(1, gFighterInjuredRegions[1]);
    } else if (playMode == PLAY_MODE_SLASH) {
        gSlashBoutIndex = 0;
        gSelectedFighter[1] = SelectSlashOpponent();
    }

    if (gPlayMode != PLAY_MODE_STORY && gIsContinueRestart == 0) {
        SetFighterInjuryOverlayMask(0, 0);
        SetFighterInjuryOverlayMask(1, 0);
    }
    p1FighterId = gSelectedFighter[0];
    if (p1FighterId >= FIGHTER_PLAYABLE_ID_COUNT)
        gSelectedWeapon[0] = gBossDefaultWeapons[p1FighterId - FIGHTER_PLAYABLE_ID_COUNT];
    p2FighterId = gSelectedFighter[1];
    if (p2FighterId >= FIGHTER_PLAYABLE_ID_COUNT)
        gSelectedWeapon[1] = gBossDefaultWeapons[p2FighterId - FIGHTER_PLAYABLE_ID_COUNT];

    InitFighterForScene(gCurrentStageId, gPrevStageId, 0, isCpu[0], p1FighterId, gSelectedWeapon[0], 0);
    InitFighterForScene(gCurrentStageId, gPrevStageId, 1, isCpu[1], gSelectedFighter[1], gSelectedWeapon[1], 0);
    AssignFighterPairSlots();
    stageChanged = gPrevStageId != gCurrentStageId;
    SetPracticeWeaponTextureMode(gPlayMode == PLAY_MODE_PRACTICE);

    if (!stageChanged) {
        int midX, midY, midZ;

        gBattleCameraLocked = 0;
        midX = (gFighters[0].position.vx + gFighters[1].position.vx) / 2;
        midY = (gFighters[0].position.vy + gFighters[1].position.vy) / 2;
        midZ = (gFighters[0].position.vz + gFighters[1].position.vz) / 2;
        /* the original has a test here that is never true; what it tests is not known */
        if (gBattleCameraLocked != 0 && stageChanged)
            gBattleCameraLocked = 0;
        gCameraState.target.vx = midX;
        gCameraState.target.vy = midY;
        gCameraState.target.vz = midZ;
        gCameraState.angles.vx = COMBAT_CAMERA_INIT_PITCH;
        gCameraState.angles.vy = COMBAT_CAMERA_INIT_YAW_OFFSET - (unsigned short)gFighters[0].angles.heading;
        gCameraState.angles.vz = 0;
        gCameraState.distance = COMBAT_CAMERA_INIT_DISTANCE;
        gCameraOffset.target.vx = 0;
        gCameraOffset.target.vy = 0;
        gCameraOffset.target.vz = 0;
        gCameraOffset.angles.vx = 0;
        gCameraOffset.angles.vy = 0;
        gCameraOffset.angles.vz = 0;
        gCameraOffset.distance = 0;

        if (gIsContinueRestart == 0) {
            InitBillboardEffects();
            if (gPlayMode == PLAY_MODE_SLASH)
                LoadSlashModeSoundBanks();
            else
                LoadCombatSoundBanks();

            if (gPovModeEnabled == 1) {
                if (gPlayMode == PLAY_MODE_LINK && gLinkRole == LINK_ROLE_CLIENT) {
                    LoadPovOverlayModel(1, gPovOverlayModelBuffer);
                    p1ModelId = gFighters[0].fighterId;
                    p1WeaponId = gFighters[0].weaponId;
                    p2ModelId = gFighters[1].characterClass;
                    p2ModelId += POV_MODEL_ID_OFFSET;
                    p2WeaponId = gFighters[1].weaponId;
                } else {
                    LoadPovOverlayModel(0, gPovOverlayModelBuffer);
                    p2ModelId = gFighters[1].fighterId;
                    p1WeaponId = gFighters[0].weaponId;
                    p1ModelId = gFighters[0].characterClass;
                    p1ModelId += POV_MODEL_ID_OFFSET;
                    p2WeaponId = gFighters[1].weaponId;
                }
            } else {
                p1ModelId = gFighters[0].fighterId;
                p2ModelId = gFighters[1].fighterId;
                p1WeaponId = gFighters[0].weaponId;
                p2WeaponId = gFighters[1].weaponId;
            }
            LoadMatchFighters(p1ModelId, p2ModelId, p1WeaponId, p2WeaponId);
            LoadWeaponPairMotions();
            while (PollCdReady() == 0)
                VSync(0);
            InitPoseStream(MOTION_FILE->poseStream);
        } else if (gPovModeEnabled == 1) {
            if (gPlayMode == PLAY_MODE_LINK && gLinkRole == LINK_ROLE_CLIENT) {
                LoadPovOverlayModel(1, gPovOverlayModelBuffer);
                LoadMatchFighters(gFighters[0].fighterId, gFighters[1].characterClass + POV_MODEL_ID_OFFSET,
                                  gFighters[0].weaponId, gFighters[1].weaponId);
            } else {
                LoadPovOverlayModel(0, gPovOverlayModelBuffer);
                LoadMatchFighters(gFighters[0].characterClass + POV_MODEL_ID_OFFSET, gFighters[1].fighterId,
                                  gFighters[0].weaponId, gFighters[1].weaponId);
            }
        } else if (gPlayMode == PLAY_MODE_SLASH) {
            LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId,
                              gFighters[0].weaponId, gFighters[1].weaponId);
            SelectSlashEnemySoundBank(GetSlashEnemySoundVariant());
        }
    } else {
        transition = gPendingTransition;
        reverse = transition->stageA != gCurrentStageId;
        delta.x = transition->anchors[reverse].x - transition->anchors[!reverse].x;
        delta.y = transition->anchors[reverse].y - transition->anchors[!reverse].y;
        delta.z = transition->anchors[reverse].z - transition->anchors[!reverse].z;
        gCameraState.target.vx += delta.x;
        gCameraState.target.vy += delta.y;
        gCameraState.target.vz += delta.z;
        ReloadStageAmbientSoundBank();
    }
    if (gPlayMode == PLAY_MODE_SLASH)
        SelectSlashEnemySoundBank(GetSlashEnemySoundVariant());

    LoadCombatStage(gCurrentStageId);
    if (gPlayMode == PLAY_MODE_SLASH) {
        if (gSlashNeedsEnemyPreload != 0)
            PreloadSlashEnemyGroup(SLASH_ENEMY_PRELOAD_BUFFER);
        TintSlashEnemyCluts(gSlashEnemyTintColor.r, gSlashEnemyTintColor.g, gSlashEnemyTintColor.b);
    }
    if (!stageChanged) {
        Fighter *fighters = gFighters;
        motionFile = MOTION_FILE;
        SetFighterMotionPlacement(&fighters[0], MOTION_FILE->startPlacements[gFighters[0].weaponPairSlot]);
        if (gPlayMode == PLAY_MODE_STORY &&
            gStoryChallengerActive == 0 &&
            gStoryEventResult != STORY_EVENT_STAGE9_BOTH_FALL) {
            SetFighterMotionPlacement(&gFighters[1],
                motionFile->storyIntroPlacements[gFighters[1].characterClass][gFighters[1].weaponPairSlot]);
        } else
            SetFighterMotionPlacement(&gFighters[1], MOTION_FILE->startPlacements[gFighters[1].weaponPairSlot]);
    }
    InitStageSnowIfEnabled();
    StartStageAmbientSounds();
    StartCombatBgm();
    if (gIsContinueRestart == 0)
        gCombatRoundCounter++;
    exitPlayMode = gPlayMode;
    gStoryEventResult = 0;
    gIsContinueRestart = 0;
    gPracticeFirstBoutFlag = 0;
    if (exitPlayMode == PLAY_MODE_SLASH)
        gSlashNeedsEnemyPreload = 0;
    stageId = (unsigned short)gCurrentStageId;
    gStateFrameTimer = 0;
    gCombatPaused = 0;
    gPrevStageId = stageId;
    RequestClothSnapReset();
    gDisplayPresentEnabled = 1;
    gGameState = GAME_STATE_UPDATE_COMBAT;
    WaitLinkSyncBarrier();
}

static int CheckStageTransition(void)
{
    int playMode;
    int p1Action;
    int p2Action;
    StageTransition *transition;

    playMode = gPlayMode;
    if (playMode == PLAY_MODE_PRACTICE || playMode == PLAY_MODE_LINK || playMode == PLAY_MODE_VS ||
        gBladeLockState != BLADE_LOCK_NONE) {
        return gCurrentStageId;
    }
    p1Action = gFighters[0].action;
    if (p1Action != FIGHTER_ACTION_NEUTRAL && p1Action != FIGHTER_ACTION_CROUCH &&
        p1Action != FIGHTER_ACTION_DOWNED) {
        return gCurrentStageId;
    }
    p2Action = gFighters[1].action;
    if (p2Action != FIGHTER_ACTION_NEUTRAL && p2Action != FIGHTER_ACTION_CROUCH &&
        p2Action != FIGHTER_ACTION_DOWNED) {
        return gCurrentStageId;
    }
    transition = FindActiveStageTransition();
    if (transition != 0) {
        gPendingTransition = transition;
        return transition->stageB;
    }
    return gCurrentStageId;
}

/* The last eight frames' held buttons per player; read only to shift them. */
unsigned short gFighterInputHistory[FIGHTER_INPUT_HISTORY_LEN][2];

void BuildFighterInput(int player, PadInputFrame *out)
{
    int frame;

    if (gPlayMode == PLAY_MODE_STORY && gStoryChallengerTimer != 0) {
        out->notHeld = 0xffff;
        out->released = 0;
        out->pressed = 0;
        out->held = 0;
        out->type[player] = PAD_TYPE_DIGITAL;
        return;
    }

    if (gFighters[player].isCpu) {
        UpdateFighterAiInput(player, out);
    } else {
        *out = gPadState;
        out->held = (gPadState.held >> (player * 16)) & 0xffff;
        out->pressed = (gPadState.pressed >> (player * 16)) & 0xffff;
        out->released = (gPadState.released >> (player * 16)) & 0xffff;
        out->notHeld = (gPadState.notHeld >> (player * 16)) & 0xffff;
        if (player == 1 && gPlayMode != PLAY_MODE_LINK) {
            out->held = REVERSE_DPAD(out->held);
            out->pressed = REVERSE_DPAD(out->pressed);
            out->released = REVERSE_DPAD(out->released);
            out->notHeld = REVERSE_DPAD(out->notHeld);
            out->analog[1][PAD_ANALOG_LEFT_X] = 0xff - out->analog[1][PAD_ANALOG_LEFT_X];
            out->analog[1][PAD_ANALOG_LEFT_Y] = 0xff - out->analog[1][PAD_ANALOG_LEFT_Y];
        }
    }

    for (frame = 0; frame < FIGHTER_INPUT_HISTORY_LEN - 1; ++frame)
        gFighterInputHistory[frame][player] = gFighterInputHistory[frame + 1][player];
    gFighterInputHistory[FIGHTER_INPUT_HISTORY_LEN - 1][player] = out->held;
}

void StopCombatRumbleAndAudio(void)
{
    gRumblePulses[0].dutyStep = 0;
    gRumblePulses[1].dutyStep = 0;
    gFighters[0].rumbleTimer = 0;
    gFighters[1].rumbleTimer = 0;
    StopPriorityVoice0();
    StopPriorityVoice1();
    StopStageAmbientSounds();
}

void UpdateSceneCameraAndWind(void)
{
    const StageWind *wind;
    int side;
    int magnitude, angle;
    int stageId;
    MATRIX *cameraMatrix;

    BeginSceneFrame(&gCameraState.target, &gCameraState.angles, gCameraState.distance);
    ApplyCameraOcclusionMarkers();
    cameraMatrix = GetAspectCorrectedCameraMatrix();
    wind = &gStageWindTable[gCurrentStageId];
    magnitude = wind->magnitude;
    angle = wind->angle;
    gCameraMatrix = cameraMatrix;
    SetClothWindTarget(magnitude, angle, CLOTH_WIND_SLOT_STAGE);

    for (side = 0; side < 2; side++) {
        stageId = gCurrentStageId;
        if (stageId == 8 || stageId == STAGE_ID_YAGURA_POINT) {
            if (gFighters[side].position.vy > 0)
                SetClothWindTarget(wind->magnitude / 256, wind->angle, side);
            else
                SetClothWindTarget(wind->magnitude, wind->angle, side);
        } else if (gFighters[side].fighterId == 17) {
            SetClothWindTarget(0, 0, side);
        } else {
            SetClothWindTarget(wind->magnitude, wind->angle, side);
        }
    }
}

void UpdateCombatState(void)
{
    PadInputFrame fighterInput;
    int paused, humanPlayerCount, povMode, playMode, action;
    int nextStage, stageId;
    short prevStage;
    unsigned int heldButtons;

    paused = gCombatPaused;
    gStateFrameTimer++;
    if (paused) {
        humanPlayerCount = gHumanPlayerCount;
        if ((humanPlayerCount == 2 && (gPadState.pressed & BOTH_PADS_START)) ||
            (gPadState.pressed & PADstart)) {
            gCombatPaused = 0;
            StartStageAmbientSounds();
            SetCdAudioMix(SOUND_MAX_VOLUME, 0, SOUND_MAX_VOLUME, 0);
        } else {
            if (humanPlayerCount == 2) {
                heldButtons = gPadState.held;
                if ((heldButtons & PAUSE_QUIT_BUTTONS) == PAUSE_QUIT_BUTTONS ||
                    (heldButtons & _PAD(1, PAUSE_QUIT_BUTTONS)) == _PAD(1, PAUSE_QUIT_BUTTONS)) {
                    gCombatPaused = 0;
                    gGameState = GAME_STATE_INIT_MAIN_MENU;
                    return;
                }
            } else if ((gPadState.held & PAUSE_QUIT_BUTTONS) == PAUSE_QUIT_BUTTONS) {
                gCombatPaused = 0;
                gGameState = GAME_STATE_INIT_MAIN_MENU;
                return;
            }
            FlushQueuedSounds();
#ifndef VERSION_JP
            ShowPauseOverlay();
#endif
            return;
        }
    } else if (gStateFrameTimer >= COMBAT_PAUSE_MIN_FRAMES &&
               ((gHumanPlayerCount == 2 && (gPadState.pressed & BOTH_PADS_START)) ||
                (gPadState.pressed & PADstart))) {
        gCombatPaused = 1;
        gDisplayPresentEnabled = 0;
        StopCombatRumbleAndAudio();
        QueuePriorityVoice0(SOUND_COMBAT_PAUSE, SOUND_MAX_VOLUME, SOUND_MAX_VOLUME);
        SetCdAudioMix(0, 0, 0, 0);
        return;
    }

    povMode = gPovModeEnabled;
    gDisplayPresentEnabled = 1;
    if (povMode == 0) {
        int isRunning = 0;
        WorldVector *p1CameraTarget = &gFighters[0].cameraTargetPos;
        WorldVector *p2CameraTarget = &gFighters[1].cameraTargetPos;

        if ((gFighters[0].action == FIGHTER_ACTION_NEUTRAL && (gFighters[0].animInfo->flags & ANIM_FLAG_RUN)) ||
            (gFighters[1].action == FIGHTER_ACTION_NEUTRAL && (gFighters[1].animInfo->flags & ANIM_FLAG_RUN)))
            isRunning = 1;
        UpdateBattleCamera(0, (VECTOR *)p1CameraTarget, (VECTOR *)p2CameraTarget, 0, isRunning, gBladeLockState);
    } else if (povMode == 1) {
        if (gPlayMode == PLAY_MODE_LINK && gLinkRole == LINK_ROLE_CLIENT)
            UpdatePovCamera(&gFighters[1]);
        else
            UpdatePovCamera(&gFighters[0]);
    }

    UpdateSceneCameraAndWind();
    UpdateCombatDistanceAndKoTimer();
    BuildFighterInput(0, &fighterInput);
    UpdateFighterFrame(0, &fighterInput);
    BuildFighterInput(1, &fighterInput);
    UpdateFighterFrame(1, &fighterInput);
    UpdateCombatCollisions();
    UpdateAndDrawSubWeapons();
    UpdateStoryHonorWindow();

    povMode = gPovModeEnabled;
    gCombatFrameCounter++;
    if (povMode == 1) {
        int heading = (gPlayMode == PLAY_MODE_LINK && gLinkRole == LINK_ROLE_CLIENT)
                    ? gFighters[1].angles.heading : gFighters[0].angles.heading;
        UpdatePovOverlayModel(heading + gCameraState.angles.vy);
    } else {
        TraceCameraSightlineTo(gFighters[0].position.vx, gFighters[0].position.vz);
        TraceCameraSightlineTo(gFighters[1].position.vx, gFighters[1].position.vz);
    }
    UpdateStageEvents();
    EndSceneFrame();
    UpdateAmbientSounds();
    FlushQueuedSounds();
#ifndef VERSION_JP
    if (gSubtitleActive)
        UpdateAndDrawSubtitle(0);
#endif

    playMode = gPlayMode;
    if (playMode == PLAY_MODE_PRACTICE) {
        DrawWinCountHud();
        if (gPostKoTimer == PRACTICE_WIN_RECORD_FRAME && !gPracticeWinRecorded) {
            IncrementVsWinCounts();
            gPracticeWinRecorded = 1;
        } else if (gPostKoTimer >= POST_KO_DELAY_FRAMES) {
            action = gFighters[0].action;
            gPracticeWinRecorded = 0;
            if ((IS_KO_ACTION(action) ||
                 (gFighters[0].placement->flags & MOTION_FLAG_FATAL) ||
                 action == FIGHTER_ACTION_NEUTRAL || action == FIGHTER_ACTION_CROUCH ||
                 action == FIGHTER_ACTION_DOWNED || action == FIGHTER_ACTION_ATTACK) &&
                (gPadState.pressed & FACE_BUTTONS)) {
                stageId = gCurrentStageId;
                InitFighterForScene(stageId, stageId, 0, gFighters[0].isCpu, gFighters[0].fighterId,
                                    gFighters[0].weaponId, 1);
                SetFighterMotionPlacement(&gFighters[0], MOTION_FILE->startPlacements[gFighters[0].weaponPairSlot]);
                stageId = gCurrentStageId;
                InitFighterForScene(stageId, stageId, 1, gFighters[1].isCpu, gFighters[1].fighterId,
                                    gFighters[1].weaponId, 1);
                SetFighterMotionPlacement(&gFighters[1], MOTION_FILE->startPlacements[gFighters[1].weaponPairSlot]);
                RequestClothSnapReset();
                gDisplayPresentEnabled = 0;
            }
        }
    } else if (playMode == PLAY_MODE_SLASH) {
        unsigned int koTimer;
        int slashLevel = gSlashLevelIndex;
        int slashBout;

        DrawSlashModeHud(slashLevel + 1, slashLevel * SLASH_BOUTS_PER_LEVEL + gSlashBoutIndex + 1);
        if (gMatchWinnerSide) {
            if (gPostKoTimer >= POST_KO_DELAY_FRAMES)
                gGameState = GAME_STATE_INIT_BATTLE_REPLAY;
        } else {
            slashBout = gSlashBoutIndex;
            koTimer = gPostKoTimer;
            if ((slashBout == SLASH_FINAL_BOUT_INDEX && koTimer >= POST_KO_DELAY_FRAMES) ||
                (slashBout != SLASH_FINAL_BOUT_INDEX && koTimer >= SLASH_INTER_BOUT_DELAY_FRAMES)) {
                AdvanceSlashWave();
                if (gSlashLevelIndex == SLASH_LEVEL_COUNT) {
                    gGameState = GAME_STATE_INIT_BATTLE_REPLAY;
                } else {
                    int slashOpponent = SelectSlashOpponent();
                    stageId = gCurrentStageId;
                    InitFighterForScene(stageId, stageId, 1, 1, slashOpponent, 0, 1);
                    SetFighterMotionPlacement(&gFighters[1], MOTION_FILE->startPlacements[gFighters[1].weaponPairSlot]);
                    WaitGpuIdle();
                    LoadMatchFighters(gFighters[0].fighterId, gFighters[1].fighterId, gFighters[0].weaponId,
                                      gFighters[1].weaponId);
                    TintSlashEnemyCluts(gSlashEnemyTintColor.r, gSlashEnemyTintColor.g, gSlashEnemyTintColor.b);
                    SelectSlashEnemySoundBank(GetSlashEnemySoundVariant());
                    LoadCombatStage(gCurrentStageId);
                    gDisplayPresentEnabled = 0;
                }
            }
        }
    } else if (playMode == PLAY_MODE_STORY) {
        if (gStoryChallengerTimer > 0) {
            DrawHudBanner(HUD_BANNER_CHALLENGER);
            if (++gStoryChallengerTimer >= CHALLENGER_BANNER_FRAMES)
                gGameState = GAME_STATE_INIT_CHARACTER_SELECT;
        } else if (!gStoryChallengerActive &&
                   !IS_KO_ACTION(gFighters[0].action) &&
                   !(gFighters[0].placement->flags & MOTION_FLAG_FATAL) &&
                   !IS_KO_ACTION(gFighters[1].action) &&
                   !(gFighters[1].placement->flags & MOTION_FLAG_FATAL) &&
                   (gPadState.pressed & _PAD(1, PADstart))) {
            gStoryChallengerTimer = 1;
            StopPriorityVoice0();
            QueuePriorityVoice0(SOUND_CHALLENGER_JOIN, SOUND_MAX_VOLUME, SOUND_MAX_VOLUME);
        } else if (gPostKoTimer >= POST_KO_DELAY_FRAMES) {
            if (!gMatchWinnerSide && !gStoryChallengerActive)
                gStoryBoutIndex++;
            gGameState = GAME_STATE_INIT_BATTLE_REPLAY;
        } else if (!gStoryChallengerActive) {
            int p1PitTimer = gFighters[0].pitFallTimer;
            int p2PitTimer = gFighters[1].pitFallTimer;
            if (p1PitTimer + RING_OUT_P2_LEAD_FRAMES < p2PitTimer ||
                p2PitTimer + RING_OUT_P1_LEAD_FRAMES < p1PitTimer) {
                gStoryEventResult = STORY_EVENT_RING_OUT;
                gGameState = GAME_STATE_INIT_VICTORY_SCENE;
                gMatchWinnerSide = 0; /* player one wins, whichever fighter fell */
                gStoryBoutIndex++;
                gStoryFallCount++;
            } else if (gCurrentStageId == 9 && p1PitTimer > 0) {
                if (p2PitTimer > 0) {
                    gStoryEventResult = STORY_EVENT_STAGE9_BOTH_FALL;
                } else {
                    action = gFighters[1].action;
                    gStoryEventResult = STORY_EVENT_STAGE9_P1_FALL;
                    if (IS_KO_ACTION(action) ||
                        (gFighters[1].placement->flags & MOTION_FLAG_FATAL) ||
                        gFighters[1].legsIntact == 0) {
                        gStoryBoutIndex = STORY_PRE_LINEAR_BOUT;
                        gStoryFallCount++;
                    } else {
                        gStoryStage9FallSkip = 1;
                    }
                }
                gGameState = GAME_STATE_INIT_COMBAT;
            }
        }
    } else if (playMode == PLAY_MODE_VS || playMode == PLAY_MODE_LINK) {
        if (gContinueCount > 0)
            DrawWinCountHud();
        if (gPostKoTimer >= POST_KO_DELAY_FRAMES)
            gGameState = GAME_STATE_INIT_BATTLE_REPLAY;
    }

    nextStage = CheckStageTransition();
    if (nextStage != gCurrentStageId) {
        StopAndWaitCdIdle();
        prevStage = gCurrentStageId;
        gCurrentStageId = nextStage;
        gDisplayPresentEnabled = 0;
        gGameState = GAME_STATE_INIT_COMBAT;
        gPrevStageId = prevStage;
    }
    if (gGameState != GAME_STATE_UPDATE_COMBAT)
        StopCombatRumbleAndAudio();
}
