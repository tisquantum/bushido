#ifndef SELECT_UI_H
#define SELECT_UI_H

#include "sprite2d.h"

/* UpdateAndDrawSelectScreenUi's mode. */
typedef enum SelectUiMode {
    SELECT_UI_CHARACTER = 0,
    SELECT_UI_WEAPON = 1,
    SELECT_UI_STAGE = 2
} SelectUiMode;

/* The stage choice's progress: SELECT_UI_STAGE's rightArg. */
typedef enum SelectStageSubState {
    SELECT_STAGE_NAVIGATE = 0,
    SELECT_STAGE_CONFIRM_DELAY = 1,
    SELECT_STAGE_DONE = 2
} SelectStageSubState;

void InitSelectScreenUiState(void);
int UpdateAndDrawSelectScreenUi(int mode, int leftArg, int rightArg);
void UpdateAndDrawWeaponConfirmStamp(int side);
int StepStageSelectCursor(int selectMode, int stageIndex, int horizontal, int vertical);

void DrawRotatedTexturedQuad(RotatedTexturedQuad *quad, int rx, int ry, int rz, int x, int y);

#endif
