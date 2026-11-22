#ifndef COMBAT_STATE_H
#define COMBAT_STATE_H

#include <sys/types.h>
#include <libgte.h>
#include "pad.h"

/* Per-stage cloth wind target passed to SetClothWindTarget.
 * magnitude: wind strength (0..8192; 1024 default, scaled by Q12 rsin/rcos).
 * angle: PlayStation GTE angle (4096 = 360 deg, 1024 = 90 deg). */
typedef struct StageWind {
    short magnitude;
    short angle;
} StageWind;

extern unsigned char gBossDefaultWeapons[12];

void LoadCutsceneFighterSoundBanks(void);
void LoadApproachSoundBanks(void);
void LoadDialogueSoundBank(void);
void LoadMenuSoundBanks(void);
void StartStageAmbientSounds(void);
void UpdateAmbientSounds(void);
void IncrementVsWinCounts(void);
int CalcPovAiAnimSpeedScale(void);
int CalcPovAiClashPowerScale(void);
int CalcSlashAiAnimSpeed(void);
int CalcSlashAiClashPower(void);
void InitCombatState(void);
void StopCombatRumbleAndAudio(void);
void UpdateSceneCameraAndWind(void);
void UpdateCombatState(void);
void LoadCombatSoundBanks(void);
void TriggerPositionalAmbientSound(int slotIndex, VECTOR *position);
void BuildFighterInput(int player, PadInputFrame *out);

extern short gCombatPaused;
extern MATRIX *gCameraMatrix;
extern short gSlashBoutIndex;

#endif
