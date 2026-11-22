#ifndef STORY_STATE_H
#define STORY_STATE_H

#include "world_vector.h"

/* Bouts 0 to 8 are the regular run; bout 9 is fought only on the way to the true ending. gStoryBoutIndex
 * advances on each win, so 9 also means the run is won and 10 that the true-ending bout is won. */
#define STORY_BOUT_COUNT         10
#define STORY_REGULAR_BOUT_COUNT 9
#define STORY_TRUE_ENDING_BOUT   9
#define STORY_TRUE_ENDING_WON    10

void InitVictorySceneState(void);
void UpdateVictorySceneState(void);
void InitStoryCutsceneState(void);
int IsTrueEndingEligible(void);
void UpdateStoryCutsceneState(void);
void InitContinueScreenState(void);
void UpdateContinueScreenState(void);
void GetApproachCutscenePositions(int side, WorldVector *first, WorldVector *second);
void PrepareApproachCutscene(void);
void InitApproachCutsceneState(void);
void UpdateApproachCutsceneState(void);
void GetSlashClearCutscenePositions(int side, WorldVector *first, WorldVector *second);
void InitSlashClearCutsceneState(void);
void UpdateSlashClearCutsceneState(void);
void GameState15NoOp(void);
void GameState16NoOp(void);
void PlayStoryEndingMovieState(void);
void WaitCancelToCreditsState(void);
void InitBadEndingSlideState(void);
void UpdateBadEndingSlideState(void);
void InitStoryPrologueState(void);
void UpdateStoryPrologueState(void);
void InitCreditsState(void);
void UpdateCreditsState(void);
void HandleLinkAbortState(void);
void WaitCrossToMenuState(void);
int TryUnlockKatze(void);
void InitWaitCancelToMenuState(void);
void UpdateWaitCancelToMenuState(void);
void InitMovieTheaterState(void);
void PlayFmvMovie(int movieId);
void UpdateMovieTheaterState(void);
void InitAnalogControllerMenuState(void);
void UpdateAnalogControllerMenuState(void);

extern short gPendingMovieId;
extern WorldVector gStoryBoutStartPos;

#endif
