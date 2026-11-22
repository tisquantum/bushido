#ifndef MAIN_H
#define MAIN_H

#include <sys/types.h>
#include "stage_tables.h"

/* Horizontal blanks (RCntCNT1) a frame lasts at least: more than one field, so a frame takes two vertical
 * blanks or more (at most 30 frames a second on NTSC, 25 on PAL). */
#define FRAME_PACING_HBLANKS 0x180

extern short gGameState;
extern short gOptionDaytime;
extern short gOptionContinueEnabled;
extern short gKatzeUnlocked;
extern short gOptionMonochrome;
extern short gOptionAutoSave;
extern short gOptionRumbleEnabled;
extern short gDemoFighterPairIndex;
extern short gDemoStageIndex;
extern short gOptionWallBump;
extern int gUnlockedMoviesMask;
extern short gOptionBgmEnabled;
extern short gOptionDifficulty;
extern StageTransition *gStageTransitionsPtr;
extern unsigned char *gSharedWorkBuffer;
extern short gDisplayPresentEnabled;
extern short gAllMenuModesEnabled;
extern int gFrameCounter;
extern u_long *gCurrentOt;

void SetBackgroundClear(int clearBackground, int red, int green, int blue);
void RunBootVersionState(void);
void RumbleVSyncCallback(void);
void WaitGpuIdle(void);

#endif
