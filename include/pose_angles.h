#ifndef POSE_ANGLES_H
#define POSE_ANGLES_H

/* SVECTOR without its pad. Euler angles, 4096 to the turn, of a decoded pose's joints (joint 0 aside: see
 * PoseJointIndex), the rest pose (gSkeletonRestAngles) and the root's rotation. */
typedef struct PoseAngles {
    short x, y, z;
} PoseAngles;

#ifdef TARGET_PC
/* A recorded pose a tick before its frame's last (pc_substep.h): `pose`, this frame's, blended from `last`,
 * the frame before's. Entry 0 is the root (height and distance blended straight, heading as an angle), the
 * rest joint angles. */
void PcBlendPose(PoseAngles *pose, const PoseAngles *last, int count);
#endif

/* One decoded 18-joint pose (DecodePoseFramePair's output; Fighter.savedPose). */
typedef struct FighterPose {
    PoseAngles joint[18];
} FighterPose;

/* FighterPose.joint in the pose stream's own order: 0 the root's offset (x its height, y its heading, z its
 * distance), 1 pelvis, 2-4 right leg, 5-7 left leg, 8 torso, 9 head, 10-13 left arm, 14-17 right arm.
 * FighterJointIndex j (1..17) takes pose joint gAnimToSkeletonJointMap[j - 1]. */
enum PoseJointIndex {
    POSE_JOINT_ROOT = 0,
    POSE_JOINT_PELVIS = 1,
    POSE_JOINT_TORSO = 8,
    POSE_JOINT_HEAD = 9,
    POSE_JOINT_LEFT_ARM = 10
};

#endif
