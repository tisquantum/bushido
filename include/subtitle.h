#ifndef SUBTITLE_H
#define SUBTITLE_H

enum SubtitleBank {
    SUBTITLE_BANK_CUTSCENE = 0,
    SUBTITLE_BANK_PROLOGUE = 1,
    SUBTITLE_BANK_MOVIE = 2,
    SUBTITLE_BANK_COUNT = 3
};

enum SubtitleIdBase {
    SUBTITLE_ID_CUTSCENE_BASE = 6,
    SUBTITLE_ID_MOVIE_BASE = 60,
    SUBTITLE_ID_COMBAT_BASE = 72
};

void StartSubtitleById(int subtitleId);
char **StartSubtitleFromBank(int bank, int index);
void UpdateAndDrawSubtitle(int useLowResWidth);
void RelocateSubtitleBank(int *data, int slot);

#ifndef VERSION_JP
extern short gSubtitleActive;
#endif

#endif
