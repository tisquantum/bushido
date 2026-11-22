/*
 * Controllers: polling both pads (through the Link Cable exchange when it is active), the held,
 * pressed and released masks of gPadState, and the VSync-driven rumble pulses.
 */
#include <sys/types.h>
#include <libetc.h>

#include "game_state.h"
#include "globals.h"
#include "link.h"
#include "main.h"
#include "pad.h"
#include "rumble_pulse.h"

/* Actuator packet: byte 0 enables sending, byte 1 is the motor command, byte 2 the motor bit. */
#define PAD_PORT_COUNT 2
#define PAD_ACTUATOR_PACKET_SIZE 16
#define PAD_ACTUATOR_ENABLE_BYTE 0
#define PAD_ACTUATOR_CMD_BYTE 1
#define PAD_ACTUATOR_MOTOR_BYTE 2
#define PAD_ACTUATOR_CMD_MOTOR 0x40

#ifdef VERSION_JP
#define PAD_STICK_COUNT 2
#define PAD_STICK_CENTER 128
#define PAD_STICK_MIN 0
#define PAD_STICK_MAX 255
#define PAD_STICK_BUTTON_THRESHOLD 96
#define PAD_RECV_ANALOG_OFFSET 4

PadStickCenter gPadStickCenter[PAD_PORT_COUNT][PAD_STICK_COUNT];
PadStickRange gPadStickRange[PAD_PORT_COUNT][PAD_STICK_COUNT];
#endif

typedef union {
    unsigned int word[2];
    struct {
        unsigned char status, id, high, low;
        unsigned char analog[PAD_ANALOG_AXIS_COUNT];
    } data;
} PadPacket;

void InitControllerState(void)
{
    int port;
    int byte;
#ifdef VERSION_JP
    int pad;
    int stick;
#endif

    gPadState.held = 0;
    gPadState.pressed = 0;
    gPadState.released = 0;
    gPadState.notHeld = ~0U;
    gRumblePulses[0].dutyStep = gRumblePulses[1].dutyStep = 0;
    gRumblePulses[0].elapsedFrames = gRumblePulses[1].elapsedFrames = 0;

    for (port = 0; port < PAD_PORT_COUNT; ++port) {
        gRumbleOutputPackets[port][PAD_ACTUATOR_CMD_BYTE] = PAD_ACTUATOR_CMD_MOTOR;
        gRumbleOutputPackets[port][PAD_ACTUATOR_MOTOR_BYTE] = 0;
        for (byte = 3; byte < PAD_ACTUATOR_PACKET_SIZE; ++byte)
            gRumbleOutputPackets[port][byte] = 0;
        gRumbleOutputPackets[port][PAD_ACTUATOR_ENABLE_BYTE] = 1;
    }
#ifdef VERSION_JP
    for (pad = 0; pad < PAD_PORT_COUNT; ++pad) {
        for (stick = 0; stick < PAD_STICK_COUNT; ++stick) {
            gPadStickCenter[pad][stick].x = PAD_STICK_CENTER;
            gPadStickCenter[pad][stick].y = PAD_STICK_CENTER;
            gPadStickRange[pad][stick].minX = PAD_STICK_MIN;
            gPadStickRange[pad][stick].minY = PAD_STICK_MIN;
            gPadStickRange[pad][stick].maxX = PAD_STICK_MAX;
            gPadStickRange[pad][stick].maxY = PAD_STICK_MAX;
        }
    }
#endif
}

#ifdef VERSION_JP
int StickToDpadButtons(unsigned char *stick)
{
    int buttons = 0;
    int x = stick[0] - PAD_STICK_CENTER;
    int y = stick[1] - PAD_STICK_CENTER;

    if (x < -PAD_STICK_BUTTON_THRESHOLD) {
        buttons = PADLleft;
    } else if (x > PAD_STICK_BUTTON_THRESHOLD) {
        buttons = PADLright;
    }
    if (y < -PAD_STICK_BUTTON_THRESHOLD) {
        buttons |= PADLup;
    } else if (y > PAD_STICK_BUTTON_THRESHOLD) {
        buttons |= PADLdown;
    }
    return buttons;
}

int StickToFaceButtons(unsigned char *stick)
{
    int buttons = 0;
    int x = stick[0] - PAD_STICK_CENTER;
    int y = stick[1] - PAD_STICK_CENTER;

    if (x < -PAD_STICK_BUTTON_THRESHOLD) {
        buttons = PADRleft;
    } else if (x > PAD_STICK_BUTTON_THRESHOLD) {
        buttons = PADRright;
    }
    if (y < -PAD_STICK_BUTTON_THRESHOLD) {
        buttons |= PADRup;
    } else if (y > PAD_STICK_BUTTON_THRESHOLD) {
        buttons |= PADRdown;
    }
    return buttons;
}

void CalibratePadSticks(int port, unsigned char *sticks, unsigned char *raw)
{
    int stick;

    for (stick = 0; stick < PAD_STICK_COUNT; ++stick, sticks += 2, raw += 2) {
        int x = raw[0];
        int y = raw[1];

        if (x < gPadStickCenter[port][stick].x) {
            sticks[0] = (x - gPadStickCenter[port][stick].x) * 128
                / (gPadStickCenter[port][stick].x - gPadStickRange[port][stick].minX) + 128;
        } else {
            sticks[0] = (x - gPadStickCenter[port][stick].x) * 127
                / (gPadStickRange[port][stick].maxX - gPadStickCenter[port][stick].x) + 128;
        }
        if (y < gPadStickCenter[port][stick].y) {
            sticks[1] = (y - gPadStickCenter[port][stick].y) * 128
                / (gPadStickCenter[port][stick].y - gPadStickRange[port][stick].minY) + 128;
        } else {
            sticks[1] = (y - gPadStickCenter[port][stick].y) * 127
                / (gPadStickRange[port][stick].maxY - gPadStickCenter[port][stick].y) + 128;
        }
    }
}

void CapturePadStickCentres(int port)
{
    unsigned char *buffer = gPadReceiveBuffers[port];
    int stick;

    if (buffer[0] == 0) {
        int type = buffer[1] >> 4;

        if (type == PAD_TYPE_ANALOG_CONTROLLER || type == PAD_TYPE_ANALOG_JOYSTICK) {
            for (stick = 0; stick < PAD_STICK_COUNT; ++stick) {
                gPadStickCenter[port][stick].x = buffer[PAD_RECV_ANALOG_OFFSET + stick * 2];
                gPadStickCenter[port][stick].y = buffer[PAD_RECV_ANALOG_OFFSET + 1 + stick * 2];
                if (gPadStickRange[port][stick].minX >= gPadStickCenter[port][stick].x)
                    gPadStickCenter[port][stick].x = gPadStickRange[port][stick].minX + 1;
                if (gPadStickRange[port][stick].minY >= gPadStickCenter[port][stick].y)
                    gPadStickCenter[port][stick].y = gPadStickRange[port][stick].minY + 1;
                if (gPadStickRange[port][stick].maxX <= gPadStickCenter[port][stick].x)
                    gPadStickCenter[port][stick].x = gPadStickRange[port][stick].maxX - 1;
                if (gPadStickRange[port][stick].maxY <= gPadStickCenter[port][stick].y)
                    gPadStickCenter[port][stick].y = gPadStickRange[port][stick].maxY - 1;
            }
        }
    }
}

int IsPadAnalog(int port)
{
    unsigned char *buffer = gPadReceiveBuffers[port];
    int analog = 0;

    if (buffer[0] == 0) {
        int type = buffer[1] >> 4;

        if (type == PAD_TYPE_ANALOG_CONTROLLER || type == PAD_TYPE_ANALOG_JOYSTICK) {
            analog = 1;
        }
    }
    return analog;
}
#endif

void PollControllers(void)
{
    PadInputFrame input;
    PadPacket packet[PAD_PORT_COUNT];
    unsigned int combinedButtons = 0;
    int port = 0;
    unsigned int port0Header = ((PadPacket *)gPadReceiveBuffers[0])->word[0];
    unsigned int port0Analog = ((PadPacket *)gPadReceiveBuffers[0])->word[1];
    unsigned int port1Header = ((PadPacket *)gPadReceiveBuffers[1])->word[0];
    unsigned int port1Analog = ((PadPacket *)gPadReceiveBuffers[1])->word[1];
    unsigned int portButtons, current, prev;
    PadPacket *portPacket;
    int axis;

    packet[0].word[0] = port0Header;
    packet[0].word[1] = port0Analog;
    packet[1].word[0] = port1Header;
    packet[1].word[1] = port1Analog;
    do {
        portPacket = &packet[port];
        portButtons = 0;
        if (portPacket->data.status == 0) {
#ifdef VERSION_JP
            int stickButtons;

            input.type[port] = portPacket->data.id >> 4;
            stickButtons = 0;
#else
            unsigned int id = portPacket->data.id;
            int analogMode = 0;

            input.type[port] = id >> 4;
            input.connected[port] = 1;
#endif
            switch (input.type[port]) {
            case PAD_TYPE_ANALOG_JOYSTICK:
            case PAD_TYPE_ANALOG_CONTROLLER:
#ifdef VERSION_JP
                CalibratePadSticks(port, input.analog[port], portPacket->data.analog);
                if (gGameState == GAME_STATE_UPDATE_COMBAT) {
                    stickButtons |= StickToDpadButtons(&input.analog[port][PAD_ANALOG_LEFT_X]);
                    stickButtons |= StickToFaceButtons(&input.analog[port][PAD_ANALOG_RIGHT_X]);
                }
#else
                /* both arms are the same in the original */
                if (analogMode)
                    input.type[port] = PAD_TYPE_DIGITAL;
                else
                    input.type[port] = PAD_TYPE_DIGITAL;
#endif
            case PAD_TYPE_NEGCON:
            case PAD_TYPE_KONAMI_GUN:
            case PAD_TYPE_DIGITAL:
#ifdef VERSION_JP
                portButtons = ~(portPacket->data.low | (portPacket->data.high << 8)) | stickButtons;
#else
                portButtons = ~((portPacket->data.high << 8) | portPacket->data.low);
#endif
                break;
            case PAD_TYPE_MOUSE:
            case PAD_TYPE_NAMCO_GUN:
            case PAD_TYPE_MULTITAP:
            default:
                portButtons = 0;
                break;
            }
        } else {
            input.type[port] = PAD_TYPE_DIGITAL;
#ifndef VERSION_JP
            input.connected[port] = 0;
#endif
        }
        combinedButtons = (combinedButtons >> 16) | (portButtons << 16);
    } while (++port < PAD_PORT_COUNT);

    input.held = combinedButtons;
    ExchangeLinkInputPacket(&input);

    port = 0;
    do {
        gPadState.type[port] = input.type[port];
#ifndef VERSION_JP
        gPadState.connected[port] = input.connected[port];
#endif
        axis = 0;
        do {
            gPadState.analog[port][axis] = input.analog[port][axis];
        } while (++axis < PAD_ANALOG_AXIS_COUNT);
    } while (++port < PAD_PORT_COUNT);

    current = input.held;
    prev = gPadState.held;
    gPadState.held = current;
    gPadState.pressed = current & ~prev;
    gPadState.notHeld = ~current;
    gPadState.released = ~current & prev;
}

void ClearControllerInput(void)
{
    gPadState.type[0] = PAD_TYPE_DIGITAL;
    gPadState.type[1] = PAD_TYPE_DIGITAL;
    gPadState.held = 0;
    gPadState.pressed = 0;
    gPadState.notHeld = ~0U;
    gPadState.released = 0;
}

void UpdateControllerRumble(void)
{
    int port;
    int pulse;
    RumblePulse *rumble = gRumblePulses;

    for (port = 0; port < PAD_PORT_COUNT; port++, rumble++) {
        if (rumble->dutyStep > 0) {
            rumble->phase += rumble->dutyStep;
            pulse = rumble->phase >= RUMBLE_PHASE_PERIOD;
            if (pulse)
                rumble->phase -= RUMBLE_PHASE_PERIOD;
        } else {
            pulse = 0;
        }

        if (gGameState == GAME_STATE_UPDATE_COMBAT && gPlayMode == PLAY_MODE_LINK) {
            if (gLinkRole == LINK_ROLE_HOST) {
                gRumbleOutputPackets[port][PAD_ACTUATOR_MOTOR_BYTE] = gOptionRumbleEnabled && pulse && port == 0;
            } else {
                gRumbleOutputPackets[!port][PAD_ACTUATOR_MOTOR_BYTE] = gOptionRumbleEnabled && pulse && port == 1;
            }
        } else {
            gRumbleOutputPackets[port][PAD_ACTUATOR_MOTOR_BYTE] = gOptionRumbleEnabled && pulse;
        }

        if (rumble->dutyStep) {
            if (++rumble->elapsedFrames > RUMBLE_TIMEOUT_VSYNCS)
                rumble->dutyStep = 0;
        } else {
            rumble->elapsedFrames = 0;
            rumble->phase = RUMBLE_PHASE_PERIOD;
        }
    }
}
