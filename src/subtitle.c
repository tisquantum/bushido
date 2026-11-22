/* Subtitles: caption scripts for Story cutscenes, prologues, FMV endings and combat taunts, each a
 * list of "TTTTN" headed lines timed in frames, and the routines that start, advance and draw them. */
#include "font_hud.h"
#include "subtitle.h"

#ifndef VERSION_JP

#define SUBTITLE_CENTER_X_HIRES  320
#define SUBTITLE_CENTER_X_LORES  160
#define SUBTITLE_BASE_Y          200
#define SUBTITLE_LINE_HALF_STEP  8
#define SUBTITLE_LINE_STEP       16
#define SUBTITLE_SPACE_ADVANCE   8
#define SUBTITLE_CHUNK_LEN       20
#define SUBTITLE_TEXT_COLOR      0x808080

/* Relocated script-pointer tables indexed by SubtitleBank (populated by RelocateSubtitleBank). */
int *gSubtitleBanks[SUBTITLE_BANK_COUNT] = { 0, 0, 0 };

/* Each caption begins with a 4-digit start frame and 1-digit line count ("TTTTN"), followed by
 * NUL-terminated lines and an empty-line terminator; the final entry ("TTTT0") marks the end frame. */
char *gSubtitleIntroKannuki[] = { "00451`C'mon!\"\0", "00900" };
char *gSubtitleIntroMikado[] = { "00011`OK! Ready!?\"\0", "01200" };
char *gSubtitleIntroBlackLotus[] = { "00051`Let's have a clean fight!\"\0", "01200" };
char *gSubtitleIntroTatsumi[] = { "00451`This one's for real now.\"\0", "01500" };
char *gSubtitleIntroRedShadow[] = { "00011`...\"\0", "00900" };
char *gSubtitleIntroUtsusemi[] = { "00301`Prepare yourself!\"\0", "00600" };
char *gSubtitleIntroKatze[] = { "00051`I'll put you out of your misery!\"\0", "00601`Die!\"\0", "00900" };
char *gSubtitleIntroBout6Boss[] = {
    "00052`Well, well...who would've ever thought\0 you'd make it to me?\"\0",
    "01200",
};
char *gSubtitleIntroBout7Boss[] = { "00151`You just don't know when to quit, do you?\"\0", "01200" };
char *gSubtitleIntroBout8Boss[] = {
    "00011`Ha ha ha...\"\0",
    "00902`Your sword is stained with the blood\0of your fallen comrades.\"\0",
    "02402`Do you think you're going to kill me\0with it?\"\0",
    "03300",
};
char *gSubtitleIntroFinalBossChallenge[] = { "00011`Wanna try?\0That's fine by me.\"\0", "00750" };
char *gSubtitleIntroFinalBossSilent[] = { "00011`...\"\0", "00900" };
char *gSubtitleYieldKannuki[] = { "00051Tsha! I guess that's that.\0", "01200" };
char *gSubtitleYieldKannukiAlt[] = { "00051Tsha! I guess that's that.\0", "01200" };
char *gSubtitleYieldMikado[] = { "00051Go ahead, do what you will.\0", "01200" };
char *gSubtitleYieldBlackLotus[] = { "00051I've been defeated.\0", "01200" };
char *gSubtitleYieldTatsumi[] = { "00051I'm in pain...hurry...finish me off.\0", "01200" };
char *gSubtitleYieldRedShadow[] = { "00051So it's come to this...kill me...\0", "01200" };
char *gSubtitleYieldUtsusemi[] = { "00051Now...kill me!\0", "01200" };
char *gSubtitleYieldUtsusemiAlt[] = { "00051Now...kill me!\0", "01200" };
char *gSubtitleYieldUtsusemiExtra[] = { "00051Now...kill me!\0", "01200" };

/* Resident combat scripts indexed by subtitleId - 72 (0..11: Story intro taunts, 12..20: Select-button yield lines). */
char **gCombatSubtitleScripts[21] = {
    gSubtitleIntroKannuki, gSubtitleIntroMikado, gSubtitleIntroBlackLotus,
    gSubtitleIntroTatsumi, gSubtitleIntroRedShadow, gSubtitleIntroUtsusemi,
    gSubtitleIntroKatze, gSubtitleIntroBout6Boss, gSubtitleIntroBout7Boss,
    gSubtitleIntroBout8Boss, gSubtitleIntroFinalBossChallenge, gSubtitleIntroFinalBossSilent,
    gSubtitleYieldKannuki, gSubtitleYieldKannukiAlt, gSubtitleYieldMikado,
    gSubtitleYieldBlackLotus, gSubtitleYieldTatsumi, gSubtitleYieldRedShadow,
    gSubtitleYieldUtsusemi, gSubtitleYieldUtsusemiAlt, gSubtitleYieldUtsusemiExtra,
};

static int gSubtitleFrame;
static char **gSubtitleCursor;
static short gSubtitleStartFlag; /* written, never read */

void StartSubtitleById(int subtitleId)
{
    char **script;

    gSubtitleStartFlag = 0;
    if (subtitleId < SUBTITLE_ID_CUTSCENE_BASE) {
        StartSubtitleFromBank(SUBTITLE_BANK_PROLOGUE, subtitleId);
    } else if (subtitleId < SUBTITLE_ID_MOVIE_BASE) {
        StartSubtitleFromBank(SUBTITLE_BANK_CUTSCENE, subtitleId - SUBTITLE_ID_CUTSCENE_BASE);
    } else if (subtitleId < SUBTITLE_ID_COMBAT_BASE) {
        StartSubtitleFromBank(SUBTITLE_BANK_MOVIE, subtitleId - SUBTITLE_ID_MOVIE_BASE);
    } else {
        subtitleId -= SUBTITLE_ID_COMBAT_BASE;
        script = gCombatSubtitleScripts[subtitleId];
        gSubtitleFrame = 0;
        gSubtitleCursor = script;
    }
}

/* Nothing calls it. */
static void StartSubtitleScriptPtr(char **script)
{
    gSubtitleCursor = script;
    gSubtitleFrame = 0;
}

char **StartSubtitleFromBank(int bank, int index)
{
    char **script;

    script = (char **)gSubtitleBanks[bank][index];
    gSubtitleFrame = 0;
    gSubtitleCursor = script;
    return script;
}

static int DrawWrappedSubtitleText(unsigned char *text, int x, int y);

void UpdateAndDrawSubtitle(int useLowResWidth)
{
    unsigned char lineBuffer[56]; /* lines are copied in unchecked: one over 55 characters runs past it */
    unsigned char *src;
    unsigned char *dst;
    unsigned char ch;
    unsigned char glyph;
    int centerX;
    int timestamp;
    int y;
    int width;

    centerX = SUBTITLE_CENTER_X_HIRES;
    if (useLowResWidth != 0) {
        centerX = SUBTITLE_CENTER_X_LORES;
    }

    src = (unsigned char *)*gSubtitleCursor;
    timestamp = (src[0] - '0') * 1000 + (src[1] - '0') * 100 + (src[2] - '0') * 10 + (src[3] - '0');
    src += 4;
    y = *src++ - '0';
    if (y == 0) {
        return;
    }

    if (gSubtitleFrame >= timestamp) {
        y = SUBTITLE_BASE_Y - (y - 1) * SUBTITLE_LINE_HALF_STEP;
        while (*src != '\0') {
            width = 0;
            dst = lineBuffer;
            while ((ch = *src++) != '\0') {
                switch (ch) {
                case '.': glyph = 0; break;
                case '!': glyph = 1; break;
                case '?': glyph = 2; break;
                case '`': glyph = 65; break;
                case '"': glyph = 66; break;
                case '\'': glyph = 67; break;
                case ',': glyph = 68; break;
                case '-': glyph = 69; break;
                default:
                    if ((unsigned char)(ch - '0') < 10) {
                        glyph = ch - ('0' - 3);
                    } else if ((unsigned char)(ch - 'A') < 26) {
                        glyph = ch - ('A' - 13);
                    } else if ((unsigned char)(ch - 'a') < 26) {
                        glyph = ch - ('a' - 39);
                    } else {
                        glyph = FONT_GLYPH_SPACE;
                    }
                    break;
                }
                if (glyph == FONT_GLYPH_SPACE) {
                    width += SUBTITLE_SPACE_ADVANCE;
                    *dst++ = ' ';
                } else {
                    width += gFontGlyphAdvances[glyph];
                    *dst++ = ch;
                }
            }
            *dst = '\0';
            DrawWrappedSubtitleText(lineBuffer, centerX - width / 2, y);
            y += SUBTITLE_LINE_STEP;
        }
    }

    src = (unsigned char *)gSubtitleCursor[1];
    timestamp = (src[0] - '0') * 1000 + (src[1] - '0') * 100 + (src[2] - '0') * 10 + (src[3] - '0');
    if (++gSubtitleFrame == timestamp) {
        gSubtitleCursor++;
    }
}

/* Draws one subtitle line through DrawFormattedText, SUBTITLE_CHUNK_LEN characters at a time. */
static int DrawWrappedSubtitleText(unsigned char *text, int x, int y)
{
    unsigned char chunk[24];
    unsigned char *out;
    int count;

    chunk[0] = '#';
    chunk[1] = 'C';
    DrawFormattedText("#X#Y", x, y);

    count = 0;
    out = chunk + 2;
    while ((*out++ = *text++) != '\0') {
        if (++count == SUBTITLE_CHUNK_LEN) {
            *out = '\0';
            DrawFormattedText(chunk, SUBTITLE_TEXT_COLOR);
            out = chunk + 2;
            count = 0;
        }
    }

    *out = '\0';
    DrawFormattedText(chunk, SUBTITLE_TEXT_COLOR);
    return 1;
}

/* Relocates a subtitle bank in place: [scriptCount, scriptWordOffsets..., captionCount,
 * captionByteOffsets..., captionStrings...] -> absolute pointers, stored in gSubtitleBanks[slot]. */
void RelocateSubtitleBank(int *data, int slot)
{
    int count;
    int base;

    gSubtitleBanks[slot] = data + 1;
    count = *data++;
    base = (int)(data + count + 1);
    while (count--) {
        *data = *data * 4 + base;
        data++;
    }
    count = *data++;
    base = (int)data;
    while (count--) {
        *data += base;
        data++;
    }
}
#endif
