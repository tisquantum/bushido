#ifndef MOTION_FILE_H
#define MOTION_FILE_H

/* The weapon-pair motion file, loaded at the start of the dynamic load buffer for each
 * bout. Its header words point into the file; each table indexed by a fighter's weaponPairSlot holds
 * one entry per fighter of the pair. */

#include "fighter.h"
#include "load_area.h"

typedef struct MotionFileHeader {
    MotionPlacement *startPlacements[2];          /* +0x00: the motion a bout starts in */
    AnimInfo *animInfo;                           /* +0x08: one record per animation clip, clip 1 first */
    int *animBaseFrames;                          /* +0x0C: each clip's first pose frame */
    unsigned long *poseStream;                    /* +0x10: the pose-stream header */
    MotionPlacement *storyIntroPlacements[FIGHTER_CLASS_COUNT][2]; /* +0x14: per-characterClass intro placements
                                                                      [charClass][weaponPairSlot] */
    MotionPlacement *victoryPlacements[2];        /* +0x74: the winner's victory motion */
    MotionPlacement *legsLostVictoryPlacements[2]; /* +0x7C: the same, legs crippled */
    struct AiAttackChoice **aiAttackTables[2];     /* +0x84: per-weaponPairSlot pointers
                                                      to 3-stance AiAttackChoice lists */
} MotionFileHeader;

#define MOTION_FILE ((MotionFileHeader *)DYNAMIC_LOAD_BUFFER)

#endif
