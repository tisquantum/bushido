/* Definitions of the program's uninitialized globals that no other unit defines. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "billboard.h"
#include "camera.h"
#include "cd.h"
#include "combat_state.h"
#include "decal.h"
#include "display_buffer.h"
#include "fighter.h"
#include "fighter_hit_sphere.h"
#include "world_vector.h"
#include "globals.h"
#include "light.h"
#include "link.h"
#include "match_setup.h"
#include "menu_state.h"
#include "model_table.h"
#include "packet_build.h"
#include "pad.h"
#include "scene_node.h"
#include "render.h"
#include "replay.h"
#include "rumble_pulse.h"
#include "scene.h"
#include "stage_callbacks.h"
#include "stage_cull.h"
#include "stage_env.h"
#include "stage_tables.h"
#include "story_state.h"
#include "tim.h"

short gIsContinueRestart;
int *gStageCameraWalls;
SVECTOR gAuxViewAngles;
CVECTOR gStageLitColor;
short gBattleCameraLocked;
short gCurrentStageId;
unsigned char gPolyF3Code;
EffectTextureEntry gGroundDecalTexture;
unsigned char gPolyG3Code;
unsigned char gPolyF4Code;
unsigned char gPolyG4Code;
short gStoryFallCount;
unsigned char gLineF2Code;
short gCombatPaused;
int gRendererMonochrome;
int gBladeLockTimer;
int gCombatFrameCounter;
StageTransition *gPendingTransition;
LightSlot *gActiveLightSlot;
short gStoryHonorWindowTimer;
struct SceneNode *gSecondaryCameraNodePtr;
unsigned char gPolyFT3Code;
unsigned char gPolyGT3Code;
unsigned char gPolyFT4Code;
unsigned char gPolyGT4Code;
short gPovModeEnabled;
short gBladeLockState;
short gSceneUnreadFlag;
short gMatchWinnerSide;
short gReachedStage9Flag;
short gHumanPlayerCount;
short gFighterInjuredRegions[2];
MATRIX *gCameraMatrix;
CVECTOR gRainColorBottom;
short gStoryStage9FallSkip;
int gStageRenderFlags;
int gReplayMarkCursor;
short gStoryBoutIndex;
int gMaxFighterDistSq;
short gSlashBoutIndex;
int gStateFrameTimer;
short gSelectedFighter[2];
short gStoryEventResult;
short gSelectedWeapon[2];
short gPracticeWinRecorded;
short gPracticeFirstBoutFlag;
short gStoryChallengerTimer;
short gContinueCount;
short gStoryDefeatedMask;
short gWeaponClashOccurred;
struct SceneNode **gRenderQueueCursor;
unsigned int gPostKoTimer;
short gFighterAltTextureLoaded[2];
int gDemoFighterP2;
int gDemoWeaponP2;
int gFighterDistanceXZ;
short gStoryHonorWindowActive;
short gLinkRole;
unsigned short gAutoSaveReturnState;
short gPendingMovieId;
short gStoryChallengerActive;
short gStage10LowerLevelActive;
short gStage4LowerLevelActive;
short gVsWinCounts[2];
short gStoryProloguePlayed;
unsigned short gSceneFrameCounter;
short gStoryNoDamageFlag;
short gPlayMode;
void *gPacketCursor;
short gSlashLevelIndex;
short gPrevStageId;
int gFighterDistanceDelta;
short gSlashNeedsEnemyPreload;
int gReplayPlaybackFrame;
short gBushidoHonorIntact;
CVECTOR gSkinnedPrelitColors[SKINNED_PRELIT_COLOR_MAX];
FighterHitSphere gFighterHitSpheres[2][FIGHTER_HIT_SPHERE_COUNT];
LightSlot gLightSlots[LIGHT_SLOT_COUNT];
struct CameraState gCameraState;
DisplayBuffer db[DISPLAY_BUFFER_COUNT];
unsigned char gPadReceiveBuffers[2][36];
struct CameraState gCameraOffset;
MATRIX gCombinedViewMatrix;
VECTOR gBloodSprayDir;
struct SceneNode gCameraNode;
CdDrive gCdDrive;
WorldVector gStoryBoutStartPos;
Fighter gFighters[2];
PadInputFrame gPadState;
unsigned short **gModelTables[MODEL_TABLE_COUNT];
unsigned short gModelTableCounts[MODEL_TABLE_COUNT];
unsigned char gRumbleOutputPackets[2][16];
RumblePulse gRumblePulses[2];
