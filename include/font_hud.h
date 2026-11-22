#ifndef FONT_HUD_H
#define FONT_HUD_H

#define FONT_GLYPH_SPACE 255 /* the glyph code drawn as a space */

extern short gFontGlyphAdvances[];

void DrawFormattedText(char *format, ...);
void AppendBitmapGlyph(unsigned char glyph);
void DrawReplayOverlayText(void);
void DrawContinueCountdown(int countdown);
void DrawHudBanner(int bannerIndex);
void ShowNowLoadingOverlay(void);
void LoadLinkLobbyScreen(void);
void DrawLinkLobbyScreen(void);
void DrawVersusWinCounts(int p1Wins, int p2Wins);
void DrawSlashModeHud(int level, int kills);
void DrawDemoOverlayText(void);
int ShowPauseOverlay(void);

#endif
