#ifndef REPLAY_H
#define REPLAY_H

#include <sys/types.h>
#include <libgte.h>

#define REPLAY_DURATION_FRAMES 195 /* a replay shows the bout's last 195 frames */

void ResetReplayBuffer(void);
void RecordReplayCombatEvent(int side, int event, VECTOR *position);
void InitBattleReplay(void);
void UpdateBattleReplay(void);
void RecordFighterReplayFrame(int side, int blendTargetFrame);

extern int gReplayMarkCursor;
extern int gReplayPlaybackFrame;

#endif
