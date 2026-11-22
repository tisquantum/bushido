#ifndef CAMERA_H
#define CAMERA_H

#include <sys/types.h>
#include <libgte.h>
#include "fighter.h"

/* Inactive cell coordinate sentinel in gOcclusionMarkers[2]. */
#define OCCLUSION_MARKER_NONE 100

/* Mode parameter for UpdateBattleCamera. */
typedef enum BattleCameraMode {
    BATTLE_CAMERA_LIVE = 0,
    BATTLE_CAMERA_REPLAY = 1
} BattleCameraMode;

/* 0x1C-byte high-level orbit camera state.
 * Passed to BeginSceneFrame, which places the eye at target + R(-angles) * (0, 0, -distance). */
struct CameraState {
    VECTOR target;  /* +0x00: world-space orbit focus point (eye position when distance == 0) */
    SVECTOR angles; /* +0x10: Euler angles, 4096 to the turn (vx pitch, vy yaw, vz roll) */
    int distance;   /* +0x18: orbit distance behind target (0 for POV / stationary cameras) */
};

extern struct CameraState gCameraState;

extern struct CameraState gCameraOffset;

/* One stage camera wall (gStageCameraWalls entries): an XZ segment the battle camera may not see through. */
typedef struct StageCameraWall {
    int x0;
    int z0;
    int x1;
    int z1;
    int topY;
} StageCameraWall;

typedef struct BattleCameraScratch {
    SVECTOR targetAngles;
    VECTOR focusTarget;
    VECTOR cameraPos;
    VECTOR hitPos;
    VECTOR fighterPos[2];
    StageCameraWall wall;
} BattleCameraScratch;

void UpdateBattleCamera(int mode, VECTOR *primaryTarget, VECTOR *secondaryTarget,
                        int frameIndex, int isRunning, int bladeLockState);
void ApplyCameraOcclusionMarkers(void);
void SetCameraTransitionFromFighter(Fighter *fighter, int progress);
void UpdateContinueOrbitCamera(Fighter *fighter, int orbitProgress, int baseYaw);
void AimSceneCameraAtFighter(Fighter *fighter, int progress);
void SetSlashClearCamera(Fighter *fighter, int progress);
void UpdatePovCamera(Fighter *fighter);

#define DEFAULT_SCENE_FOV 45
int CalcGeomScreenFromFov(int degrees);

#endif
