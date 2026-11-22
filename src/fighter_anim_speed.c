/* Fighter animation speed: scales Fighter.baseAnimSpeed by torso, right-arm, thigh, and
 * two-handed left-arm wound penalties, clamping fixed-speed actions and active attack windows to ONE. */

#include <sys/types.h>
#include <libgte.h>
#include "fighter.h"
#include "fighter_anim_speed.h"

typedef enum WoundSpeedCategory {
    WOUND_SPEED_TORSO = 0,
    WOUND_SPEED_RIGHT_ARM = 1,
    WOUND_SPEED_THIGH = 2,
    WOUND_SPEED_TWO_HANDED_LEFT_ARM = 3
} WoundSpeedCategory;

void UpdateFighterAnimSpeed(Fighter *fighter)
{
    int action = fighter->action;
    int speed = fighter->baseAnimSpeed;
    int wounds;

    if (action == FIGHTER_ACTION_FATAL_KO ||
        (action == FIGHTER_ACTION_NEUTRAL &&
         (fighter->animInfo->flags & ANIM_FLAG_FIXED_SPEED)) ||
        action == FIGHTER_ACTION_RECOIL || action == FIGHTER_ACTION_HEAVY_RECOIL ||
        action == FIGHTER_ACTION_STAGGER) {
        speed = ONE;
    } else if (action == FIGHTER_ACTION_ATTACK) {
        int frame = fighter->animFrame;
        const AnimInfo *animInfo = fighter->animInfo;

        if (frame >= animInfo->activeStartFrame && frame <= animInfo->activeEndFrame) {
            speed = ONE;
        } else {
            wounds = fighter->torsoWoundCount;
            if (wounds > 4)
                wounds = 4;
            if (wounds > 0)
                speed = (speed * gWoundSpeedScales[WOUND_SPEED_TORSO][wounds - 1]) >> 12;

            wounds = fighter->rightArmWoundCount;
            if (wounds > 4)
                wounds = 4;
            if (wounds > 0)
                speed = (speed * gWoundSpeedScales[WOUND_SPEED_RIGHT_ARM][wounds - 1]) >> 12;

            if (fighter->weaponGripType == FIGHTER_GRIP_TWO_HANDED && fighter->leftArmIntact == 0)
                speed = (speed * gWoundSpeedScales[WOUND_SPEED_TWO_HANDED_LEFT_ARM][0]) >> 12;
        }
    } else {
        wounds = fighter->torsoWoundCount;
        if (wounds > 4)
            wounds = 4;
        if (wounds > 0)
            speed = (speed * gWoundSpeedScales[WOUND_SPEED_TORSO][wounds - 1]) >> 12;

        if (fighter->legsIntact != 0) {
            wounds = fighter->thighWoundCount;
            if (wounds > 4)
                wounds = 4;
            if (wounds > 0)
                speed = (speed * gWoundSpeedScales[WOUND_SPEED_THIGH][wounds - 1]) >> 12;
        }
    }
    if (speed == 0)
        speed = 1;
    fighter->animSpeed = speed;
}
