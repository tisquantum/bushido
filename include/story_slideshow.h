#ifndef STORY_SLIDESHOW_H
#define STORY_SLIDESHOW_H

typedef enum StorySlideshowMode {
    SLIDESHOW_MODE_PROLOGUE = 0,
    SLIDESHOW_MODE_BAD_ENDING = 1
} StorySlideshowMode;

/* Status codes returned by UpdateStorySlideshow() in SLIDESHOW_MODE_PROLOGUE (story_slideshow.c). */
typedef enum StoryPrologueStatus {
    PROLOGUE_STATUS_FINISHED = -1,
    PROLOGUE_STATUS_RUNNING = 0,
    PROLOGUE_STATUS_CUE_INTRO_XA = 1,
    PROLOGUE_STATUS_START_INTRO_XA = 2,
    PROLOGUE_STATUS_STOP_INTRO_XA = 3,
    PROLOGUE_STATUS_START_CHAR_XA = 4
} StoryPrologueStatus;

void InitStorySlideshow(int mode, char *data, int scene);
int UpdateStorySlideshow(void);
void RequestSkipStoryPrologue(void);

#endif
