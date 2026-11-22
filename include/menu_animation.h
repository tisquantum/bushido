#ifndef MENU_ANIMATION_H
#define MENU_ANIMATION_H

/* Capacity of gMenuAnimClips and gSelectAnimations. */
#define MENU_ANIM_CLIP_COUNT 42
#define MENU_ANIM_SLOT_COUNT 3

/* MenuAnimState.duration (+0x14) sentinel: hold without auto-advancing until a script transition. */
#define MENU_ANIM_DURATION_HOLD (-1)

/* Bit 15 of a halfword script entry distinguishes an opcode ((cmd >> 8) & 0x7F, arg = cmd & 0xFF)
 * from a direct gMenuAnimClips index (0..MENU_ANIM_CLIP_COUNT - 1). */
#define MENU_ANIM_OP_MASK 0x8000
#define MENU_ANIM_CMD(op, arg) ((unsigned short)(MENU_ANIM_OP_MASK | ((op) << 8) | (arg)))

/* Showcase controller indices in gSelectAnimations[3]. */
typedef enum MenuAnimSlot {
    MENU_ANIM_SLOT_P1 = 0,   /* P1 showcase fighter at (900, 0, 900); gSelectP1Script */
    MENU_ANIM_SLOT_P2 = 1,   /* P2 showcase fighter at (-900, 0, 900); gSelectP2Script */
    MENU_ANIM_SLOT_HOST = 2  /* Center showcase figure at (0, 0, 0); gSelectHostScript */
} MenuAnimSlot;

/* Player-slot (P1/P2) MenuAnimState.flags (+0x08) bit masks tested by opcodes 0 and 4. */
#define MENU_ANIM_FLAG_CHAR_CONFIRMED   0x01 /* bit 0: character chosen */
#define MENU_ANIM_FLAG_WEAPON_CONFIRMED 0x02 /* bit 1: weapon chosen */
#define MENU_ANIM_FLAG_HOST_DONE        0x04 /* bit 2: host reached halt step (0x0E or 0x15) */
#define MENU_ANIM_FLAG_P2_WEAPON_CUE    0x08 /* bit 3: P2 weapon confirmed and host at step 0x12 */

/* Host-slot (MENU_ANIM_SLOT_HOST) MenuAnimState.flags (+0x08) bit masks. */
#define MENU_ANIM_HOST_CHARS_CONFIRMED  0x01 /* bit 0: active player(s) reached step 4 */
#define MENU_ANIM_HOST_P1_WEAPON_DONE   0x02 /* bit 1: P1 reached step 0x0A */
#define MENU_ANIM_HOST_P2_WEAPON_DONE   0x04 /* bit 2: P1 at step 0x0A and P2 at step 7 (2P) */

/* Pose-script bytecode opcodes ((cmd >> 8) & 0x7F when cmd & MENU_ANIM_OP_MASK) executed by UpdateMenuAnimScript. */
typedef enum MenuAnimOpcode {
    MENU_ANIM_OP_WAIT_FLAG_HOLD = 0, /* advance if (flags >> arg) & 1; else hold current frame */
    MENU_ANIM_OP_HALT = 1,           /* stop advancing and hold current frame indefinitely */
    MENU_ANIM_OP_PLAY_CHAR_CLIP = 2, /* play gMenuAnimClips[arg + (charId % 6) * 4];
                                        skip with duration=-1 if charId >= 12 */
    MENU_ANIM_OP_BRANCH_MODE = 3,    /* relative signed 4-bit jump +(nibble + 1): low nibble in 2P, high in 1P */
    MENU_ANIM_OP_WAIT_FLAG_LOOP = 4, /* advance if (flags >> arg) & 1; else loop current clip (timer = 0) */
    MENU_ANIM_OP_NOP = 5,            /* advance script/step without stopping */
    MENU_ANIM_OP_BRANCH_COND = 6,    /* jump +(arg & 0xF) + 1 if cond ((arg >> 4) & 0xF):
                                        0=!(P1.flags&2), 1=(P2==Katze) */
    MENU_ANIM_OP_JUMP = 7            /* unconditional relative signed 8-bit jump +(signed char)arg + 1 */
} MenuAnimOpcode;

/* 4-byte clip entry in gMenuAnimClips[42]: first frame in the Character Select
 * pose stream (resource 0x54) and clip length in frames. */
typedef struct MenuAnimEntry {
    short startFrame; /* +0x00: base frame index in pose stream */
    short duration;   /* +0x02: clip length in frames */
} MenuAnimEntry;

/* 28-byte (0x1C) Character Select showcase animation controller in gSelectAnimations[3].
 * Updated by UpdateMenuAnimScript; ApplyMenuShowcasePose decodes pose frame (startFrame + timer). */
typedef struct MenuAnimState {
    unsigned short *script; /* +0x00: pointer to current halfword pose-script instruction */
    int step;               /* +0x04: halfword offset of next instruction from script start */
    int flags;              /* +0x08: confirmation/sync bitfield (MENU_ANIM_FLAG_* / MENU_ANIM_HOST_*) */
    int clipIndex;          /* +0x0C: active gMenuAnimClips index (0..41) */
    int startFrame;         /* +0x10: base frame index in pose stream */
    int duration;           /* +0x14: clip length in frames (MENU_ANIM_DURATION_HOLD = hold, 0 = step immediately) */
    int timer;              /* +0x18: elapsed frames within active clip */
} MenuAnimState;

/* One frame of the select screen's camera track (gSelectCameraTrack): position and rotation. */
typedef struct SelectCameraKey {
    short x, y, z;
    short rotX, rotY;
    unsigned short rotZ;
} SelectCameraKey;

extern MenuAnimEntry gMenuAnimClips[];

#endif
