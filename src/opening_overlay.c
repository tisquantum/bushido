/* The opening sequence's overlays: the scrolling title banners and the slash-cut card overlays the
 * opening script triggers, each built as GPU packets every frame. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "packet_build.h"
#include "gpu_code.h"
#include "menu_state.h"
#include "opening_overlay.h"
#include "scene_math.h"
#include "camera.h"
#include "rot_matrix.h"

#define OPENING_SCREEN_WIDTH  640
#define OPENING_SCREEN_HEIGHT 240
#define TPAGE_WIDTH           256
#define TPAGE_ABR_SHIFT       5
#define TPAGE_ABR_MASK        3

typedef enum SlashColorPhase {
    SLASH_COLOR_PHASE_INTRO = 1,
    SLASH_COLOR_PHASE_HOLD = 2,
    SLASH_COLOR_PHASE_FAST_INTRO = 3,
    SLASH_COLOR_PHASE_FAST_HOLD = 4
} SlashColorPhase;

typedef struct OverlayDesc {
    short x;
    short y;
    CVECTOR color; /* r, g, b = slash glow RGB; cd = hold duration in frames */
    short kind;
    short delay;
} OverlayDesc;

typedef struct OpeningOverlayTexTable {
    OverlayTex banner[6][5];
    OverlayTex slash[17];
} OpeningOverlayTexTable;

typedef struct OpeningOverlayState {
    OpeningOverlayTexTable *textures;
    short tick;
    short unread; /* set to 0 at init; nothing reads it */
} OpeningOverlayState;

typedef struct OpeningBannerState {
    short activeBanner;
    short timer;
    short cueIndex; /* written, never read */
} OpeningBannerState;

/* Position, frame count, and scroll direction for opening title banners 0..5. */
TitleEntry gOpeningBannerEntries[6] = {
    { 0x30, 5, 0x170, 0x30, 1 },
    { 0x28, 5, 0x40, 0x30, 1 },
    { 0x40, 5, 0x40, 0x30, -1 },
    { 0x40, 4, 0x170, 0x30, -1 },
    { 0x40, 5, 0x40, 0x30, 1 },
    { 0x30, 3, 0x170, 0x30, -1 },
};

OpeningOverlayState gOpeningOverlayState;
OpeningBannerState gOpeningBannerState;

void InitOpeningOverlaySystem(OverlayTex *texTable)
{
    gOpeningOverlayState.textures = (OpeningOverlayTexTable *)texTable;
    gOpeningOverlayState.tick = 0;
    gOpeningOverlayState.unread = 0;
    gOpeningBannerState.activeBanner = -1;
}

#ifdef VERSION_EU
#define OPENING_CARD_OVERLAY_HOLD 0x2c
#else
#define OPENING_CARD_OVERLAY_HOLD 0x6c
#endif

OverlayDesc gOpeningOverlayDescs[17] = {
    { 142, 110, { 0x80, 0x80, 0x80, 0x96 }, SLASH_OVERLAY_WIDE_FADE, 0 },
    { 300, 150, { 0x04, 0x40, 0x18, 0x45 }, SLASH_OVERLAY_HORIZONTAL_FAST, 0 },
    { 300, 150, { 0x04, 0x40, 0x18, 0x45 }, SLASH_OVERLAY_HORIZONTAL_FAST, 0 },
    { 300, 161, { 0x08, 0x80, 0x30, 0x36 }, SLASH_OVERLAY_HORIZONTAL_FAST, 0 },
    { 72, 104, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 312, 104, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 312, 56, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 312, 144, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 72, 56, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 72, 144, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 312, 56, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 312, 144, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 72, 56, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 72, 144, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 72, 160, { 0x08, 0x80, 0x30, OPENING_CARD_OVERLAY_HOLD }, SLASH_OVERLAY_HORIZONTAL, 12 },
    { 44, 42, { 0x80, 0x80, 0x80, 0xf0 }, SLASH_OVERLAY_WIDE_INSTANT, 0 },
    { 92, 190, { 0x80, 0x80, 0x80, 0xf0 }, SLASH_OVERLAY_WIDE_FLASH, 0 },
};
/* Banner index (0..5, or -1) by opening overlay cue 0..9; nothing reads entries 10 and 11. */
signed char gOpeningOverlayToBanner[12] = { -1, -1, -1, 0, 1, 2, 3, 4, 5, -1, 0, 0 };
signed char gOpeningOverlayGroups[10][4] = {
    { 0, -1, -1, -1 },
    { 2, 1, -1, -1 },
    { 3, -1, -1, -1 },
    { 4, -1, -1, -1 },
    { 5, -1, -1, -1 },
    { 6, 7, -1, -1 },
    { 8, 9, -1, -1 },
    { 10, 11, -1, -1 },
    { 12, 13, -1, -1 },
    { 15, 16, -1, -1 },
};
/* Zero translation matrix shared with 3D renderers via SetTransMatrix. */
MATRIX gZeroTransMatrix = {
    { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { 0, 0, 0 },
};

void TriggerOpeningOverlay(int overlayIndex)
{
    SlashOverlayPayload payload;
    OverlayTex *tex;
    int i;
    int descId;

    gOpeningBannerState.activeBanner = gOpeningOverlayToBanner[overlayIndex];
    if (gOpeningBannerState.activeBanner != -1) {
        gOpeningBannerState.timer = 0;
        gOpeningBannerState.cueIndex = overlayIndex;
    }

    for (i = 0; i < 4; i++) {
        descId = gOpeningOverlayGroups[overlayIndex][i];
        if (descId == -1) {
            break;
        }
        payload.kind = gOpeningOverlayDescs[descId].kind;
        payload.delay = gOpeningOverlayDescs[descId].delay;
        tex = &gOpeningOverlayState.textures->slash[descId];
        payload.tpage = tex->tpage;
        payload.clut = tex->clut;
        payload.x = gOpeningOverlayDescs[descId].x;
        payload.y = gOpeningOverlayDescs[descId].y;
        payload.width = tex->x1 - tex->x0 + 1;
        payload.height = tex->y1 - tex->y0 + 1;
        payload.u = tex->x0;
        payload.v = tex->y0;
        if (GetMonochromeOption() != 0) {
            payload.color.r = payload.color.g = payload.color.b =
                (gOpeningOverlayDescs[descId].color.r + gOpeningOverlayDescs[descId].color.g +
                 gOpeningOverlayDescs[descId].color.b) / 3;
            payload.color.cd = gOpeningOverlayDescs[descId].color.cd;
        } else {
            payload.color = gOpeningOverlayDescs[descId].color;
        }
        AllocSlashOverlaySlot(&payload);
    }
}

/* The frame the title starts fading out: the European build holds the title 64 frames shorter. */
#ifdef VERSION_EU
#define TITLE_FADE_OUT 86
#else
#define TITLE_FADE_OUT 150
#endif

int UpdateOpeningBannerPackets(char **cursor)
{
    void *prims[32];
    int depths[32];
    TitleEntry *entry;
    OpeningOverlayState *state;
    SPRT *sprite;
    char *p;
    void **out;
    int *depthOut;
    int count;
    int index;
    int step;
    int scroll;
    int shade;
    int i;

    state = &gOpeningOverlayState;
    count = 0;
    p = *cursor;
    for (i = 31; i >= 0; i--) {
        depths[i] = 0;
    }
    state->tick++;
    if (gOpeningBannerState.activeBanner >= 0) {
        index = gOpeningBannerState.activeBanner;
        entry = &gOpeningBannerEntries[index];
        gOpeningBannerState.timer++;
        step = 0;
        if (gOpeningBannerState.timer >= 60) {
            step = (gOpeningBannerState.timer - 60) / 2 + 1;
            if (step > entry->frames - 1) {
                step = entry->frames - 1;
            }
        }
        scroll = gOpeningBannerState.timer / 4;
        if (scroll > 32) {
            scroll = 32;
        }
        if (entry->dir < 0) {
            scroll = 32 - scroll;
        }
        if (gOpeningBannerState.timer < 8) {
            shade = gOpeningBannerState.timer * 16;
        } else if (gOpeningBannerState.timer < TITLE_FADE_OUT) {
            shade = 128;
        } else {
            shade = 128 - (gOpeningBannerState.timer - TITLE_FADE_OUT) * 8;
            if (shade < 0) {
                shade = 0;
            }
        }
        if (step > 0) {
            sprite = (SPRT *)p;
            SetSprt(sprite);
            sprite->r0 = shade;
            sprite->g0 = shade;
            sprite->b0 = shade;
            sprite->x0 = entry->x;
            sprite->y0 = entry->y + entry->dy;
            sprite->w = 160;
            sprite->h = state->textures->banner[index][step].y1 - state->textures->banner[index][step].y0;
            sprite->u0 = state->textures->banner[index][step].x0 + scroll;
            sprite->v0 = state->textures->banner[index][step].y0;
            sprite->clut = state->textures->banner[index][step].clut;
            prims[count++] = p;
            p += sizeof(SPRT);
            SetDrawMode((DR_MODE *)p, 0, 1, state->textures->banner[index][step].tpage, 0);
            prims[count++] = p;
            p += sizeof(DR_MODE);
        }
        sprite = (SPRT *)p;
        SetSprt(sprite);
        sprite->r0 = shade;
        sprite->g0 = shade;
        sprite->b0 = shade;
        sprite->x0 = entry->x;
        sprite->y0 = entry->y;
        sprite->w = 160;
        sprite->h = state->textures->banner[index][0].y1 - state->textures->banner[index][0].y0;
        sprite->u0 = state->textures->banner[index][0].x0 + scroll;
        sprite->v0 = state->textures->banner[index][0].y0;
        sprite->clut = state->textures->banner[index][0].clut;
        prims[count++] = p;
        p += sizeof(SPRT);
        SetDrawMode((DR_MODE *)p, 0, 1, state->textures->banner[index][0].tpage, 0);
        prims[count++] = p;
        p += sizeof(DR_MODE);
        if (gOpeningBannerState.timer >= TITLE_FADE_OUT + 17) {
            gOpeningBannerState.activeBanner = -1;
        }
    }
    *cursor = p;
    out = (void **)p;
    depthOut = (int *)(out + count);
    for (i = 0; i < count; i++) {
        out[i] = prims[i];
    }
    for (i = 0; i < count; i++) {
        depthOut[i] = depths[i];
    }
    return count;
}

SlashOverlaySlot gSlashOverlaySlots[SLASH_OVERLAY_SLOT_COUNT];

void ClearSlashOverlaySlots(void)
{
    int i;

    for (i = 0; i < SLASH_OVERLAY_SLOT_COUNT; i++) {
        gSlashOverlaySlots[i].state = SLASH_OVERLAY_STATE_IDLE;
    }
}


int AllocSlashOverlaySlot(const SlashOverlayPayload *source)
{
    SlashOverlaySlot *slot = gSlashOverlaySlots;
    int i = 0;

    while (i < SLASH_OVERLAY_SLOT_COUNT && slot->state != SLASH_OVERLAY_STATE_IDLE) {
        ++i;
        ++slot;
    }

    if (i == SLASH_OVERLAY_SLOT_COUNT)
        return -1;

    slot->timer = slot->state = SLASH_OVERLAY_STATE_INTRO;
    slot->payload = *source;
    return i;
}


static void BuildSemiTransLineG2(LINE_G2 *line, short x0, short y0, short x1,
                   short y1, unsigned long rgb0, unsigned long rgb1)
{
    SetLineG2(line);
    line->x0 = x0;
    line->y0 = y0;
    line->x1 = x1;
    line->y1 = y1;
    *(unsigned long *)&line->r0 = (rgb0 & GPU_RGB_MASK) | GPU_CODE_WORD(GPU_CODE_LINE_G2 | GPU_CODE_SEMITRANS);
    *(unsigned long *)&line->r1 = (rgb1 & GPU_RGB_MASK) | GPU_CODE_WORD(GPU_CODE_LINE_G2 | GPU_CODE_SEMITRANS);
}

static int EmitOverlaySlashBeams(PacketBuilder *build, SlashOverlaySlot *desc, int index, char **cursor,
                  CVECTOR *glow, CVECTOR *edge)
{
    char *p;
    int quarterW;
    int quarterH;
    int slant;

    p = *cursor;
    switch (desc->payload.kind) {
    case SLASH_OVERLAY_HORIZONTAL:
    case SLASH_OVERLAY_HORIZONTAL_ALT:
    case SLASH_OVERLAY_HORIZONTAL_FAST:
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x - 64, desc->payload.y + desc->payload.height / 2,
            desc->payload.x + desc->payload.width / 2 - 1, desc->payload.y + desc->payload.height / 2,
            0, *(unsigned long *)glow);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2, desc->payload.y + desc->payload.height / 2,
            desc->payload.x + desc->payload.width + 64, desc->payload.y + desc->payload.height / 2,
            *(unsigned long *)glow, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x, desc->payload.y + desc->payload.height / 2 + 1,
            desc->payload.x + desc->payload.width / 2 - 1, desc->payload.y + desc->payload.height / 2 + 1,
            0, *(unsigned long *)edge);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2, desc->payload.y + desc->payload.height / 2 + 1,
            desc->payload.x + desc->payload.width, desc->payload.y + desc->payload.height / 2 + 1,
            *(unsigned long *)edge, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x, desc->payload.y + desc->payload.height / 2 - 1,
            desc->payload.x + desc->payload.width / 2 - 1, desc->payload.y + desc->payload.height / 2 - 1,
            0, *(unsigned long *)edge);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2, desc->payload.y + desc->payload.height / 2 - 1,
            desc->payload.x + desc->payload.width, desc->payload.y + desc->payload.height / 2 - 1,
            *(unsigned long *)edge, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        SetDrawMode((DR_MODE *)p, 0, 1, GetTPage(0, 1, 0, 0), 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(DR_MODE);
        break;
    case SLASH_OVERLAY_DIAG_HORIZONTAL:
    case SLASH_OVERLAY_DIAG_TRI:
        quarterW = desc->payload.width / 4;
        quarterH = desc->payload.height / 4;
        slant = desc->payload.width / desc->payload.height / 2;
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x - quarterW, desc->payload.y + desc->payload.height + quarterH,
            desc->payload.x + desc->payload.width / 2 - 1, desc->payload.y + desc->payload.height / 2,
            0, *(unsigned long *)glow);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2, desc->payload.y + desc->payload.height / 2,
            desc->payload.x + desc->payload.width + quarterW, desc->payload.y - quarterH,
            *(unsigned long *)glow, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x - quarterW - slant, desc->payload.y + desc->payload.height + quarterH,
            desc->payload.x + desc->payload.width / 2 - 1 - slant, desc->payload.y + desc->payload.height / 2,
            0, *(unsigned long *)edge);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2 - slant, desc->payload.y + desc->payload.height / 2,
            desc->payload.x + desc->payload.width + quarterW - slant, desc->payload.y - quarterH,
            *(unsigned long *)edge, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x - quarterW + slant, desc->payload.y + desc->payload.height + quarterH,
            desc->payload.x + desc->payload.width / 2 - 1 + slant, desc->payload.y + desc->payload.height / 2,
            0, *(unsigned long *)edge);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        BuildSemiTransLineG2(
            (LINE_G2 *)p,
            desc->payload.x + desc->payload.width / 2 + slant, desc->payload.y + desc->payload.height / 2,
            desc->payload.x + desc->payload.width + quarterW + slant, desc->payload.y - quarterH,
            *(unsigned long *)edge, 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(LINE_G2);
        SetDrawMode((DR_MODE *)p, 0, 1, GetTPage(0, 1, 0, 0), 0);
        build->packets[index] = p;
        build->depths[index] = 0;
        index++;
        p += sizeof(DR_MODE);
        break;
    }
    *cursor = p;
    return index;
}

static void ScaleClampRgbColor(CVECTOR *sourceColor, CVECTOR *outColor, int scale, int divisor)
{
    int channel;

    channel = sourceColor->r * scale / divisor;
    if (channel > 255) {
        channel = 255;
    } else if (channel < 0) {
        channel = 0;
    }
    outColor->r = channel;

    channel = sourceColor->g * scale / divisor;
    if (channel > 255) {
        channel = 255;
    } else if (channel < 0) {
        channel = 0;
    }
    outColor->g = channel;

    channel = sourceColor->b * scale / divisor;
    if (channel > 255) {
        channel = 255;
    } else if (channel < 0) {
        channel = 0;
    }
    outColor->b = channel;
}

static void CalcOverlaySlashColors(int phase, int timer, CVECTOR *baseColor,
                   CVECTOR *glow, CVECTOR *edge)
{
    switch (phase) {
    case SLASH_COLOR_PHASE_HOLD:
        ScaleClampRgbColor(baseColor, glow, 60 - timer, 8);
        ScaleClampRgbColor(baseColor, edge, 30 - timer / 2, 8);
        break;
    case SLASH_COLOR_PHASE_INTRO:
        ScaleClampRgbColor(baseColor, glow, timer * 8, 16);
        ScaleClampRgbColor(baseColor, edge, timer * 4, 16);
        break;
    case SLASH_COLOR_PHASE_FAST_INTRO:
        ScaleClampRgbColor(baseColor, glow, timer * 8, 16);
        ScaleClampRgbColor(baseColor, edge, timer * 4, 16);
        break;
    case SLASH_COLOR_PHASE_FAST_HOLD:
        ScaleClampRgbColor(baseColor, glow, 60 - timer * 2, 8);
        ScaleClampRgbColor(baseColor, edge, 30 - timer, 8);
        break;
    default:
        glow->b = 0;
        glow->g = 0;
        glow->r = 0;
        edge->b = 0;
        edge->g = 0;
        edge->r = 0;
        break;
    }
}

static int EmitHorizontalSplitQuad(PacketBuilder *build, SlashOverlaySlot *desc, int index, char **cursor)
{
    POLY_FT4 *quad;
    int rows;
    int halfHeight;
    int yShift;
    int stripHeight;
    int xShift;
    u_long rgbCode;

    quad = (POLY_FT4 *)*cursor;
    halfHeight = desc->payload.height / 2;
    rows = desc->timer;
    yShift = halfHeight - rows;
    if (yShift < 0) {
        yShift = 0;
    }
    stripHeight = halfHeight - yShift;

    SetPolyFT4(quad);
    xShift = yShift * 2;
    quad->x0 = desc->payload.x + xShift;
    quad->y0 = desc->payload.y + yShift;
    quad->x1 = desc->payload.x + xShift + desc->payload.width - 1;
    quad->y1 = desc->payload.y + yShift;
    quad->x2 = desc->payload.x + xShift;
    quad->y2 = desc->payload.y + yShift + stripHeight;
    quad->x3 = desc->payload.x + xShift + desc->payload.width - 1;
    quad->y3 = desc->payload.y + yShift + stripHeight;
    quad->tpage = desc->payload.tpage;
    quad->clut = desc->payload.clut;
    quad->u0 = desc->payload.u;
    quad->v0 = desc->payload.v;
    quad->u1 = desc->payload.u + desc->payload.width - 1;
    quad->v1 = desc->payload.v;
    quad->u2 = desc->payload.u;
    quad->v2 = desc->payload.v + stripHeight;
    quad->u3 = desc->payload.u + desc->payload.width - 1;
    quad->v3 = desc->payload.v + stripHeight;
    /* Neutral RGB texture modulation and the FT4 opcode share one word. */
    rgbCode = GPU_CODE_WORD(GPU_CODE_POLY_FT4) | GPU_RGB_NEUTRAL;
    *(u_long *)&quad->r0 = rgbCode;
    if (((desc->payload.tpage >> TPAGE_ABR_SHIFT) & TPAGE_ABR_MASK) != 0) {
        *(u_long *)&quad->r0 = GPU_CODE_WORD(GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS) | GPU_RGB_NEUTRAL;
    }

    build->packets[index] = quad;
    build->depths[index] = 0;
    quad++;

    SetPolyFT4(quad);
    quad->x0 = desc->payload.x - xShift;
    quad->y0 = desc->payload.y - yShift + desc->payload.height - stripHeight;
    quad->x1 = desc->payload.x - xShift + desc->payload.width - 1;
    quad->y1 = desc->payload.y - yShift + desc->payload.height - stripHeight;
    quad->x2 = desc->payload.x - xShift;
    quad->y2 = desc->payload.y - yShift + desc->payload.height - 1;
    quad->x3 = desc->payload.x - xShift + desc->payload.width - 1;
    quad->y3 = desc->payload.y - yShift + desc->payload.height - 1;
    quad->tpage = desc->payload.tpage;
    quad->clut = desc->payload.clut;
    quad->u0 = desc->payload.u;
    quad->v0 = desc->payload.v + desc->payload.height - stripHeight;
    quad->u1 = desc->payload.u + desc->payload.width - 1;
    quad->v1 = desc->payload.v + desc->payload.height - stripHeight;
    quad->u2 = desc->payload.u;
    quad->v2 = desc->payload.v + desc->payload.height - 1;
    quad->u3 = desc->payload.u + desc->payload.width - 1;
    quad->v3 = desc->payload.v + desc->payload.height - 1;
    *(u_long *)&quad->r0 = rgbCode;
    ++index;
    if (((desc->payload.tpage >> TPAGE_ABR_SHIFT) & TPAGE_ABR_MASK) != 0) {
        *(u_long *)&quad->r0 = GPU_CODE_WORD(GPU_CODE_POLY_FT4 | GPU_CODE_SEMITRANS) | GPU_RGB_NEUTRAL;
    }

    build->packets[index] = quad;
    build->depths[index] = 0;
    quad++;
    *cursor = (char *)quad;
    return index + 1;
}

static int EmitDiagonalSplitTriangles(PacketBuilder *build, SlashOverlaySlot *desc, int index, char **cursor)
{
    POLY_FT3 *poly = (POLY_FT3 *)*cursor;
    int yInset = desc->payload.height - 2 * desc->timer;
    int xInset;
    unsigned int color;

    if (yInset < 0) {
        yInset = 0;
    }
    xInset = (desc->payload.width - 1) * (desc->payload.height - yInset) / desc->payload.height;
    SetPolyFT3(poly);
    poly->x0 = desc->payload.x;
    poly->y0 = desc->payload.y + yInset;
    poly->x1 = desc->payload.x + xInset;
    poly->y1 = desc->payload.y + yInset;
    poly->x2 = desc->payload.x;
    poly->y2 = desc->payload.y + desc->payload.height;
    poly->tpage = desc->payload.tpage;
    poly->clut = desc->payload.clut;
    poly->u0 = desc->payload.u;
    poly->v0 = desc->payload.v;
    poly->u1 = desc->payload.u + xInset;
    poly->v1 = desc->payload.v;
    poly->u2 = desc->payload.u;
    poly->v2 = desc->payload.v + desc->payload.height - yInset;
    color = GPU_CODE_WORD(GPU_CODE_POLY_FT3 | GPU_CODE_SEMITRANS) | GPU_RGB_NEUTRAL;
    *(unsigned int *)&poly->r0 = color;
    build->packets[index] = poly;
    build->depths[index] = 0;
    ++poly;

    SetPolyFT3(poly);
    poly->x0 = desc->payload.x + desc->payload.width - 1;
    poly->y0 = desc->payload.y + desc->payload.height - yInset;
    poly->x1 = desc->payload.x + desc->payload.width - 1 - xInset;
    poly->y1 = desc->payload.y + desc->payload.height - yInset;
    poly->x2 = desc->payload.x + desc->payload.width - 1;
    poly->y2 = desc->payload.y;
    poly->tpage = desc->payload.tpage;
    poly->clut = desc->payload.clut;
    poly->u0 = desc->payload.u + desc->payload.width - 1;
    poly->v0 = desc->payload.v + desc->payload.height;
    poly->u1 = desc->payload.u + desc->payload.width - 1 - xInset;
    poly->v1 = desc->payload.v + desc->payload.height;
    poly->u2 = desc->payload.u + desc->payload.width - 1;
    poly->v2 = desc->payload.v + yInset;
    ++index;
    *(unsigned int *)&poly->r0 = color;
    build->packets[index] = poly;
    build->depths[index] = 0;
    *cursor = (char *)(++poly);
    return index + 1;
}

static int EmitWideOverlaySprites(PacketBuilder *build, SlashOverlaySlot *desc, int index, char **cursor, int level)
{
    char *p;
    SPRT *sprite;
    POLY_FT4 *quad;
    int x;
    int covered;
    int page;

    p = *cursor;
    x = desc->payload.x;
    covered = TPAGE_WIDTH - desc->payload.u;
    sprite = (SPRT *)p;
    SetSprt(sprite);
    page = 1;
    sprite->x0 = x;
    sprite->y0 = desc->payload.y;
    sprite->w = covered;
    sprite->h = desc->payload.height;
    sprite->u0 = desc->payload.u;
    sprite->v0 = desc->payload.v;
    sprite->clut = desc->payload.clut;
    *(u_long *)&sprite->r0 = GPU_CODE_WORD(GPU_CODE_SPRT) | level << 16 | level << 8 | level;
    build->packets[index] = p;
    build->depths[index++] = 0;
    p += sizeof(SPRT);
    SetDrawMode((DR_MODE *)p, 0, 1, (short)desc->payload.tpage, 0);
    x += covered;
    build->packets[index] = p;
    build->depths[index++] = 0;
    p += sizeof(DR_MODE);
    while (desc->payload.width - covered > TPAGE_WIDTH) {
        sprite = (SPRT *)p;
        SetSprt(sprite);
        sprite->x0 = x;
        sprite->y0 = desc->payload.y;
        sprite->w = TPAGE_WIDTH;
        sprite->h = desc->payload.height;
        sprite->u0 = 0;
        sprite->v0 = desc->payload.v;
        sprite->clut = desc->payload.clut;
        *(u_long *)&sprite->r0 = GPU_CODE_WORD(GPU_CODE_SPRT) | level << 16 | level << 8 | level;
        build->packets[index] = p;
        build->depths[index++] = 0;
        p += sizeof(SPRT);
        SetDrawMode((DR_MODE *)p, 0, 1, (short)desc->payload.tpage + page, 0);
        build->packets[index] = p;
        build->depths[index++] = 0;
        page++;
        p += sizeof(DR_MODE);
        x += TPAGE_WIDTH;
        covered += TPAGE_WIDTH;
    }
    sprite = (SPRT *)p;
    SetSprt(sprite);
    sprite->x0 = x;
    sprite->y0 = desc->payload.y;
    sprite->w = desc->payload.width - covered;
    sprite->h = desc->payload.height;
    sprite->u0 = 0;
    sprite->v0 = desc->payload.v;
    sprite->clut = desc->payload.clut;
    *(u_long *)&sprite->r0 = GPU_CODE_WORD(GPU_CODE_SPRT) | level << 16 | level << 8 | level;
    build->packets[index] = p;
    build->depths[index++] = 0;
    p += sizeof(SPRT);
    SetDrawMode((DR_MODE *)p, 0, 1, (short)desc->payload.tpage + page, 0);
    build->packets[index] = p;
    build->depths[index++] = 0;
    p += sizeof(DR_MODE);
    if (desc->payload.kind == SLASH_OVERLAY_WIDE_INSTANT || desc->payload.kind == SLASH_OVERLAY_WIDE_FADE) {
        quad = (POLY_FT4 *)p;
        SetPolyFT4(quad);
        setXY4(quad, 0, 0, OPENING_SCREEN_WIDTH, 0,
               0, OPENING_SCREEN_HEIGHT, OPENING_SCREEN_WIDTH, OPENING_SCREEN_HEIGHT);
        setUV4(quad, 0, 0, 0, 0, 0, 0, 0, 0);
        quad->clut = desc->payload.clut;
        quad->tpage = desc->payload.tpage;
        setRGB0(quad, level, level, level);
        build->packets[index] = p;
        build->depths[index++] = 0;
        p += sizeof(POLY_FT4);
    }
    *cursor = p;
    return index;
}

int UpdateSlashOverlays(PacketBuilder *build)
{
    int depths[40];
    void *packets[40];
    CVECTOR glow;
    CVECTOR edge;
    char *cursor;
    SlashOverlaySlot *desc;
    TILE *tile;
    int index;
    int i;
    int level;
    u_long code;
    int fade;
    void **outPackets;
    int *outDepths;
    int j;

    index = 0;
    cursor = build->cursor;
    build->depths = depths;
    build->packets = packets;
    for (i = 0; i < SLASH_OVERLAY_SLOT_COUNT; i++) {
        desc = &gSlashOverlaySlots[SLASH_OVERLAY_SLOT_COUNT - 1 - i];
        if (desc->state == SLASH_OVERLAY_STATE_IDLE) {
            continue;
        }
        if (desc->payload.delay != 0) {
            desc->payload.delay--;
            continue;
        }
        if (desc->payload.kind >= SLASH_OVERLAY_WIDE_FLASH) {
            if (desc->state == SLASH_OVERLAY_STATE_INTRO) {
                if (desc->payload.kind == SLASH_OVERLAY_WIDE_FLASH) {
                    tile = (TILE *)cursor;
                    level = ((20 - desc->timer) * 255) / 20;
                    SetTile(tile);
                    SetSemiTrans(tile, 1);
                    tile->w = OPENING_SCREEN_WIDTH;
                    tile->h = OPENING_SCREEN_HEIGHT;
                    tile->x0 = 0;
                    tile->y0 = 0;
                    tile->r0 = level;
                    tile->g0 = level;
                    tile->b0 = level;
                    packets[index] = tile;
                    depths[index] = 0;
                    index++;
                    cursor = (char *)(tile + 1);
                    SetDrawMode((DR_MODE *)cursor, 0, 1, GetTPage(0, 1, 0, 0), 0);
                    level = 0x80;
                    build->packets[index] = cursor;
                    build->depths[index] = 0;
                    index++;
                    cursor += sizeof(DR_MODE);
                } else if (desc->payload.kind == SLASH_OVERLAY_WIDE_INSTANT) {
                    level = 0x80;
                } else {
                    level = (desc->timer << 7) / 20;
                }
                desc->timer++;
                if (desc->timer >= 20) {
                    desc->timer = 0;
                    desc->state = SLASH_OVERLAY_STATE_HOLD;
                }
            } else if (desc->state == SLASH_OVERLAY_STATE_HOLD) {
                desc->timer++;
                level = 0x80;
                if (desc->timer >= desc->payload.color.cd) {
                    desc->timer = 0;
                    desc->state = SLASH_OVERLAY_STATE_FADEOUT;
                }
            } else if (desc->state == SLASH_OVERLAY_STATE_FADEOUT) {
                level = ((20 - desc->timer) << 7) / 20;
                desc->timer++;
                if (desc->timer >= 20) {
                    desc->state = SLASH_OVERLAY_STATE_IDLE;
                }
            }
            index = EmitWideOverlaySprites(build, desc, index, &cursor, level);
        } else {
            if (desc->state == SLASH_OVERLAY_STATE_INTRO) {
                CalcOverlaySlashColors(
                    desc->payload.kind == SLASH_OVERLAY_HORIZONTAL_FAST ? SLASH_COLOR_PHASE_FAST_INTRO
                                                                        : SLASH_COLOR_PHASE_INTRO,
                    desc->timer, &desc->payload.color, &glow, &edge);
                index = EmitOverlaySlashBeams(build, desc, index, &cursor, &glow, &edge);
                desc->timer = 0;
                desc->state = SLASH_OVERLAY_STATE_HOLD;
            } else if (desc->state == SLASH_OVERLAY_STATE_HOLD) {
                CalcOverlaySlashColors(
                    desc->payload.kind == SLASH_OVERLAY_HORIZONTAL_FAST ? SLASH_COLOR_PHASE_FAST_HOLD
                                                                        : SLASH_COLOR_PHASE_HOLD,
                    desc->timer, &desc->payload.color, &glow, &edge);
                index = EmitOverlaySlashBeams(build, desc, index, &cursor, &glow, &edge);
                switch (desc->payload.kind) {
                case SLASH_OVERLAY_HORIZONTAL:
                case SLASH_OVERLAY_DIAG_HORIZONTAL:
                case SLASH_OVERLAY_HORIZONTAL_ALT:
                case SLASH_OVERLAY_HORIZONTAL_FAST:
                    index = EmitHorizontalSplitQuad(build, desc, index, &cursor);
                    break;
                case SLASH_OVERLAY_DIAG_TRI:
                    index = EmitDiagonalSplitTriangles(build, desc, index, &cursor);
                    break;
                }
                desc->timer++;
                if (desc->timer >= desc->payload.color.cd) {
                    desc->timer = 0;
                    desc->state = SLASH_OVERLAY_STATE_FADEOUT;
                }
            } else if (desc->state == SLASH_OVERLAY_STATE_FADEOUT) {
                fade = 16 - desc->timer;
                SetPolyFT4((POLY_FT4 *)cursor);
                ((POLY_FT4 *)cursor)->x0 = desc->payload.x;
                ((POLY_FT4 *)cursor)->y0 = desc->payload.y;
                ((POLY_FT4 *)cursor)->x1 = desc->payload.x + desc->payload.width - 1;
                ((POLY_FT4 *)cursor)->y1 = desc->payload.y;
                ((POLY_FT4 *)cursor)->x2 = desc->payload.x;
                ((POLY_FT4 *)cursor)->y2 = desc->payload.y + desc->payload.height - 1;
                ((POLY_FT4 *)cursor)->x3 = desc->payload.x + desc->payload.width - 1;
                ((POLY_FT4 *)cursor)->y3 = desc->payload.y + desc->payload.height - 1;
                ((POLY_FT4 *)cursor)->tpage = desc->payload.tpage;
                ((POLY_FT4 *)cursor)->clut = desc->payload.clut;
                ((POLY_FT4 *)cursor)->u0 = desc->payload.u;
                ((POLY_FT4 *)cursor)->v0 = desc->payload.v;
                ((POLY_FT4 *)cursor)->u1 = desc->payload.u + desc->payload.width - 1;
                ((POLY_FT4 *)cursor)->v1 = desc->payload.v;
                ((POLY_FT4 *)cursor)->u2 = desc->payload.u;
                ((POLY_FT4 *)cursor)->v2 = desc->payload.v + desc->payload.height - 1;
                ((POLY_FT4 *)cursor)->u3 = desc->payload.u + desc->payload.width - 1;
                ((POLY_FT4 *)cursor)->v3 = desc->payload.v + desc->payload.height - 1;
                level = fade << 3;
                code = GPU_CODE_WORD(GPU_CODE_POLY_FT4) | (level << 16) | (level << 8) | level;
                *(u_long *)&((POLY_FT4 *)cursor)->r0 = code;
                if (((desc->payload.tpage >> TPAGE_ABR_SHIFT) & TPAGE_ABR_MASK) != 0) {
                    *(u_long *)&((POLY_FT4 *)cursor)->r0 = code | GPU_CODE_WORD(GPU_CODE_SEMITRANS);
                }
                depths[index] = 0;
                packets[index] = cursor;
                cursor += sizeof(POLY_FT4);
                index++;
                desc->timer++;
                if (desc->timer >= 16) {
                    desc->state = SLASH_OVERLAY_STATE_IDLE;
                }
            }
        }
    }
    outPackets = (void **)cursor;
    outDepths = (int *)(outPackets + index);
    build->cursor = cursor;
    for (j = 0; j < index; j++) {
        outPackets[j] = packets[j];
    }
    for (j = 0; j < index; j++) {
        outDepths[j] = depths[j];
    }
    return index;
}

/* Perspective screen distance h = 320 * cot(fov / 2) for a 640-pixel width. */
int CalcGeomScreenFromFov(int degrees)
{
    int angle = (degrees << 12) / 360;
    int halfAngle = angle / 2;
    return (gSinLutQ12[(halfAngle + 1024) & 4095] * 320) /
        gSinLutQ12[halfAngle & 4095];
}
