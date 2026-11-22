/* Fighter turning: a turn toward a target heading taken as the shortest signed angle, and the start
 * of a blend from the current pose. */

#include <sys/types.h>
#include <libgte.h>
#include "fighter.h"
#include "fighter_turn.h"

void AdjustFighterTurnTarget(Fighter *fighter, int targetHeading)
{
    int delta;

    delta = (targetHeading - (fighter->angles.heading + fighter->turnRemaining)) & ANGLE_MASK;
    if (delta >= ANGLE_HALF) {
        delta -= ANGLE_FULL;
    }
    fighter->turnRemaining += delta;
}

void SetFighterTurnTarget(Fighter *fighter, int targetHeading)
{
    int delta;

    delta = (targetHeading - fighter->angles.heading) & ANGLE_MASK;
    if (delta >= ANGLE_HALF) {
        delta -= ANGLE_FULL;
    }
    fighter->turnRemaining = delta;
}

void StartFighterPoseBlend(Fighter *fighter, int blendStep)
{
    fighter->poseBlendWeight = ONE - blendStep;
    fighter->poseBlendStep = blendStep;
    fighter->blendSavedRoot = fighter->prevRootOffset;
}
