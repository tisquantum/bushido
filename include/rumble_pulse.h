#ifndef RUMBLE_PULSE_H
#define RUMBLE_PULSE_H

/* Q12 PWM phase accumulator period and maximum pulse duration (VSync ticks). */
#define RUMBLE_PHASE_PERIOD 0x1000
#define RUMBLE_TIMEOUT_VSYNCS 60

/* Per-port DualShock PWM vibration state (gRumblePulses[2]). */
typedef struct RumblePulse {
    short dutyStep;
    short elapsedFrames;
    short phase;
} RumblePulse;

/* Duration (frames) and Q12 PWM duty step for one fighter rumble preset. */
typedef struct RumblePreset {
    unsigned short duration;
    unsigned short intensity;
} RumblePreset;

#define RUMBLE_OPPONENT_PATTERN_COUNT 6

/* DualShock rumble presets passed to TriggerFighterRumble (indexes gPrimaryRumbleTable[12]).
 * Presets 1..5 also vibrate the opponent (gOpponentRumbleTable[6]) unless the opponent holds a gun. */
typedef enum RumblePattern {
    RUMBLE_NONE = 0,
    RUMBLE_FATAL_HIT = 1,
    RUMBLE_WOUND = 2,
    RUMBLE_BLADE_CLASH = 3,
    RUMBLE_GUARD_PARRY = 4,
    RUMBLE_GUARD_BREAK = 5,
    RUMBLE_LANDING = 6,
    RUMBLE_BODY_BUMP = 7,
    RUMBLE_DEBRIS_HIT = 8,
    RUMBLE_GUNSHOT = 9,
    RUMBLE_PROP_CUT = 10,
    RUMBLE_WEAPON_ENV_HIT = 11
} RumblePattern;

extern RumblePulse gRumblePulses[2];
#endif
