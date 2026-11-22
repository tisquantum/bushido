#ifndef PAD_H
#define PAD_H

#include <sys/types.h>
#include <libetc.h>

/* Menu confirm and cancel pad masks: the Japanese build confirms with circle (PADRright) and
 * cancels with cross (PADRdown), the US and European builds the reverse. */
#ifdef VERSION_JP
#define MENU_CONFIRM PADRright
#define MENU_CANCEL PADRdown
#else
#define MENU_CONFIRM PADRdown
#define MENU_CANCEL PADRright
#endif

/* PadInputFrame.type: the high nibble of the controller's id. */
typedef enum PadType {
    PAD_TYPE_MOUSE = 1,
    PAD_TYPE_NEGCON = 2,
    PAD_TYPE_KONAMI_GUN = 3,
    PAD_TYPE_DIGITAL = 4,
    PAD_TYPE_ANALOG_JOYSTICK = 5,
    PAD_TYPE_NAMCO_GUN = 6,
    PAD_TYPE_ANALOG_CONTROLLER = 7,
    PAD_TYPE_MULTITAP = 8
} PadType;

#define PAD_ANALOG_RIGHT_X 0
#define PAD_ANALOG_RIGHT_Y 1
#define PAD_ANALOG_LEFT_X  2
#define PAD_ANALOG_LEFT_Y  3
#define PAD_ANALOG_AXIS_COUNT 4

#define PAD_DPAD_MASK (PADLup | PADLright | PADLdown | PADLleft)
/* A button pressed on either controller, as gPadState holds both. */
#define BOTH_PADS(button) (_PAD(0, button) | _PAD(1, button))
#define BOTH_PADS_START   BOTH_PADS(PADstart)
#define BOTH_PADS_CONFIRM BOTH_PADS(MENU_CONFIRM)
#define BOTH_PADS_CANCEL  BOTH_PADS(MENU_CANCEL)

/* A controller frame: the pad state of both ports (gPadState), or the input one fighter is driven
 * by. Buttons are libetc.h's PAD bits, port 1 in the low half and port 2 in the high half of
 * gPadState; notHeld is ~held. */
typedef struct PadInputFrame {
    short type[2];
#ifndef VERSION_JP
    short connected[2];
#endif
    unsigned int held;
    unsigned int pressed;
    unsigned int released;
    unsigned int notHeld;
    unsigned char analog[2][4];
} PadInputFrame;

#ifdef VERSION_JP
/* Calibrated neutral center (x, y) for one analog stick (gPadStickCenter[2][2]). */
typedef struct PadStickCenter {
    short x;
    short y;
} PadStickCenter;

/* Calibrated travel bounds for one analog stick (gPadStickRange[2][2]). */
typedef struct PadStickRange {
    short minX;
    short minY;
    short maxX;
    short maxY;
} PadStickRange;

extern PadStickCenter gPadStickCenter[2][2];
extern PadStickRange gPadStickRange[2][2];
#endif

extern PadInputFrame gPadState;

void InitControllerState(void);
void ClearControllerInput(void);
void UpdateControllerRumble(void);
void PollControllers(void);

#ifdef VERSION_JP
int StickToDpadButtons(unsigned char *stick);
int StickToFaceButtons(unsigned char *stick);
void CalibratePadSticks(int port, unsigned char *sticks, unsigned char *raw);
void CapturePadStickCentres(int port);
int IsPadAnalog(int port);
#endif

extern unsigned char gPadReceiveBuffers[2][36];
extern unsigned char gRumbleOutputPackets[2][16];

#endif
