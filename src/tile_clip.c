/*
 * Per-frame toggle for stage-tile near-plane OTZ clipping: cleared by callers
 * that disable clipping for one frame, then read and re-enabled by SubmitSceneDisplayList.
 */

#include "tile_clip.h"

int gStageTileNearClipEnabled = 1;

void DisableStageTileNearClip(void)
{
    gStageTileNearClipEnabled = 0;
}

void SetStageTileNearClip(int enabled)
{
    if (enabled != 0) {
        gStageTileNearClipEnabled = 1;
    } else {
        gStageTileNearClipEnabled = 0;
    }
}

int IsStageTileNearClipEnabled(void)
{
    return gStageTileNearClipEnabled;
}
