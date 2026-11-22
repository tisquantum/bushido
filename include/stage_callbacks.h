#ifndef STAGE_CALLBACKS_H
#define STAGE_CALLBACKS_H

#include <sys/types.h>
#include <libgte.h>

void UpdateAspectCorrectedCameraMatrix(void);
MATRIX *GetAspectCorrectedCameraMatrix(void);
void InsertDeferredPrimsIntoOt(int count);
void InitStageSnowIfEnabled(void);

/* The routines gStageCallbacks names, by stage. */
void InitStageSnowAndDecals(void);
void UpdateStageSnowAndDecals(void);
void InitStage1Waterfall(void);
void UpdateStage1Waterfall(void);
void InitStage3SnowAndDecals(void);
void UpdateStage3PropsAndSnow(void);
void InitStage4SnowDecalsAndGlows(void);
void UpdateStage4SnowAndDecals(void);
void InitStage5SnowAndDecals(void);
void UpdateStage5SnowAndDecals(void);
void InitStage9PropsAndCloth(void);
void UpdateStage9ClothStrips(void);
void InitStage10SnowAndDecals(void);
void UpdateStage10SnowAndVramAnim(void);
void InitStage11Props(void);
void UpdateStage11PropsAndLight(void);
void InitCoveShoreline(void);
void UpdateCoveShoreline(void);
void InitBambooThicketNoOp(void);
void UpdateBambooThicketCuttableProps(void);
void InitYaguraPointProps(void);
void UpdateYaguraPointNightProps(void);
void InitMoatRainAndFoliage(void);
void BeginMoatFrame(void);
void UpdateMoatRainAndFoliage(void);
void InitStage20NoOp(void);
void UpdateStage20NoOp(void);
void InitStage22SnowAndDecals(void);
void UpdateStage22SnowAndVramAnim(void);
void InitGrottoStream(void);
void UpdateGrottoStream(void);

extern CVECTOR gRainColorBottom;

#endif
