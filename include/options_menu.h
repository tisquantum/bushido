#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

/* Options menu rows (gOptionsCursor, 0..7), indexing gOptionValuePtrs[8]. */
typedef enum OptionsMenuRow {
    OPTIONS_ROW_DIFFICULTY = 0,
    OPTIONS_ROW_WALL_BUMP = 1,
    OPTIONS_ROW_BGM = 2,
    OPTIONS_ROW_MONOCHROME = 3,
    OPTIONS_ROW_CONTINUE = 4,
    OPTIONS_ROW_DAYTIME = 5,
    OPTIONS_ROW_MEMORY_CARD = 6,
    OPTIONS_ROW_AUTO_SAVE = 7,
    OPTIONS_ROW_COUNT = 8
} OptionsMenuRow;

/* Bitmask of the value assigned to each row when pressing Left in the US/EU Options menu
 * (Right assigns (~OPTIONS_LEFT_VALUE_MASK >> row) & 1). */
#define OPTIONS_LEFT_VALUE_MASK 0xb6

/* Options row 6 (gOptionsMemoryCardAction) value for Load vs Save. */
#ifdef VERSION_JP
#define OPTIONS_CARD_ACTION_SAVE 0
#define OPTIONS_CARD_ACTION_LOAD 1
#else
#define OPTIONS_CARD_ACTION_LOAD 0
#define OPTIONS_CARD_ACTION_SAVE 1
#endif

/* Options sub-screen state (gOptionsSubScreen). */
typedef enum OptionsSubScreen {
    OPTIONS_SCREEN_MAIN = 0,
    OPTIONS_SCREEN_CARD_STATUS = 1,
    OPTIONS_SCREEN_CARD_CONFIRM = 2
} OptionsSubScreen;

/* Yes/No cursor on the Options memory-card confirmation dialog (gOptionsConfirmCursor). */
typedef enum OptionsConfirmChoice {
    OPTIONS_CONFIRM_YES = 0,
    OPTIONS_CONFIRM_NO = 1
} OptionsConfirmChoice;

/* One-shot status sound latch during OPTIONS_SCREEN_CARD_STATUS (gOptionsCardSoundState). */
typedef enum OptionsCardSoundState {
    OPTIONS_CARD_SOUND_PENDING = -1,
    OPTIONS_CARD_SOUND_ERROR_PLAYED = 0,
    OPTIONS_CARD_SOUND_OK_PLAYED = 1
} OptionsCardSoundState;

/* Sound indices passed to PlayMenuSound by UpdateOptionsMenu (gMenuSoundIds[7..20]). */
typedef enum OptionsMenuSound {
    OPTIONS_SOUND_CURSOR_UP = 7,
    OPTIONS_SOUND_CURSOR_DOWN = 8,
    OPTIONS_SOUND_TOGGLE = 9,
    OPTIONS_SOUND_EXIT = 10,
    OPTIONS_SOUND_START_SAVE = 11,
    OPTIONS_SOUND_START_LOAD = 12,
    OPTIONS_SOUND_START_FORMAT = 13,
    OPTIONS_SOUND_START_OVERWRITE = 14,
    OPTIONS_SOUND_CARD_SUCCESS = 15,
    OPTIONS_SOUND_CARD_ERROR = 16,
    OPTIONS_SOUND_CARD_PROMPT = 17,
    OPTIONS_SOUND_CONFIRM_YES = 18,
    OPTIONS_SOUND_CONFIRM_NO = 19,
    OPTIONS_SOUND_CONFIRM_DISMISS = 20
} OptionsMenuSound;

/* Options memory-card message sprite indices (gOptionsMemoryCardMsg, gOptionsMsgSprites[]). */
#define OPTIONS_MSG_NONE              (-1)
#define OPTIONS_MSG_SAVING            0
#define OPTIONS_MSG_SAVE_OK           1
#define OPTIONS_MSG_SAVE_ERROR        2
#define OPTIONS_MSG_OVERWRITE_CONFIRM 3
#define OPTIONS_MSG_NO_FREE_BLOCKS    4
#define OPTIONS_MSG_CORRUPT_DATA      5
#define OPTIONS_MSG_FORMAT_CONFIRM    6
#define OPTIONS_MSG_FORMATTING        7
#define OPTIONS_MSG_FORMAT_OK         8
#define OPTIONS_MSG_FORMAT_ERROR      9
#define OPTIONS_MSG_NO_CARD           10
#define OPTIONS_MSG_LOADING           11
#define OPTIONS_MSG_LOAD_OK           12
#define OPTIONS_MSG_LOAD_ERROR        13
#define OPTIONS_MSG_NO_DATA           14
#define OPTIONS_MSG_NONE_BYTE         0xff

#endif
