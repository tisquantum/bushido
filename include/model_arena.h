#ifndef MODEL_ARENA_H
#define MODEL_ARENA_H

/* At the start of a character archive (DATA0\M*.DAT, DATA6\A*.DAT): byte offsets from it. */
typedef struct CharacterArchiveHeader {
    unsigned int sectionCount;
    unsigned int modelOffset;
    unsigned int overrideTableOffset;
    unsigned int shadowModelOffset;
    unsigned int timGroupOffset;      /* once uploaded, the fighter's cloth work buffer */
    unsigned int endOffset;
} CharacterArchiveHeader;

/* The model arena: per-group archive slots for the fighters, their weapons and the stage. */
enum {
    MODEL_ARENA_CANARY = 0x12345678,
    COMMON_EFFECT_TEXTURE_WORDS = 32768,
    MODEL_ARENA_CANARY_INDEX = COMMON_EFFECT_TEXTURE_WORDS - 1,
    ARENA_UNLOADED = -1,
    ARENA_INVALIDATED = 999,
    ARENA_GROUP_COUNT = 5,
    CHAR_SLOT_COUNT = 3,
    GPU_PACKET_BUFFER_SIZE = 0x15800,
    AUX_GPU_PACKET_BUFFER_SHIFT = 15,
    AUX_RESOURCE_BUFFER_SIZE = 0x32800
};

/* Character and weapon archives: their ids, and the meshes and overrides inside them. */
enum {
    STANDARD_WEAPON_CHAR_COUNT = 12,
    SHARED_WEAPON_CHAR_FIRST = 26,
    SHARED_WEAPON_CHAR_END = 32,
    CHARACTER_ARCHIVE_COUNT = 35,
    STANDARD_WEAPON_COUNT = 8,
    WEAPON_ARCHIVE_SHARED_FILE = 12,
    WEAPON_ARCHIVE_FOG_BASE = 13,
    WEAPON_ARCHIVE_FOG_END = 19,
    WEAPON_ARCHIVE_CUTSCENE_FILE = 19,
    WEAPON_SECONDARY_MESH_BASE = 8,
    WEAPON_EXTRA_MESH_BASE = 10,
    FIGHTER_OVERRIDE_END = 0xFFFF,
    WEAPON_CUTSCENE_OVERRIDE_BASE = 16,
    PRACTICE_WEAPON_TIM_GROUP = 8
};

/* GetFighterInjuryOverlayMask's bits. */
enum {
    INJURY_OVERLAY_LEFT_ARM = 0x01,
    INJURY_OVERLAY_RIGHT_ARM = 0x02,
    INJURY_OVERLAY_RIGHT_LEG = 0x04,
    INJURY_OVERLAY_LEFT_LEG = 0x08,
    INJURY_OVERLAY_HEAD_TORSO = 0x10,
    INJURY_OVERLAY_LIMBS_MASK = 0x0F,
    INJURY_OVERLAY_ALL = 0x1F,
    INJURY_OVERLAY_COUNT = 5
};

/* Player two's texture pages and the fog palettes, as offsets in VRAM. */
enum {
    TPAGE_WIDTH_HALFWORDS = 64,
    TPAGE_HEIGHT_LINES = 256,
    TPAGE_P2_PAGE_DX = 3,
    TPAGE_P2_X_OFFSET = TPAGE_P2_PAGE_DX * TPAGE_WIDTH_HALFWORDS,
    FOG_CLUT_PAGE_DX = -10,
    FOG_CLUT_PAGE_DY = 1,
    FOG_CLUT_X_OFFSET = FOG_CLUT_PAGE_DX * TPAGE_WIDTH_HALFWORDS,
    FOG_CLUT_Y_OFFSET = FOG_CLUT_PAGE_DY * TPAGE_HEIGHT_LINES
};

enum ArenaGroup {
    ARENA_GROUP_P1_CHARACTER = 0,
    ARENA_GROUP_P1_WEAPON = 1,
    ARENA_GROUP_P2_CHARACTER = 2,
    ARENA_GROUP_P2_WEAPON = 3,
    ARENA_GROUP_STAGE = 4
};

enum CharacterArchiveSlot {
    CHAR_SLOT_P1 = 0,
    CHAR_SLOT_P2 = 1,
    CHAR_SLOT_SELECT_AUX = 2
};

enum ModelTableSlot {
    MODEL_SLOT_P1_FIGHTER = 4,
    MODEL_SLOT_P2_FIGHTER = 5,
    MODEL_SLOT_P1_SHADOW = 6,
    MODEL_SLOT_P2_SHADOW = 7,
    MODEL_SLOT_P1_WEAPON = 8,
    MODEL_SLOT_P2_WEAPON = 9,
    MODEL_SLOT_STAGE = 10,
    MODEL_SLOT_STAGE_EXTRA = 11
};

enum WeaponArchiveKind {
    WEAPON_ARCHIVE_STANDARD = 0,
    WEAPON_ARCHIVE_SHARED = 1,
    WEAPON_ARCHIVE_CUTSCENE = 2
};

/* Header at the start of a weapon archive (DATA1\H00..H19.DAT): section byte offsets from base. */
typedef struct WeaponArchiveHeader {
    unsigned int sectionCount;
    unsigned int modelOffset;  /* weapon mesh table (compacted via CompactArchiveSubset) */
    unsigned int timOffset;    /* TIM texture group */
    unsigned int endOffset;    /* total archive byte length */
} WeaponArchiveHeader;

int GetArenaUsedBytes(void);
int GetFighterCharacterId(int slot);
const unsigned short *GetFighterOverrideTable(int slot);
void LoadFighterModel(int side, int characterId);
void LoadFighterWeaponModel(int side, int weaponId);
void SetFighterCharacterId(int slot, int characterId);
int GetLoadedStageId(void);
void LoadArenaGroup(int group, int resourceId);
unsigned int *LoadStageArchive(unsigned int *base, int stageId, int modelSlot);
unsigned int *LoadWeaponArchive(unsigned int *base, int side, int weaponId, int modelSlot);
unsigned int *LoadCharacterArchive(unsigned int *base, int slot, int characterId, int modelSlot, int shadowModelSlot,
                                   int *outStatus);
void RelocateArenaGroup(int group, int delta);
void RelocateFighterClothBuffer(int slot, int delta);
void RelocateStageCameraWalls(int delta);
void RelocateStagePropMarkers(int delta);
void UpdateArenaPeakUsage(void *end);
void ResetModelArena(void);
void *GetGpuPacketBuffer(int buffer);
int GetGpuPacketBufferRemaining(void);
void *GetAuxGpuPacketBuffer(int buffer);
void *GetArenaFreePtr(void);

void CallResetModelArena(void);
void LoadStageResource(int stageId);
void LoadCommonEffectTextures(void);
int DidLastArenaLoadReload(void);
void *GetModelArenaBase(void);
void *GetStageCameraWalls(void);
void *GetStagePropMarkers(void);
int GetFighterInjuryOverlayMask(int side);
int GetFighterModelCount(int side);
int GetFighterModelTableSlot(int slot);
int CountFighterOverrideEntries(int side);
int GetPacketBufferSize(void);
void *GetFighterClothBufferBase(int slot);
void InvalidateStageArenaSlot(void);
void InvalidateArenaGroup(int group);
void *GetAuxResourceBuffer(void);
void SetFighterInjuryOverlayMask(int side, int mask);
void SetPracticeWeaponTextureMode(int enabled);
void CheckModelArenaCanary(void);

#endif
