#ifndef STORY_CUTSCENE_H
#define STORY_CUTSCENE_H

/* Story cutscene variant header in DATA7\V00..V08.DAT (flags == 0 means empty variant). */
#define SCENE_VARIANT_HAS_SECOND_FIGHTER 1

typedef struct SceneVariant {
    int flags;
    int frameCount;
} SceneVariant;

void InitStoryCutscene(int scene, int variant, char *data);
void SelectCutsceneAnchorPoint(void);
/* UpdateStoryCutscene returns this when the scene ends, otherwise the frame's voice cue. */
#define STORY_CUTSCENE_FINISHED  (-1)
#define STORY_CUTSCENE_VOICE_CUE 1
int UpdateStoryCutscene(void);

struct SceneNode;
void ApplyCutsceneFighterPose(int side, struct SceneNode *base);

#endif
