#ifndef MENU_STATE_H
#define MENU_STATE_H

#define MAIN_MENU_ENTRY_COUNT 12
#define MAIN_MENU_ENTRY_VS 1
#define MAIN_MENU_ENTRY_AI_CYCLE 11

/* Sound indices passed to PlayMenuSound by UpdateMainMenuState (gMenuSoundIds[0..3]). */
typedef enum MainMenuSound {
    MAIN_MENU_SOUND_CURSOR_UP = 0,
    MAIN_MENU_SOUND_CURSOR_DOWN = 1,
    MAIN_MENU_SOUND_CONFIRM = 2,
    MAIN_MENU_SOUND_REJECT = 3
} MainMenuSound;

/* A main-menu dispatch entry (gMainMenuEntries), read by UpdateMainMenuState. */
typedef struct MainMenuEntry {
    short playMode;               /* +0x00: PlayMode (0=Story, 1=VS, 2=Practice, 3=Slash, 4=Link) */
    short povMode;                /* +0x02: 0=third-person, 1=first-person POV */
    short nextState;              /* +0x04: GameState transitioned to on confirm */
    unsigned char playerCount;    /* +0x06: 1=P1 only, 2=either pad / 2P */
    unsigned char alwaysUnlocked; /* +0x07: 1=always selectable, 0=requires gAllMenuModesEnabled != 0 */
} MainMenuEntry;

extern unsigned char gMenuSoundIds[36];

void PlayMenuSound(int soundIndex);
void PlayCharacterSelectSound(int soundIndex);
int GetMonochromeOption(void);
int GetDaytimeOption(void);

extern short gMainMenuSelection;

void InitMainMenuState(void);
void InitCharacterSelect(void);
void UpdateCharacterSelect(void);
void GameState11NoOp(void);
void GameState12NoOp(void);
void InitOptionsMenu(void);
void UpdateMainMenuState(void);
void UpdateOptionsMenu(void);
void InitDemoBattle(void);
void UpdateDemoBattle(void);

extern int gDemoFighterP2;
extern int gDemoWeaponP2;

#endif
