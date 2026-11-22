#ifndef LINK_H
#define LINK_H

#include "pad.h"

/* One 32-bit word of a serial link packet, accessed as a word or its 16-bit halves. */
typedef union {
    int word;
    unsigned short half[2];
} LinkWord;

/* An 8-byte serial link packet:
 *   header: [31] LinkRole, [30] disconnect request, [29:28] LinkSyncPhase,
 *           [27] daytime option, [19:16] PadType, [15:0] button mask
 *   data:   [31:24] left stick X (analog[2]), [23:16] left stick Y (analog[3]),
 *           [15:0] 16-bit XOR checksum of the three upper 16-bit halves. */
typedef struct {
    LinkWord header;
    LinkWord data;
} LinkPacket;

/* LinkPacket.header bit fields. */
#define LINK_HEADER_ROLE_SHIFT 31
#define LINK_HEADER_DISCONNECT_SHIFT 30
#define LINK_HEADER_SYNC_SHIFT 28
#define LINK_HEADER_DAYTIME_SHIFT 27
#define LINK_HEADER_PAD_TYPE_SHIFT 16
#define LINK_HEADER_SYNC_MASK 3
#define LINK_HEADER_PAD_TYPE_MASK 0xf
#define LINK_HEADER_DISCONNECT 0x40000000

/* 2-bit Link sync barrier phase (gLinkSyncPhase; LinkPacket.header bits 29:28). */
typedef enum LinkSyncPhase {
    LINK_SYNC_NORMAL = 0,
    LINK_SYNC_PAUSE = 1,
    LINK_SYNC_BARRIER = 2
} LinkSyncPhase;

/* Link Cable console role (gLinkRole; LinkPacket.header bit 31). */
typedef enum LinkRole {
    LINK_ROLE_HOST = 0,
    LINK_ROLE_CLIENT = 1
} LinkRole;

/* Link Cable lobby substate (gLinkLobbyState, UpdateLinkLobbyState). */
typedef enum LinkLobbyState {
    LINK_LOBBY_CHECK_CABLE = 0,
    LINK_LOBBY_NO_CABLE = 1,
    LINK_LOBBY_INIT_CABLE = 2,
    LINK_LOBBY_WAIT_PEER = 3,
    LINK_LOBBY_CLEAR_INPUT_1 = 4,
    LINK_LOBBY_CLEAR_INPUT_2 = 5,
    LINK_LOBBY_CLEAR_INPUT_3 = 6,
    LINK_LOBBY_START_SESSION = 7,
    LINK_LOBBY_UNUSED_8 = 8,
    LINK_LOBBY_UNUSED_9 = 9,
    LINK_LOBBY_INIT_FAILED = 10
} LinkLobbyState;

void CheckLinkCablePresent(void);
int InitLinkCable(void);
int ShutdownLinkCable(void);
int CloseLinkSession(void);
void RequestLinkDisconnect(void);
int GetLinkPeerDaytimeOption(void);
void ExchangeLinkInputPacket(struct PadInputFrame *input);
void ExchangeIdleLinkPacket(void);
void BeginLinkSyncPause(void);
int WaitLinkSyncBarrier(void);
void InitLinkLobbyState(void);
void UpdateLinkLobbyState(void);

extern short gLinkSyncPhase;
extern short gLinkErrorFlag;

extern short gLinkRole;

#endif
