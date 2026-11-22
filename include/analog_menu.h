#ifndef ANALOG_MENU_H
#define ANALOG_MENU_H

/* DrawAnalogControllerMenu's flags. */
#define ANALOG_MENU_FLAG_ROW_RUMBLE 0x01
#define ANALOG_MENU_FLAG_ROW_STICKS 0x02
#define ANALOG_MENU_FLAG_RUMBLE_ON  0x04
#define ANALOG_MENU_FLAG_RUMBLE_OFF 0x08
#define ANALOG_MENU_FLAG_P1_ANALOG  0x10
#define ANALOG_MENU_FLAG_P2_ANALOG  0x20

void LoadAnalogControllerAssets(void *loadBuffer);
void DrawAnalogControllerMenu(int flags, int p1LeftX, int p1LeftY, int p1RightX, int p1RightY,
                              int p2LeftX, int p2LeftY, int p2RightX, int p2RightY);

#endif
