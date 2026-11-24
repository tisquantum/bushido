/* Simulation substeps: the PC port runs a console frame as several ticks (port/sdk/include/pc_substep.h).
 * On the console a frame is one tick, and every macro below is the original code. */

#ifndef SUBSTEP_H
#define SUBSTEP_H

#ifdef TARGET_PC
#include "pc_substep.h"
#else
#define PC_LAST_SUBTICK 1
#define PC_FIRST_SUBTICK 1
#define PC_SHARE(perFrame) (perFrame)
#define PC_DECAY_EX(original, value, retentionQ12) (original)
#define PC_APPROACH_EX(original, distance, retentionQ12) (original)
#define PC_TICKS_PER_FRAME 1
#define PC_FRAME_INC(x) (++(x))
#define PC_ON_LAST(perFrame) (perFrame)
#endif

#endif
