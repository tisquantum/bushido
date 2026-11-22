#ifndef MATCH_SETUP_H
#define MATCH_SETUP_H

void AssignFighterPairSlots(void);
void LoadWeaponPairMotions(void);
int PollCdReady(void);
void LoadCombatStage(int stageId);
void UpdateCombatDistanceAndKoTimer(void);
void UpdateStoryHonorWindow(void);
void UpdateStageEvents(void);

extern int gMaxFighterDistSq;
extern short gStage10LowerLevelActive;
extern short gStage4LowerLevelActive;
extern int gFighterDistanceXZ;
extern int gFighterDistanceDelta;
extern int *gStageCameraWalls;  /* a count, then the StageCameraWall records */

#endif
