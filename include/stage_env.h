#ifndef STAGE_ENV_H
#define STAGE_ENV_H

void CullStageAndQueueFog(int fogStartDepth);

void InitStageRenderEnvironment(int stageId);
void LoadStageAndLighting(int stageId);

extern int gStageRenderFlags;

#endif
