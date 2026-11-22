/* Fighter textures: per-character base texture archives (P2 offset +3 pages right of P1), Stage 4
 * snow-triggered alternate archives for story fighters, and 5-bit injury overlay TIM sub-groups. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "fighter.h"
#include "model_arena.h"
#include "packet_build.h"
#include "asset_load.h"
#include "fighter_texture.h"
#include "globals.h"
#include "render.h"
#include "tim.h"

/* Disc files of character 0's base textures (DATA4\D06.DAT) and the first story alternate's extra
 * textures (DATA4\D00.DAT); the Japanese disc holds one file fewer before them. */
#ifdef VERSION_JP
#define CD_FILE_FIGHTER_TEXTURES     107
#define CD_FILE_FIGHTER_ALT_TEXTURES 101
#else
#define CD_FILE_FIGHTER_TEXTURES     108
#define CD_FILE_FIGHTER_ALT_TEXTURES 102
#endif

enum FighterTextureMode {
    FIGHTER_TEXTURE_BASE = 0,
    FIGHTER_TEXTURE_ALT = 1,
    FIGHTER_TEXTURE_MODE_COUNT = 2
};

enum {
    FIGHTER_ALT_TEXTURE_MIN_BUFFER = 0xaf08, /* minimum free bytes in GPU packet buffer for D00..D05 */
    FIGHTER_BASE_TEXTURE_COUNT = 18,         /* character IDs 0..17 map to DATA4\D06..D23 (case 12 skipped) */
    FIGHTER_OVERLAY_CHAR_COUNT = 12          /* character IDs 0..5 playable + 6..11 story alternates */
};

/* Whether each story alternate (characters 6..11) has an alternate texture file (DATA4\D00..D05). */
short gAltTextureEnabled[PLAYABLE_CHARACTER_COUNT] = { 1, 1, 1, 1, 1, 1 };

void LoadFighterAltTextures(int player)
{
    int characterIndex = GetFighterCharacterId(player);
    int injuryMask;

    characterIndex -= PLAYABLE_CHARACTER_COUNT;
    if (characterIndex >= 0 && characterIndex < PLAYABLE_CHARACTER_COUNT) {
        if (GetGpuPacketBufferRemaining() < FIGHTER_ALT_TEXTURE_MIN_BUFFER) {
            TriggerDebugTrap();
        } else {
            if (gAltTextureEnabled[characterIndex] != 0) {
                ReadCdFileSync(characterIndex + CD_FILE_FIGHTER_ALT_TEXTURES, gPacketCursor);
                DrawSync(0);
                if (player == 0) {
                    LoadTimGroup(gPacketCursor, 0);
                } else {
                    LoadTimGroupWithOffset(gPacketCursor, 0, TPAGE_P2_X_OFFSET, 0);
                }
            }
            injuryMask = GetFighterInjuryOverlayMask(player);
            if (injuryMask != 0) {
                ApplyFighterTextureOverlays(player, characterIndex + PLAYABLE_CHARACTER_COUNT,
                                            FIGHTER_TEXTURE_ALT, injuryMask, gPacketCursor);
            }
            DrawSync(0);
        }
    }
}

void LoadFighterBaseTextures(int player, int injuryMask)
{
    int character = GetFighterCharacterId(player);
    void *buffer;

    if (character < 0 || character >= FIGHTER_BASE_TEXTURE_COUNT) {
        return;
    }

    switch (character) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
        buffer = GetGpuPacketBuffer(0);
        ReadCdFileSync(CD_FILE_FIGHTER_TEXTURES + character, buffer);
        if (player == 0) {
            LoadTimGroup(buffer, 0);
        } else {
            LoadTimGroupWithOffset(buffer, 0, TPAGE_P2_X_OFFSET, 0);
        }
        if ((character >= PLAYABLE_CHARACTER_COUNT && character < FIGHTER_OVERLAY_CHAR_COUNT) ||
            character == 2) {
            ApplyFighterTextureOverlays(player, character, FIGHTER_TEXTURE_BASE, injuryMask, buffer);
        }
        break;
    }
}

/* Injury overlay bitmask available per character (0..11) and archive mode (base, alt).
 * Bit 4 (0x10) is the head/torso overlay; bits 3..0 (0x0F) are the four limb overlays. */
unsigned char gFighterOverlayMasks[FIGHTER_OVERLAY_CHAR_COUNT * FIGHTER_TEXTURE_MODE_COUNT] = {
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    0, 0,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
    INJURY_OVERLAY_ALL, INJURY_OVERLAY_LIMBS_MASK,
};

void ApplyFighterTextureOverlays(int player, int character, int mode, int injuryMask, void *buffer)
{
    int available;
    int bit;
    int i;

    if (player >= 0 && player < 2 && character >= 0 && character < FIGHTER_OVERLAY_CHAR_COUNT) {
        available = gFighterOverlayMasks[character * FIGHTER_TEXTURE_MODE_COUNT + mode];
        bit = INJURY_OVERLAY_HEAD_TORSO;
        for (i = 0; i < INJURY_OVERLAY_COUNT; ++i) {
            if ((available & bit) && (injuryMask & bit)) {
                if (player == 0)
                    LoadTimGroup(buffer, i + 1);
                else
                    LoadTimGroupWithOffset(buffer, i + 1, TPAGE_P2_X_OFFSET, 0);
            }
            bit >>= 1;
        }
    }
}
