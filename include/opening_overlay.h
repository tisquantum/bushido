#ifndef OPENING_OVERLAY_H
#define OPENING_OVERLAY_H

#include <sys/types.h>
#include <libgte.h>
#include "packet_build.h"

/* Entries 0..29 of the opening OverlayTex table are 6 banners x 5 frames;
 * entries 30..46 are the 17 slash-cut / wide opening overlays. */
#define OPENING_OVERLAY_TEX_BASE 30
#define SLASH_OVERLAY_SLOT_COUNT 4

/* Lifecycle states for SlashOverlaySlot.state. */
typedef enum SlashOverlayState {
    SLASH_OVERLAY_STATE_IDLE = 0,
    SLASH_OVERLAY_STATE_INTRO = 1,
    SLASH_OVERLAY_STATE_HOLD = 2,
    SLASH_OVERLAY_STATE_FADEOUT = 3
} SlashOverlayState;

/* Rendering mode for SlashOverlayPayload.kind and OverlayDesc.kind (0..7).
 * Modes 0..4 emit slash beams and split polygons; modes 5..7 emit wide
 * overlay sprites (5 flashes a white tile; 6/7 draw a fullscreen backdrop). */
typedef enum SlashOverlayKind {
    SLASH_OVERLAY_HORIZONTAL = 0,
    SLASH_OVERLAY_DIAG_HORIZONTAL = 1,
    SLASH_OVERLAY_DIAG_TRI = 2,
    SLASH_OVERLAY_HORIZONTAL_ALT = 3,
    SLASH_OVERLAY_HORIZONTAL_FAST = 4,
    SLASH_OVERLAY_WIDE_FLASH = 5,
    SLASH_OVERLAY_WIDE_INSTANT = 6,
    SLASH_OVERLAY_WIDE_FADE = 7
} SlashOverlayKind;

typedef struct {
    short dy;
    short frames;
    short x;
    short y;
    short dir;
} TitleEntry;

typedef struct OverlayTex {
    short tpage;
    unsigned short clut;
    unsigned short x0;
    unsigned short y0;
    unsigned short x1;
    unsigned short y1;
} OverlayTex;

typedef struct SlashOverlayPayload {
    short kind;
    short delay;
    short x;
    short y;
    short width;
    short height;
    CVECTOR color;
    unsigned char u;
    unsigned char v;
    unsigned short tpage;
    unsigned short clut;
} SlashOverlayPayload;

typedef struct SlashOverlaySlot {
    SlashOverlayPayload payload;
    short state;
    short timer;
} SlashOverlaySlot;

void InitOpeningOverlaySystem(OverlayTex *texTable);
void TriggerOpeningOverlay(int overlayIndex);
int UpdateOpeningBannerPackets(char **cursor);
void ClearSlashOverlaySlots(void);
int AllocSlashOverlaySlot(const SlashOverlayPayload *source);
int UpdateSlashOverlays(PacketBuilder *build);

#endif
