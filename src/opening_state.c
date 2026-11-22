/* The opening attract state: starts the next of the three opening demo variants with its XA track,
 * then runs it until it ends (on to the demo battle) or a player presses START (on to the main menu). */

#include <sys/types.h>
#include <libgte.h>
#include <libetc.h>

#include "pad.h"
#include "cd.h"
#include "game_state.h"
#include "main.h"
#include "opening_demo.h"
#include "opening_state.h"
#include "load_area.h"
#include "globals.h"

/* Frame of the opening at which the CD stream held for it is let go. */
#define OPENING_STREAM_RELEASE_FRAME 195

short gOpeningVariantIndex = 0;

void InitOpeningState(void)
{
    StopAndWaitCdIdle();
    WaitGpuIdle();
    InitOpeningDemo(gOpeningVariantIndex, DYNAMIC_LOAD_BUFFER);

    gOpeningVariantIndex = gOpeningVariantIndex != OPENING_DEMO_VARIANT_COUNT - 1 ? gOpeningVariantIndex + 1 : 0;

    EnableCdSpuAudio();
    CueXaTrackPaused(XA_TRACK_OPENING);
    gStateFrameTimer = 0;
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_OPENING;
}

void UpdateOpeningState(void)
{
    gDisplayPresentEnabled = 1;
    if (++gStateFrameTimer == OPENING_STREAM_RELEASE_FRAME) {
        ReleaseHeldCdStream();
    }
    if (UpdateOpeningDemo() == 0) {
        gGameState = GAME_STATE_INIT_DEMO_BATTLE;
    }
    if ((gPadState.pressed & BOTH_PADS_START) != 0) {
        gGameState = GAME_STATE_INIT_MAIN_MENU;
    }
}
