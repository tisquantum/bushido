/*
 * The Link cable: libcomb's "sio:" device, the lobby that settles host and client, the checksummed
 * 8-byte packet exchanged each frame inside PollControllers, and the sync barrier around CD loads.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <kernel.h>
#include <libcomb.h>
#include <stdio.h>
#include "comb_control.h"
#include "game_state.h"
#include "link.h"
#include "menu_state.h"
#include "font_hud.h"
#include "main.h"
#include "pad.h"
#include "sound.h"
#include "comb.h"
#include "globals.h"

#define LINK_BAUD_RATE 2073600


/* InitLinkCable clears it; nothing reads it. */
PadInputFrame gLinkPadState;

short gLinkConnected = 0;
short gLocalPort2Enabled = 1;
short gLinkSyncPhase = 0;
short gLinkErrorFlag = 0;
/* Set bits of the received and the sent buttons, summed; nothing reads them. */
int gLinkRxButtonBitCount = 0;
int gLinkTxButtonBitCount = 0;
char gLinkDeviceName[8] = "sio:";
int gLinkReadFd;
int gLinkReadEvent;
int gLinkWriteFd;
int gLinkErrorEvent;
short gLinkCablePresent;

void CheckLinkCablePresent(void)
{
    gLinkCablePresent =
        (_comb_control(COMB_CTRL_GET, COMB_PARAM_STATUS, 0) & (COMB_CTS | COMB_DSR)) != (COMB_CTS | COMB_DSR);
}

static void OpenLinkCableDevice(void)
{
    EnterCriticalSection();
    do {
        gLinkReadEvent = OpenEvent(HwSIO, EvSpIOER, EvMdNOINTR, 0);
    } while (gLinkReadEvent == -1);
    do {
        gLinkErrorEvent = OpenEvent(HwSIO, EvSpERROR, EvMdNOINTR, 0);
    } while (gLinkErrorEvent == -1);
    ExitCriticalSection();
    VSync(2);
    AddCOMB();
    do {
        gLinkWriteFd = open(gLinkDeviceName, O_WRONLY);
    } while (gLinkWriteFd == -1);
    do {
        gLinkReadFd = open(gLinkDeviceName, O_NOWAIT | O_RDONLY);
    } while (gLinkReadFd == -1);
}

short gLinkDisconnectRequested;

int InitLinkCable(void)
{
    OpenLinkCableDevice();
    _comb_control(COMB_CTRL_RESET, COMB_RESET_ALL, 0);
    _comb_control(COMB_CTRL_SET, COMB_PARAM_BAUD, LINK_BAUD_RATE);
    _comb_control(COMB_CTRL_SET, COMB_PARAM_RX_UNIT, 1);
    gLinkPadState.held = 0;
    gLinkPadState.pressed = 0;
    gLinkPadState.released = 0;
    gLinkPadState.notHeld = 0xffff;
    gLinkDisconnectRequested = 0;
    gLinkSyncPhase = LINK_SYNC_NORMAL;
    gLinkErrorFlag = 0;
    return 1;
}

static void NegotiateLinkHostRole(void)
{
    if (_comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_GET_CTS, 0) != 0) {
        gLinkRole = LINK_ROLE_CLIENT;
    } else {
        gLinkRole = LINK_ROLE_HOST;
    }
    _comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_SET_RTS, 1);
}

static void EnableLinkCableEvents(void)
{
    EnableEvent(gLinkErrorEvent);
    EnableEvent(gLinkReadEvent);
    gLinkConnected = 1;
}

static int CloseLinkCableDevice(void)
{
    close(gLinkReadFd);
    close(gLinkWriteFd);
    EnterCriticalSection();
    CloseEvent(gLinkReadEvent);
    CloseEvent(gLinkErrorEvent);
    ExitCriticalSection();
    VSync(2);
    DelCOMB();
    return _comb_control(COMB_CTRL_SET, COMB_PARAM_CONTROL, 0);
}

int ShutdownLinkCable(void)
{
    int portMask;

    gLocalPort2Enabled = 1;
    if (gLinkRole == LINK_ROLE_HOST) {
        portMask = 0xffff;
    } else {
        portMask = 0xffff0000;
    }
    gPadState.held &= portMask;
    gPadState.pressed &= portMask;
    gPadState.released &= portMask;
    gPadState.notHeld |= ~portMask;
    return CloseLinkCableDevice();
}

int CloseLinkSession(void)
{
    gLinkConnected = 0;
    gLinkDisconnectRequested = 0;
    return ShutdownLinkCable();
}

void RequestLinkDisconnect(void)
{
    gLinkDisconnectRequested = 1;
}

short gLinkWriteOk;

#define LINK_WRITE_MAX_POLLS 0x10000

static int CheckLinkWriteTimeout(int direction, int pollCount)
{
    if ((unsigned int)pollCount > LINK_WRITE_MAX_POLLS) {
        gLinkWriteOk = 0;
        return 0;
    }
    return 1;
}

#define LINK_RCNT_RESET_THRESHOLD 1024
#define LINK_CTS_STABLE_POLLS 1000
#define LINK_SEND_TIMEOUT_TICKS 30720
#define LINK_READ_TIMEOUT_TICKS 15360
#define LINK_READ_MAX_RETRIES 5
#define LINK_SYNC_POLL_DELAY_TICKS 0x100

/* Status register bits 7 and 8: DSR and CTS. */
#define COMB_STATUS_LINE_SHIFT 7
#define COMB_STATUS_LINE_MASK 3
#define COMB_STATUS_DSR_ONLY 1

static int gLinkRxRawBuffer[2];
static LinkPacket gLinkTxPacket;

static int SendLinkPacket(LinkPacket *packet)
{
    int startCount;
    int pollCount;

    startCount = GetRCnt(RCntCNT1);
    if (startCount > LINK_RCNT_RESET_THRESHOLD) {
        ResetRCnt(RCntCNT1);
        startCount = 0;
    }

    do {
        while (_comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_GET_CTS, 0) == 0) {
            if (GetRCnt(RCntCNT1) - startCount > LINK_SEND_TIMEOUT_TICKS) {
                return 0;
            }
        }

        for (pollCount = 0; pollCount < LINK_CTS_STABLE_POLLS; ++pollCount) {
            if (_comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_GET_CTS, 0) == 0) {
                break;
            }
        }
    } while (pollCount < LINK_CTS_STABLE_POLLS);

    _comb_control(COMB_CTRL_SET, COMB_PARAM_CONTROL, COMB_BIT_DTR);
    gLinkWriteOk = 1;
    _comb_control(COMB_CTRL_SET_CALLBACK, COMB_ASYNC_WRITE, (int)CheckLinkWriteTimeout);
    write(gLinkWriteFd, &gLinkTxPacket, sizeof(gLinkTxPacket));
    _comb_control(COMB_CTRL_SET_CALLBACK, COMB_ASYNC_WRITE, 0);
    _comb_control(COMB_CTRL_SET, COMB_PARAM_CONTROL, 0);
    return gLinkWriteOk;
}

static LinkWord gLinkRxHeader;
static LinkWord gLinkRxData;

static void StartLinkPacketRead(void)
{
    read(gLinkReadFd, gLinkRxRawBuffer, sizeof(gLinkRxRawBuffer));
}

static int WaitLinkPacketRead(void)
{
    int startCount;
    int retries = 0;
    int header, data, checksum;

    startCount = GetRCnt(RCntCNT1);
    if (startCount > LINK_RCNT_RESET_THRESHOLD) {
        ResetRCnt(RCntCNT1);
        startCount = 0;
    }
    for (;;) {
        if (TestEvent(gLinkReadEvent)) {
            header = gLinkRxRawBuffer[0];
            data = gLinkRxRawBuffer[1];
            checksum = (header ^ (header >> 16) ^ (data >> 16)) & 0xffff;
            if ((data & 0xffff) != checksum) {
                return 0;
            }
            gLinkRxHeader.word = header;
            gLinkRxData.word = data;
            return 1;
        }
        if (TestEvent(gLinkErrorEvent)) {
            ++retries;
            printf("\nREAD ERROR\n");
            if (retries >= LINK_READ_MAX_RETRIES) {
                break;
            }
            _comb_control(COMB_CTRL_RESET, COMB_RESET_ALL, 0);
            startCount = 0;
            StartLinkPacketRead();
            ResetRCnt(RCntCNT1);
        }
        if (((_comb_control(COMB_CTRL_GET, COMB_PARAM_STATUS, 0) >> COMB_STATUS_LINE_SHIFT) & COMB_STATUS_LINE_MASK) ==
            COMB_STATUS_DSR_ONLY) {
            continue;
        }
        if (GetRCnt(RCntCNT1) - startCount <= LINK_READ_TIMEOUT_TICKS) {
            continue;
        }
        printf("\nREAD TIME OUT:%d\n", retries);
        if (++retries >= LINK_READ_MAX_RETRIES) {
            break;
        }
        ResetRCnt(RCntCNT1);
        startCount = 0;
    }
    return 0;
}

static int CountSetBits(int value)
{
    int count = 0;
    int bit;

    for (bit = 0; bit < 32; ++bit) {
        count += ((unsigned int)value >> bit) & 1;
    }
    return count;
}

int GetLinkPeerDaytimeOption(void)
{
    return (gLinkRxHeader.word >> LINK_HEADER_DAYTIME_SHIFT) & 1;
}

static LinkPacket gLinkPrevTxPacket;
short gLinkFirstExchange;

void ExchangeLinkInputPacket(PadInputFrame *input)
{
    unsigned int buttons;
    int linkRole;
    unsigned short txButtons;
    int header;
    unsigned int data;

    if (gLocalPort2Enabled == 0) {
        unsigned short port1Buttons = input->held;
        input->type[1] = PAD_TYPE_DIGITAL;
        input->held = port1Buttons;
    }
    if (gLinkConnected != 0) {
        buttons = input->held;
        linkRole = gLinkRole;
        txButtons = (unsigned short)buttons;
        header = (unsigned int)linkRole << LINK_HEADER_ROLE_SHIFT |
                 (unsigned int)gLinkDisconnectRequested << LINK_HEADER_DISCONNECT_SHIFT |
                 (gLinkSyncPhase & LINK_HEADER_SYNC_MASK) << LINK_HEADER_SYNC_SHIFT |
                 (gOptionDaytime & 1) << LINK_HEADER_DAYTIME_SHIFT |
                 (unsigned int)input->type[0] << LINK_HEADER_PAD_TYPE_SHIFT | txButtons;
        gLinkTxPacket.header.word = header;
        data = (unsigned int)input->analog[0][2] << 24 | input->analog[0][3] << 16;
        gLinkTxPacket.data.word = data | ((header ^ (header >> 16) ^ ((int)data >> 16)) & 0xffff);

        if (gLinkFirstExchange != 0) {
            if (linkRole == LINK_ROLE_HOST) {
                StartLinkPacketRead();
            } else if (SendLinkPacket(&gLinkTxPacket)) {
                gLinkTxButtonBitCount += CountSetBits(txButtons);
                StartLinkPacketRead();
            } else {
                CloseLinkSession();
                gLinkErrorFlag = 1;
                return;
            }
        } else {
            if (!WaitLinkPacketRead()) {
                CloseLinkSession();
                gLinkErrorFlag = 1;
                return;
            }
            if (gLinkRole == LINK_ROLE_CLIENT &&
                ((gLinkRxHeader.word & LINK_HEADER_DISCONNECT) ||
                 (gLinkPrevTxPacket.header.word & LINK_HEADER_DISCONNECT))) {
                CloseLinkSession();
                gGameState = GAME_STATE_INIT_MAIN_MENU;
                return;
            }
            if (!SendLinkPacket(&gLinkTxPacket)) {
                CloseLinkSession();
                gLinkErrorFlag = 1;
                return;
            }
            gLinkTxButtonBitCount += CountSetBits((unsigned short)buttons);
            StartLinkPacketRead();
            if (gLinkRole == LINK_ROLE_HOST &&
                (gLinkDisconnectRequested != 0 || (gLinkRxHeader.word & LINK_HEADER_DISCONNECT))) {
                CloseLinkSession();
                return;
            }
        }

        if (gLinkFirstExchange == 0) {
            int receivedButtons;

            gLinkRxButtonBitCount += CountSetBits(gLinkRxHeader.half[0]);
            receivedButtons = gLinkRxHeader.half[0];
            if (gLinkRole == LINK_ROLE_HOST) {
                buttons = gLinkTxPacket.header.half[0];
                input->held = (unsigned int)receivedButtons << 16 | buttons;
                input->type[1] = (gLinkRxHeader.word >> LINK_HEADER_PAD_TYPE_SHIFT) & LINK_HEADER_PAD_TYPE_MASK;
                input->analog[1][2] = gLinkRxData.word >> 24;
                input->analog[1][3] = gLinkRxData.word >> 16;
            } else {
                buttons = gLinkPrevTxPacket.header.half[0];
                input->held = buttons << 16 | receivedButtons;
                input->type[0] = (gLinkRxHeader.word >> LINK_HEADER_PAD_TYPE_SHIFT) & LINK_HEADER_PAD_TYPE_MASK;
                input->analog[0][2] = gLinkRxData.word >> 24;
                input->analog[0][3] = gLinkRxData.word >> 16;
                input->type[1] = (gLinkPrevTxPacket.header.word >> LINK_HEADER_PAD_TYPE_SHIFT) &
                    LINK_HEADER_PAD_TYPE_MASK;
                input->analog[1][2] = gLinkPrevTxPacket.data.word >> 24;
                input->analog[1][3] = gLinkPrevTxPacket.data.word >> 16;
            }

            if (gLinkRole == LINK_ROLE_HOST) {
                if (((gLinkRxHeader.word >> LINK_HEADER_SYNC_SHIFT) & LINK_HEADER_SYNC_MASK) == LINK_SYNC_BARRIER &&
                    gLinkSyncPhase == LINK_SYNC_BARRIER) {
                    gLinkSyncPhase = LINK_SYNC_NORMAL;
                }
            } else if (((gLinkRxHeader.word >> LINK_HEADER_SYNC_SHIFT) & LINK_HEADER_SYNC_MASK) == LINK_SYNC_BARRIER &&
                       ((gLinkPrevTxPacket.header.word >> LINK_HEADER_SYNC_SHIFT) & LINK_HEADER_SYNC_MASK) ==
                           LINK_SYNC_BARRIER) {
                gLinkSyncPhase = LINK_SYNC_NORMAL;
            }
        }
        gLinkPrevTxPacket.header.word = gLinkTxPacket.header.word;
        gLinkPrevTxPacket.data.word = gLinkTxPacket.data.word;
        gLinkFirstExchange = 0;
    } else {
        gLinkSyncPhase = LINK_SYNC_NORMAL;
    }
}

void ExchangeIdleLinkPacket(void)
{
    PadInputFrame packet;

    packet.held = 0;
    packet.type[1] = PAD_TYPE_DIGITAL;
    packet.type[0] = PAD_TYPE_DIGITAL;
    ExchangeLinkInputPacket(&packet);
}

void BeginLinkSyncPause(void)
{
    gLinkSyncPhase = LINK_SYNC_PAUSE;
    VSync(2);
    ExchangeIdleLinkPacket();
    VSync(2);
}

int WaitLinkSyncBarrier(void)
{
    gLinkSyncPhase = LINK_SYNC_BARRIER;
    VSync(2);
    do {
        ExchangeIdleLinkPacket();
        if (gLinkConnected == 0)
            break;
        ResetRCnt(RCntCNT1);
        while (GetRCnt(RCntCNT1) < LINK_SYNC_POLL_DELAY_TICKS)
            ;
    } while (gLinkSyncPhase == LINK_SYNC_BARRIER);
    VSync(2);
    ExchangeIdleLinkPacket();
    return VSync(2);
}

short gLinkLobbyScreenReady;
short gLinkLobbyState;

void InitLinkLobbyState(void)
{
    gLinkLobbyScreenReady = 0;
    gLocalPort2Enabled = 0;
    gLinkLobbyState = LINK_LOBBY_CHECK_CABLE;
    gStateFrameTimer = 0;
    gDisplayPresentEnabled = 0;
    gGameState = GAME_STATE_UPDATE_LINK_LOBBY;
}

typedef enum LinkLobbySound {
    LINK_LOBBY_SOUND_CANCEL = 0x0a,
    LINK_LOBBY_SOUND_READY = 0x1f,
    LINK_LOBBY_SOUND_L1 = 0x20,
    LINK_LOBBY_SOUND_L2 = 0x21,
    LINK_LOBBY_SOUND_R1 = 0x22,
    LINK_LOBBY_SOUND_R2 = 0x23
} LinkLobbySound;

#define LINK_LOBBY_READY_SOUND_DELAY 30
#define LINK_LOBBY_READY_COOLDOWN 60
#define LINK_LOBBY_TAUNT_COOLDOWN 30

short gLinkLobbySoundCooldown;
int gLinkLobbyFrameCount;

void UpdateLinkLobbyState(void)
{
    unsigned int pressed;

    gDisplayPresentEnabled = 1;
    if (gLinkLobbyScreenReady != 0) {
        DrawLinkLobbyScreen();
    }
    switch (gLinkLobbyState) {
    case LINK_LOBBY_CHECK_CABLE:
        CheckLinkCablePresent();
        if (gLinkCablePresent != 0) {
            gLinkLobbyState = LINK_LOBBY_INIT_CABLE;
        } else {
            gLinkLobbyState = LINK_LOBBY_NO_CABLE;
        }
        break;
    case LINK_LOBBY_INIT_CABLE:
        if (InitLinkCable() != 0) {
            NegotiateLinkHostRole();
            if (gLinkRole == LINK_ROLE_HOST) {
                WaitGpuIdle();
                LoadLinkLobbyScreen();
                gLinkLobbyScreenReady = 1;
                gLinkLobbyFrameCount = 0;
            }
            gLinkLobbyState++;
        } else {
            gLinkLobbyState = LINK_LOBBY_INIT_FAILED;
        }
        break;
    case LINK_LOBBY_WAIT_PEER:
        if (gLinkRole == LINK_ROLE_HOST) {
            gLinkLobbyFrameCount++;
            if (gLinkLobbyFrameCount == LINK_LOBBY_READY_SOUND_DELAY) {
                PlayMenuSound(LINK_LOBBY_SOUND_READY);
                gLinkLobbySoundCooldown = LINK_LOBBY_READY_COOLDOWN;
            } else if (gLinkLobbyFrameCount > LINK_LOBBY_READY_SOUND_DELAY) {
                if (gLinkLobbySoundCooldown == 0 || --gLinkLobbySoundCooldown == 0) {
                    pressed = gPadState.pressed;
                    if ((pressed & MENU_CONFIRM) != 0) {
                        PlayMenuSound(LINK_LOBBY_SOUND_READY);
                        gLinkLobbySoundCooldown = LINK_LOBBY_READY_COOLDOWN;
                    } else if ((pressed & PADL1) != 0) {
                        PlayMenuSound(LINK_LOBBY_SOUND_L1);
                        gLinkLobbySoundCooldown = LINK_LOBBY_TAUNT_COOLDOWN;
                    } else if ((pressed & PADL2) != 0) {
                        PlayMenuSound(LINK_LOBBY_SOUND_L2);
                        gLinkLobbySoundCooldown = LINK_LOBBY_TAUNT_COOLDOWN;
                    } else if ((pressed & PADR1) != 0) {
                        PlayMenuSound(LINK_LOBBY_SOUND_R1);
                        gLinkLobbySoundCooldown = LINK_LOBBY_TAUNT_COOLDOWN;
                    } else if ((pressed & PADR2) != 0) {
                        PlayMenuSound(LINK_LOBBY_SOUND_R2);
                        gLinkLobbySoundCooldown = LINK_LOBBY_TAUNT_COOLDOWN;
                    }
                }
            }
            if ((gPadState.pressed & MENU_CANCEL) != 0) {
                ShutdownLinkCable();
                PlayMenuSound(LINK_LOBBY_SOUND_CANCEL);
                gGameState = GAME_STATE_INIT_MAIN_MENU;
            } else if (_comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_GET_CTS, 0) != 0) {
                _comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_SET_RTS, 0);
                gLinkLobbyState++;
            }
        } else if (_comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_GET_CTS, 0) == 0) {
            _comb_control(COMB_CTRL_HANDSHAKE, COMB_HANDSHAKE_SET_RTS, 0);
            gLinkLobbyState++;
        }
        break;
    case LINK_LOBBY_CLEAR_INPUT_1:
    case LINK_LOBBY_CLEAR_INPUT_2:
    case LINK_LOBBY_CLEAR_INPUT_3:
        ClearControllerInput();
        gLinkLobbyState++;
        break;
    case LINK_LOBBY_START_SESSION:
        gLinkFirstExchange = 1;
        EnableLinkCableEvents();
        gPlayMode = PLAY_MODE_LINK;
        gHumanPlayerCount = 2;
        gPovModeEnabled = 1;
        gGameState = GAME_STATE_INIT_CHARACTER_SELECT;
        break;
    case LINK_LOBBY_NO_CABLE:
    case LINK_LOBBY_INIT_FAILED:
        gLocalPort2Enabled = 1;
        gGameState = GAME_STATE_LINK_ABORT;
        break;
    case LINK_LOBBY_UNUSED_8:
    case LINK_LOBBY_UNUSED_9:
        break;
    }
    gStateFrameTimer++;
    FlushQueuedSounds();
}
