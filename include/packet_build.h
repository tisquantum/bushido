#ifndef PACKET_BUILD_H
#define PACKET_BUILD_H

/* The opening overlays write packets at cursor and record each packet and its depth in the
 * caller's tables, which are added to the ordering table afterwards. */
typedef struct PacketBuilder {
    char *cursor;
    int *depths;
    void **packets;
} PacketBuilder;

/* The next free byte of the GPU packet buffer of the frame being built. */

extern void *gPacketCursor;

#endif
