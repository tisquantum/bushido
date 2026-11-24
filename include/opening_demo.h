#ifndef OPENING_DEMO_H
#define OPENING_DEMO_H

#include "scene_node.h"

#define OPENING_DEMO_VARIANT_COUNT 3

typedef enum OpeningDemoVariant {
    OPENING_VARIANT_RED_SHADOW = 0,
    OPENING_VARIANT_KANNUKI,
    OPENING_VARIANT_BLACK_LOTUS
} OpeningDemoVariant;

int InitOpeningDemo(int variant, char *resource);
int UpdateOpeningDemo(void);
void ResetOpeningScriptState(void);
short *StepOpeningDemoScript(short *commands, int frame);
char *SetupOpeningDemoStage(int variant, char *nextSection);
void UpdateOpeningDemoFighterScene(void);
void UpdateOpeningDemoCameraScene(void);
void ApplyOpeningDemoPose(struct SceneNode *parts);
void InitOpeningOverlays(void);
void UpdateAndDrawOpeningOverlays(void);
void SubmitOpeningOverlayPrims(int count);
void InitWhiteFullscreenBackdrop(void);
void SubmitWhiteFullscreenBackdrop(void);
void BuildOpeningTintedCluts(void);
void OpeningDemoScriptNoOp(int operand);

#endif
