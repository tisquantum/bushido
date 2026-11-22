/*
 * Textured rectangle lists: streams of groups sharing one 16-byte GlyphTexture, followed by a
 * 32-bit rectangle count (TEXTURED_RECT_LAST_GROUP in bit 31 marks the final group) and `count`
 * short[4] (x, y, width, height) rectangles, emitted as POLY_FT4 primitives.
 */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "display_buffer.h"
#include "globals.h"
#include "main.h"
#include "packet_build.h"
#include "sprite2d.h"

#ifdef VERSION_JP
/* DrawTexturedRectList at the list's own coordinates, linked at the head of the ordering table.
 * Nothing calls it. */
static void DrawTexturedRectListAtOrigin(void *list)
{
    POLY_FT4 *next = gPacketCursor;
    POLY_FT4 *packet;
    GlyphTexture *texture;
    int lastGroup = 0;
    int count;
    int rectX, rectY, width, height;
    unsigned int left, right, top, bottom;

    do {
        texture = (GlyphTexture *)list;
        list = texture + 1;
        count = *(int *)list;
        list = (int *)list + 1;
        if (count < 0) {
            count &= ~TEXTURED_RECT_LAST_GROUP;
            lastGroup = 1;
        }

        while (--count != -1) {
            packet = next;
            setlen(packet, 9);
            *(u_long *)&packet->r0 = *(u_long *)&texture->r;
            *(u_long *)&packet->u0 = *(u_long *)&texture->u0;
            *(u_long *)&packet->u1 = *(u_long *)&texture->u1;
            *(short *)&packet->u2 = *(int *)&texture->u2;
            *(short *)&packet->u3 = *(int *)&texture->u2 >> 16;

            rectX = *(short *)list;
            list = (short *)list + 1;
            rectY = *(short *)list;
            list = (short *)list + 1;
            width = *(short *)list;
            list = (short *)list + 1;
            height = *(short *)list;
            list = (short *)list + 1;

            /* POLY_FT4 words 2, 4, 6, 8 hold packed (x0,y0) .. (x3,y3). */
            left = rectX & 0xffff;
            top = (unsigned int)rectY << 16;
            ((u_long *)packet)[2] = left | top;
            right = (rectX + width) & 0xffff;
            ((u_long *)packet)[4] = right | top;
            bottom = (unsigned int)(rectY + height) << 16;
            ((u_long *)packet)[6] = left | bottom;
            ((u_long *)packet)[8] = right | bottom;

            AddPrim(gCurrentOt, packet);
            ++next;
        }
    } while (!lastGroup);
    gPacketCursor = next;
}
#endif

void DrawTexturedRectList(void *list, int xOffset, int yOffset, int layer)
{
    POLY_FT4 *next = gPacketCursor;
    POLY_FT4 *packet;
    GlyphTexture *texture;
    int lastGroup = 0;
    int count;
    int rectX, rectY, width, height;
    unsigned int left, right, top, bottom;

    do {
        texture = (GlyphTexture *)list;
        list = texture + 1;
        count = *(int *)list;
        list = (int *)list + 1;
        if (count < 0) {
            count &= ~TEXTURED_RECT_LAST_GROUP;
            lastGroup = 1;
        }

        while (--count != -1) {
            packet = next;
            setlen(packet, 9);
            *(u_long *)&packet->r0 = *(u_long *)&texture->r;
            *(u_long *)&packet->u0 = *(u_long *)&texture->u0;
            *(u_long *)&packet->u1 = *(u_long *)&texture->u1;
            *(short *)&packet->u2 = *(int *)&texture->u2;
            *(short *)&packet->u3 = *(int *)&texture->u2 >> 16;

            rectX = *(short *)list;
            list = (short *)list + 1;
            rectY = *(short *)list;
            list = (short *)list + 1;
            width = *(short *)list;
            list = (short *)list + 1;
            height = *(short *)list;
            list = (short *)list + 1;
            rectX += xOffset;
            rectY += yOffset;

            /* POLY_FT4 words 2, 4, 6, 8 hold packed (x0,y0) .. (x3,y3). */
            left = rectX & 0xffff;
            top = (unsigned int)rectY << 16;
            ((u_long *)packet)[2] = left | top;
            right = (rectX + width) & 0xffff;
            ((u_long *)packet)[4] = right | top;
            bottom = (unsigned int)(rectY + height) << 16;
            ((u_long *)packet)[6] = left | bottom;
            ((u_long *)packet)[8] = right | bottom;

            ++next;
            AddPrim(gCurrentOt + layer, packet);
        }
    } while (!lastGroup);
    gPacketCursor = next;
}

static POLY_FT4 gImmediateRectPackets[8]; /* drawn at once with DrawPrim, so reused each list */

void DrawTexturedRectListImmediate(void *list, int xOffset, int yOffset)
{
    POLY_FT4 *packet;
    GlyphTexture *texture;
    int lastGroup;
    int count;
    int rectX, rectY, width, height;
    unsigned int left, right, top, bottom;

    DrawSync(0);
    packet = gImmediateRectPackets;
    lastGroup = 0;
    do {
        texture = (GlyphTexture *)list;
        list = texture + 1;
        count = *(int *)list;
        list = (int *)list + 1;
        if (count < 0) {
            count &= ~TEXTURED_RECT_LAST_GROUP;
            lastGroup = 1;
        }

        while (--count != -1) {
            setlen(packet, 9);
            *(u_long *)&packet->r0 = *(u_long *)&texture->r;
            *(u_long *)&packet->u0 = *(u_long *)&texture->u0;
            *(u_long *)&packet->u1 = *(u_long *)&texture->u1;
            *(short *)&packet->u2 = *(int *)&texture->u2;
            *(short *)&packet->u3 = *(int *)&texture->u2 >> 16;

            rectX = *(short *)list;
            list = (short *)list + 1;
            rectY = *(short *)list;
            list = (short *)list + 1;
            width = *(short *)list;
            list = (short *)list + 1;
            height = *(short *)list;
            list = (short *)list + 1;
            rectX += xOffset;
            rectY += yOffset;

            left = rectX & 0xffff;
            top = (unsigned int)rectY << 16;
            right = (rectX + width) & 0xffff;
            bottom = (unsigned int)(rectY + height) << 16;
            *(u_long *)&packet->x0 = left | top;
            *(u_long *)&packet->x1 = right | top;
            *(u_long *)&packet->x2 = left | bottom;
            *(u_long *)&packet->x3 = right | bottom;

            DrawPrim(packet);
            ++packet;
        }
    } while (!lastGroup);
    DrawSync(0);
}
