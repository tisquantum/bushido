/* Play mode, story and bout state shared across the game; defined in globals.c. */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <sys/types.h>
#include <libgte.h>

extern short gIsContinueRestart;
extern short gBattleCameraLocked;
extern short gCurrentStageId;
extern short gStoryFallCount;  /* story bouts ended by a fall; written, never read */
extern int gBladeLockTimer;
extern int gCombatFrameCounter;
extern short gStoryHonorWindowTimer;
extern short gPovModeEnabled;
extern short gBladeLockState;
extern short gMatchWinnerSide;
extern short gReachedStage9Flag;
extern short gHumanPlayerCount;
extern short gFighterInjuredRegions[2];
extern short gStoryStage9FallSkip;
extern short gStoryBoutIndex;
extern int gStateFrameTimer;
extern short gSelectedFighter[2];
extern short gStoryEventResult;
extern short gSelectedWeapon[2];
extern short gPracticeWinRecorded;
extern short gPracticeFirstBoutFlag;
extern short gStoryChallengerTimer;
extern short gContinueCount;
extern short gStoryDefeatedMask;
extern short gWeaponClashOccurred;
extern unsigned int gPostKoTimer;
extern short gFighterAltTextureLoaded[2];
extern short gStoryHonorWindowActive;  /* 1 at a Story bout's start, 0 once honor is broken */
extern unsigned short gAutoSaveReturnState;
extern short gStoryChallengerActive;
extern short gVsWinCounts[2];
extern short gStoryProloguePlayed;
extern short gStoryNoDamageFlag;
extern short gPlayMode;
extern short gSlashLevelIndex;
extern short gPrevStageId;
extern short gSlashNeedsEnemyPreload;
extern short gBushidoHonorIntact;
extern VECTOR gBloodSprayDir;

#endif
