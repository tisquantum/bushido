/* The compacting model arena: five groups (each fighter's character and weapon, the stage) in
 * gModelArena, moved down when one is reloaded; with the GPU packet buffers and their bookkeeping. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "asset_load.h"
#include "cuttable_prop.h"
#include "fighter_cloth.h"
#include "fighter_texture.h"
#include "floor_collision.h"
#include "fog.h"
#include "fighter.h"
#include "model_arena.h"
#include "model_table.h"
#include "packet_build.h"
#include "stage_cull.h"
#include "stage_id.h"
#include "stage_moat.h"
#include "globals.h"
#include "render.h"
#include "tim.h"
#include "tpage_reloc.h"

void CallResetModelArena(void)
{
    ResetModelArena();
}

static int gArenaLastLoadReloaded;
static int gLoadedStageId;
static unsigned int *gLoadedStageCameraWalls;
static unsigned int *gLoadedStagePropMarkers;
static short gLoadedWeaponCharacterIds[2];
static int gFighterInjuryOverlayMasks[2];
static int gPracticeWeaponTextures;
static int gArenaUsedBytes; /* written, never read */
static int gArenaPeakBytes;
static int gArenaFirstLoadPending;
static int gArenaSlotCount;

static int *gFighterOverrideTables[CHAR_SLOT_COUNT];
static int gSlotCharacterIds[CHAR_SLOT_COUNT];
static unsigned int *gFighterClothBufferBases[CHAR_SLOT_COUNT];
static int gFighterClothBufferSizes[CHAR_SLOT_COUNT];
static int gArenaGroupAllocSlots[ARENA_GROUP_COUNT];
static unsigned int *gArenaSlotBases[ARENA_GROUP_COUNT];
static unsigned int gArenaSlotSizes[ARENA_GROUP_COUNT];
static int gArenaGroupResourceIds[ARENA_GROUP_COUNT];
static unsigned int gModelArena[41984];
static char gAuxGpuPacketBuffers[65536];
/* One region used three ways: the aux resource buffer, the two GPU packet buffers and
 * the effect textures' load area. */
static union AuxRegion {
    char resourceBuffer[AUX_RESOURCE_BUFFER_SIZE];
    struct {
        char head[AUX_RESOURCE_BUFFER_SIZE - 2 * GPU_PACKET_BUFFER_SIZE];
        unsigned char buffers[2 * GPU_PACKET_BUFFER_SIZE];
    } packets;
    struct {
        char head[AUX_RESOURCE_BUFFER_SIZE - COMMON_EFFECT_TEXTURE_WORDS * 4];
        unsigned int words[COMMON_EFFECT_TEXTURE_WORDS]; /* last word holds MODEL_ARENA_CANARY */
    } effectTextures;
} gAuxRegion;
/* 1,336 bytes; nothing reads or writes them. */
static unsigned char gModelArenaUnused[1336];

void ResetModelArena(void)
{
    int i;

    for (i = 0; i < ARENA_GROUP_COUNT; i++) {
        gArenaGroupResourceIds[i] = gArenaGroupAllocSlots[i] = ARENA_UNLOADED;
    }

    gArenaFirstLoadPending = 1;
    gArenaSlotCount = 0;
    gSlotCharacterIds[CHAR_SLOT_P1] = ARENA_UNLOADED;
    gSlotCharacterIds[CHAR_SLOT_P2] = ARENA_UNLOADED;
    gLoadedWeaponCharacterIds[1] = ARENA_INVALIDATED;
    gLoadedWeaponCharacterIds[0] = ARENA_INVALIDATED;
    gAuxRegion.effectTextures.words[MODEL_ARENA_CANARY_INDEX] = MODEL_ARENA_CANARY;
    gArenaPeakBytes = 0;
    gLoadedStageId = ARENA_UNLOADED;
    gFighterInjuryOverlayMasks[0] = 0;
    gFighterInjuryOverlayMasks[1] = 0;
    gPracticeWeaponTextures = 0;
}

void LoadFighterModel(int side, int characterId)
{
    FreeFighterClothSlot(side);
    if (side == CHAR_SLOT_P1)
        LoadArenaGroup(ARENA_GROUP_P1_CHARACTER, characterId);
    else
        LoadArenaGroup(ARENA_GROUP_P2_CHARACTER, characterId);
}

void LoadFighterWeaponModel(int side, int weaponId)
{
    int group = ARENA_GROUP_P1_WEAPON;
    int characterId;

    if (side != CHAR_SLOT_P1) {
        group = ARENA_GROUP_P2_WEAPON;
    }
    characterId = GetFighterCharacterId(side);
    if (weaponId != gArenaGroupResourceIds[group] || characterId != gLoadedWeaponCharacterIds[side]) {
        if (gArenaGroupResourceIds[group] != ARENA_UNLOADED) {
            gArenaGroupResourceIds[group] = ARENA_INVALIDATED;
        }
        LoadArenaGroup(group, weaponId);
        gLoadedWeaponCharacterIds[side] = characterId;
    }
}

void LoadStageResource(int stageId)
{
    LoadArenaGroup(ARENA_GROUP_STAGE, stageId);
}

void LoadArenaGroup(int group, int resourceId)
{
    int groupIndex;
    int allocSlot;
    int existingSlot;
    int i;
    int byteSize;
    unsigned int *cursor;
    int loadStatus;

    DrawSync(0);
    if (gAuxRegion.effectTextures.words[MODEL_ARENA_CANARY_INDEX] != MODEL_ARENA_CANARY) {
        TriggerDebugTrap();
    }
    groupIndex = group;
    gArenaLastLoadReloaded = 0;
    if (resourceId == gArenaGroupResourceIds[groupIndex]) {
        return;
    }
    gArenaLastLoadReloaded = 1;
    if (gArenaFirstLoadPending != 0) {
        cursor = gModelArena;
        gArenaFirstLoadPending = 0;
        allocSlot = gArenaSlotCount++;
    } else if (gArenaGroupResourceIds[groupIndex] == ARENA_UNLOADED) {
        allocSlot = gArenaSlotCount++;
        cursor = gArenaSlotBases[allocSlot - 1] + gArenaSlotSizes[allocSlot - 1] / 4;
    } else {
        existingSlot = gArenaGroupAllocSlots[groupIndex];
        if (existingSlot == gArenaSlotCount - 1 || gArenaSlotCount == 1) {
            cursor = gArenaSlotBases[existingSlot];
            allocSlot = existingSlot;
        } else {
            byteSize = 0;
            for (i = existingSlot + 1; i < gArenaSlotCount; i++) {
                byteSize += gArenaSlotSizes[i];
            }
            CopyWordsFast(gArenaSlotBases[existingSlot + 1], gArenaSlotBases[existingSlot], byteSize);
            byteSize = gArenaSlotSizes[existingSlot];
            for (i = existingSlot + 1; i < gArenaSlotCount; i++) {
                gArenaSlotBases[i - 1] = gArenaSlotBases[i] - byteSize / 4;
                gArenaSlotSizes[i - 1] = gArenaSlotSizes[i];
            }
            for (i = 0; i < ARENA_GROUP_COUNT; i++) {
                if (gArenaGroupAllocSlots[i] != ARENA_UNLOADED && existingSlot < gArenaGroupAllocSlots[i]) {
                    gArenaGroupAllocSlots[i]--;
                    RelocateArenaGroup(i, -byteSize);
                }
            }
            allocSlot = gArenaSlotCount - 1;
            cursor = gArenaSlotBases[gArenaSlotCount - 2] + gArenaSlotSizes[gArenaSlotCount - 2] / 4;
        }
    }
    gArenaGroupResourceIds[groupIndex] = resourceId;
    gArenaSlotBases[allocSlot] = cursor;
    gArenaGroupAllocSlots[groupIndex] = allocSlot;
    switch (groupIndex) {
    case ARENA_GROUP_P1_CHARACTER:
        cursor = LoadCharacterArchive(cursor, CHAR_SLOT_P1, resourceId, MODEL_SLOT_P1_FIGHTER, MODEL_SLOT_P1_SHADOW,
                                      &loadStatus);
        break;
    case ARENA_GROUP_P1_WEAPON:
        cursor = LoadWeaponArchive(cursor, CHAR_SLOT_P1, resourceId, MODEL_SLOT_P1_WEAPON);
        break;
    case ARENA_GROUP_P2_CHARACTER:
        cursor = LoadCharacterArchive(cursor, CHAR_SLOT_P2, resourceId, MODEL_SLOT_P2_FIGHTER, MODEL_SLOT_P2_SHADOW,
                                      &loadStatus);
        break;
    case ARENA_GROUP_P2_WEAPON:
        cursor = LoadWeaponArchive(cursor, CHAR_SLOT_P2, resourceId, MODEL_SLOT_P2_WEAPON);
        break;
    case ARENA_GROUP_STAGE:
        cursor = LoadStageArchive(cursor, resourceId, MODEL_SLOT_STAGE);
        break;
    }
    gArenaSlotSizes[allocSlot] = (char *)cursor - (char *)gArenaSlotBases[allocSlot];
    gArenaUsedBytes = GetArenaUsedBytes();
    ((char *)cursor)[0] = 'M';
    ((char *)cursor)[1] = 'd';
    ((char *)cursor)[2] = 'l';
    ((char *)cursor)[3] = 'T';
    ((char *)cursor)[4] = 'a';
    ((char *)cursor)[5] = 'b';
    ((char *)cursor)[6] = 'l';
    ((char *)cursor)[7] = 'e';
    if (gAuxRegion.effectTextures.words[MODEL_ARENA_CANARY_INDEX] != MODEL_ARENA_CANARY) {
        TriggerDebugTrap();
    }
}

unsigned int *LoadCharacterArchive(unsigned int *base, int slot, int characterId, int modelSlot, int shadowModelSlot,
                                   int *outStatus)
{
    CharacterArchiveHeader *header;
    unsigned int *timGroup;
    int *archiveEnd;
    int *modelTable;
    int *overrideTable;
    int *shadowModelTable;
    int byteSize;
    int clothWorkWords;

    LoadCharacterFile(characterId, base);
    *outStatus = 7;
    header = (CharacterArchiveHeader *)base;
    archiveEnd = (int *)(base + header->endOffset / 4);
    byteSize = (int)header->modelOffset;
    modelTable = (int *)(base + (unsigned int)byteSize / 4);
    overrideTable = (int *)(base + header->overrideTableOffset / 4);
    shadowModelTable = (int *)(base + header->shadowModelOffset / 4);
    base = base + header->timGroupOffset / 4;

    UpdateArenaPeakUsage(archiveEnd);
    timGroup = base;

    RegisterModelTable(modelTable, modelSlot);
    RegisterModelTable(shadowModelTable, shadowModelSlot);

    switch (slot) {
    case CHAR_SLOT_P1:
        LoadTimGroup((unsigned short *)timGroup, 0); /* uploaded again below, either way */
        if ((unsigned int)characterId < PLAYABLE_CHARACTER_COUNT && IsCharFogSlotActive(0)) {
            OffsetModelSlotTPageClut(modelSlot, 0, 0, FOG_CLUT_PAGE_DX, FOG_CLUT_PAGE_DY);
            LoadTimGroupWithSeparateOffsets(timGroup, 0, 0, 0, FOG_CLUT_X_OFFSET, FOG_CLUT_Y_OFFSET);
        } else {
            LoadTimGroup((unsigned short *)timGroup, 0);
        }
        break;
    case CHAR_SLOT_P2:
        if ((unsigned int)characterId < PLAYABLE_CHARACTER_COUNT && IsCharFogSlotActive(1)) {
            OffsetModelSlotTPageClut(modelSlot, TPAGE_P2_PAGE_DX, 0, FOG_CLUT_PAGE_DX, FOG_CLUT_PAGE_DY);
            LoadTimGroupWithSeparateOffsets(timGroup, 0, TPAGE_P2_X_OFFSET, 0, FOG_CLUT_X_OFFSET, FOG_CLUT_Y_OFFSET);
        } else {
            OffsetModelSlotTPageClut(modelSlot, TPAGE_P2_PAGE_DX, 0, TPAGE_P2_PAGE_DX, 0);
            LoadTimGroupWithOffset(timGroup, 0, TPAGE_P2_X_OFFSET, 0);
        }
        break;
    case CHAR_SLOT_SELECT_AUX:
        OffsetModelSlotTPageClut(modelSlot, 0, 1, 0, 1);
        LoadTimGroupWithOffset(timGroup, 0, 0, TPAGE_HEIGHT_LINES);
        break;
    }
    gFighterOverrideTables[slot] = overrideTable;
    gSlotCharacterIds[slot] = characterId;
    clothWorkWords = CalcFighterClothWorkSize(slot);
    byteSize = clothWorkWords;
    clothWorkWords >>= 2;
    gFighterClothBufferBases[slot] = timGroup;
    gFighterClothBufferSizes[slot] = byteSize;
    /* kept: byteSize / 4 written out; as / 4, the size and the quotient share one register; cause not established */
    if (byteSize < 0) {
        clothWorkWords = (byteSize + 3) >> 2;
    }
    timGroup += clothWorkWords;
    UpdateArenaPeakUsage(timGroup);
    DrawSync(0);
    return timGroup;
}

/* By weapon id 0 to 7, the hand weapons of characters 0 to 11; the boss weapons 8 and 9 load no weapon archive. */
short gWeaponSecondaryMeshOffset[STANDARD_WEAPON_COUNT] = { 0, 1, 0, 1, 1, 1, 0, 0 };

/* Weapon archives (DATA1): H00..H11 by character, H13..H18 for fogged characters 0..5, H12 shared
 * by characters 26..31, H19 the cutscene weapons 16..19 of characters 6..11 (four meshes each). */
unsigned int *LoadWeaponArchive(unsigned int *base, int side, int weaponId, int modelSlot)
{
    int fileId;
    int characterId;
    int kind;
    unsigned int *modelTable;
    unsigned int *timGroup;
    unsigned int *archiveEnd;

    fileId = GetFighterCharacterId(side);
    characterId = fileId;
    if (fileId >= STANDARD_WEAPON_CHAR_COUNT &&
        (fileId < SHARED_WEAPON_CHAR_FIRST || fileId >= SHARED_WEAPON_CHAR_END))
        TriggerDebugTrap();
    kind = (fileId >= STANDARD_WEAPON_CHAR_COUNT) ? WEAPON_ARCHIVE_SHARED : WEAPON_ARCHIVE_STANDARD;
    if (weaponId >= WEAPON_CUTSCENE_OVERRIDE_BASE)
        kind = WEAPON_ARCHIVE_CUTSCENE;
    switch (kind) {
    case WEAPON_ARCHIVE_STANDARD:
        if (fileId >= 0 && fileId < PLAYABLE_CHARACTER_COUNT && IsCharFogSlotActive(side) != 0)
            fileId += WEAPON_ARCHIVE_FOG_BASE;
        break;
    case WEAPON_ARCHIVE_SHARED:
        fileId = WEAPON_ARCHIVE_SHARED_FILE;
        break;
    case WEAPON_ARCHIVE_CUTSCENE:
        if (fileId < PLAYABLE_CHARACTER_COUNT || fileId >= STANDARD_WEAPON_CHAR_COUNT)
            TriggerDebugTrap();
        fileId = WEAPON_ARCHIVE_CUTSCENE_FILE;
        break;
    }
    LoadWeaponFile(fileId, base);
    modelTable = base + ((WeaponArchiveHeader *)base)->modelOffset / 4;
    timGroup = base + ((WeaponArchiveHeader *)base)->timOffset / 4;
    archiveEnd = base + ((WeaponArchiveHeader *)base)->endOffset / 4;
    switch (kind) {
    case WEAPON_ARCHIVE_STANDARD:
        base = CompactArchiveSubset(modelTable, 3, weaponId,
                                    gWeaponSecondaryMeshOffset[weaponId] + WEAPON_SECONDARY_MESH_BASE,
                                    weaponId + WEAPON_EXTRA_MESH_BASE);
        break;
    case WEAPON_ARCHIVE_SHARED:
        base = CompactArchiveSubset(modelTable, 1, weaponId);
        break;
    case WEAPON_ARCHIVE_CUTSCENE:
        characterId = (characterId - PLAYABLE_CHARACTER_COUNT) * 4;
        switch (weaponId) {
        case WEAPON_CUTSCENE_OVERRIDE_BASE + 0:
            base = CompactArchiveSubset(modelTable, 2, characterId, characterId + 2);
            break;
        case WEAPON_CUTSCENE_OVERRIDE_BASE + 1:
            base = CompactArchiveSubset(modelTable, 2, characterId, characterId + 3);
            break;
        case WEAPON_CUTSCENE_OVERRIDE_BASE + 2:
            base = CompactArchiveSubset(modelTable, 2, characterId + 1, characterId + 2);
            break;
        case WEAPON_CUTSCENE_OVERRIDE_BASE + 3:
            base = CompactArchiveSubset(modelTable, 2, characterId + 1, characterId + 3);
            break;
        }
        break;
    }
    RegisterModelTable(modelTable, modelSlot);
    if (side == CHAR_SLOT_P1) {
        if (fileId >= WEAPON_ARCHIVE_FOG_BASE && fileId < WEAPON_ARCHIVE_FOG_END) {
            OffsetModelSlotTPageClut(modelSlot, 0, 0, FOG_CLUT_PAGE_DX, FOG_CLUT_PAGE_DY);
            LoadTimGroupWithSeparateOffsets(timGroup, weaponId, 0, 0, FOG_CLUT_X_OFFSET, FOG_CLUT_Y_OFFSET);
        } else if (gPracticeWeaponTextures == 0) {
            LoadTimGroup((unsigned short *)timGroup, weaponId);
        } else {
            LoadTimGroup((unsigned short *)timGroup, PRACTICE_WEAPON_TIM_GROUP);
            LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, 0, -1);
            LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, 0, -2);
            LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, 0, -3);
            LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, 0, 2);
        }
    } else {
        if (fileId >= WEAPON_ARCHIVE_FOG_BASE && fileId < WEAPON_ARCHIVE_FOG_END) {
            OffsetModelSlotTPageClut(modelSlot, TPAGE_P2_PAGE_DX, 0, FOG_CLUT_PAGE_DX, FOG_CLUT_PAGE_DY);
            LoadTimGroupWithSeparateOffsets(timGroup, weaponId, TPAGE_P2_X_OFFSET, 0, FOG_CLUT_X_OFFSET,
                                            FOG_CLUT_Y_OFFSET);
        } else {
            OffsetModelSlotTPageClut(modelSlot, TPAGE_P2_PAGE_DX, 0, TPAGE_P2_PAGE_DX, 0);
            if (gPracticeWeaponTextures == 0) {
                LoadTimGroupWithOffset(timGroup, weaponId, TPAGE_P2_X_OFFSET, 0);
            } else {
                LoadTimGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, TPAGE_P2_X_OFFSET, 0);
                LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, TPAGE_P2_X_OFFSET, -1);
                LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, TPAGE_P2_X_OFFSET, -2);
                LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, TPAGE_P2_X_OFFSET, -3);
                LoadTimClutGroupWithOffset(timGroup, PRACTICE_WEAPON_TIM_GROUP, TPAGE_P2_X_OFFSET, 2);
            }
        }
    }
    DrawSync(0);
    UpdateArenaPeakUsage(archiveEnd);
    return base;
}

/*
 * Stage archive offset table (DATA2\S00..S25.DAT):
 *   base[0]: sectionCount, base[1]: stage models, base[2]: floor collision,
 *   base[3]: camera walls, base[4]: prop markers, base[5]: optional stage-extra models,
 *   base[sectionCount - 1]: scene layout (freed after ParseStageSceneLayout),
 *   base[sectionCount]: TIM texture group (freed after VRAM upload), base[sectionCount + 1]: end.
 */
unsigned int *LoadStageArchive(unsigned int *base, int stageId, int modelSlot)
{
    unsigned int *modelTable;
    short *floorCollision;
    unsigned int *cameraWalls;
    unsigned int *propMarkers;
    unsigned int *sceneLayout;
    unsigned int *timGroup;
    unsigned int *archiveEnd;
    int sectionCount;

    gLoadedStageId = stageId;
    LoadStageFile(stageId, base);

    modelTable = base + (base[1] >> 2);
    floorCollision = (short *)(base + (base[2] >> 2));
    cameraWalls = base + (base[3] >> 2);
    propMarkers = base + (base[4] >> 2);
    sectionCount = base[0];
    sceneLayout = base + (base[sectionCount - 1] >> 2);
    timGroup = base + (base[sectionCount] >> 2);
    archiveEnd = base + (base[sectionCount + 1] >> 2);

    switch (stageId) {
    case 3:
    case 14:
        RegisterModelTable(modelTable, modelSlot);
        LoadTimGroup((unsigned short *)timGroup, 0);
        ParseStageSceneLayout((u_short *)sceneLayout, modelSlot);
        SetFloorCollisionData(floorCollision);
        InitCuttableProps(base + (base[5] >> 2), modelSlot + 1, propMarkers);
        break;
    case 9:
    case 11:
        RegisterModelTable(modelTable, modelSlot);
        LoadTimGroup((unsigned short *)timGroup, 0);
        ParseStageSceneLayout((u_short *)sceneLayout, modelSlot);
        SetFloorCollisionData(floorCollision);
        RegisterModelTable(base + (base[5] >> 2), modelSlot + 1);
        break;
    /* both arms are the same in the original */
    case 12:
    case 17:
        RegisterModelTable(modelTable, modelSlot);
        LoadTimGroup((unsigned short *)timGroup, 0);
        ParseStageSceneLayout((u_short *)sceneLayout, modelSlot);
        SetFloorCollisionData(floorCollision);
        RegisterModelTable(base + (base[5] >> 2), modelSlot + 1);
        break;
    case 19:
        RegisterModelTable(base + (base[5] >> 2), modelSlot + 1);
        InitMoatBillboards(modelSlot + 1);
    default:
        RegisterModelTable(modelTable, modelSlot);
        LoadTimGroup((unsigned short *)timGroup, 0);
        ParseStageSceneLayout((u_short *)sceneLayout, modelSlot);
        SetFloorCollisionData(floorCollision);
        break;
    }

    gLoadedStagePropMarkers = propMarkers;
    gLoadedStageCameraWalls = cameraWalls;
    UpdateArenaPeakUsage(archiveEnd);
    DrawSync(0);
    return sceneLayout;
}

void UpdateArenaPeakUsage(void *end)
{
    int used;
    unsigned int peak;

    used = (char *)end - (char *)gModelArena;
    peak = gArenaPeakBytes;
    if (peak < used) {
        gArenaPeakBytes = used;
    }
}

void RelocateArenaGroup(int group, int delta)
{
    switch (group) {
    case ARENA_GROUP_P1_CHARACTER:
        RelocateModelTable(MODEL_SLOT_P1_FIGHTER, delta);
        RelocateModelTable(MODEL_SLOT_P1_SHADOW, delta);
        gFighterOverrideTables[CHAR_SLOT_P1] += delta / 4;
        RelocateFighterClothSlot(CHAR_SLOT_P1, delta);
        RelocateFighterClothBuffer(CHAR_SLOT_P1, delta);
        break;
    case ARENA_GROUP_P2_CHARACTER:
        RelocateModelTable(MODEL_SLOT_P2_FIGHTER, delta);
        RelocateModelTable(MODEL_SLOT_P2_SHADOW, delta);
        gFighterOverrideTables[CHAR_SLOT_P2] += delta / 4;
        RelocateFighterClothSlot(CHAR_SLOT_P2, delta);
        RelocateFighterClothBuffer(CHAR_SLOT_P2, delta);
        break;
    case ARENA_GROUP_P1_WEAPON:
        RelocateModelTable(MODEL_SLOT_P1_WEAPON, delta);
        break;
    case ARENA_GROUP_P2_WEAPON:
        RelocateModelTable(MODEL_SLOT_P2_WEAPON, delta);
        break;
    case ARENA_GROUP_STAGE:
        switch (GetLoadedStageId()) {
        case 3:
        case 14:
            RelocateCuttableProps(delta);
            break;
        case 9:
        case 11:
        case 12:
        case 17:
        case 19:
            RelocateModelTable(MODEL_SLOT_STAGE_EXTRA, delta);
            break;
        }
        RelocateModelTable(MODEL_SLOT_STAGE, delta);
        RelocateFloorCollisionData(delta);
        RelocateStageCameraWalls(delta);
        RelocateStagePropMarkers(delta);
        break;
    }
}

void LoadCommonEffectTextures(void)
{
    unsigned int *buffer;

    buffer = gAuxRegion.effectTextures.words;
    LoadCommonEffectFile(buffer);
    LoadTimGroup((unsigned short *)buffer, 0);
}

/* Nothing calls it. */
static void ModelArenaNoOp(void)
{
}

int DidLastArenaLoadReload(void)
{
    return gArenaLastLoadReloaded;
}

int GetArenaUsedBytes(void)
{
    int last;

    last = gArenaSlotCount - 1;
    return (char *)gArenaSlotBases[last] + gArenaSlotSizes[last] - (char *)gModelArena;
}

void *GetArenaFreePtr(void)
{
    int count;
    char *freePtr;
    int last;

    count = gArenaSlotCount;
    if (count == 0) {
        freePtr = (char *)gModelArena;
    } else {
        last = count - 1;
        freePtr = (char *)gArenaSlotBases[last] + gArenaSlotSizes[last];
    }
    return freePtr;
}

int GetFighterCharacterId(int slot)
{
    return gSlotCharacterIds[slot];
}

void SetFighterCharacterId(int slot, int characterId)
{
    gSlotCharacterIds[slot] = characterId;
}

const unsigned short *GetFighterOverrideTable(int slot)
{
    return (const unsigned short *)gFighterOverrideTables[slot];
}

int GetLoadedStageId(void)
{
    return gLoadedStageId;
}

/* Nothing calls it. */
static int GetCharacterArchiveCount(void)
{
    return CHARACTER_ARCHIVE_COUNT;
}

/* Nothing calls it. */
static int GetStageArchiveCount(void)
{
    return STAGE_COUNT;
}

/* Nothing calls it. */
static int GetStandardWeaponCount(void)
{
    return STANDARD_WEAPON_COUNT;
}

void *GetModelArenaBase(void)
{
    return gModelArena;
}

void *GetStageCameraWalls(void)
{
    return gLoadedStageCameraWalls;
}

void *GetStagePropMarkers(void)
{
    return gLoadedStagePropMarkers;
}

int GetFighterInjuryOverlayMask(int side)
{
    return gFighterInjuryOverlayMasks[side];
}

int GetFighterModelCount(int side)
{
    return gModelTableCounts[MODEL_SLOT_P1_FIGHTER + side];
}

int GetFighterModelTableSlot(int slot)
{
    return slot + MODEL_SLOT_P1_FIGHTER;
}

int CountFighterOverrideEntries(int side)
{
    const unsigned short *entry;
    int count;

    /* Each entry is a (meshIndex, boneIndex) pair terminated by FIGHTER_OVERRIDE_END. */
    entry = GetFighterOverrideTable(side);
    count = 0;
    if (*entry != FIGHTER_OVERRIDE_END) {
        do {
            entry += 2;
            ++count;
        } while (*entry != FIGHTER_OVERRIDE_END);
    }
    return count;
}

int GetPacketBufferSize(void)
{
    return GPU_PACKET_BUFFER_SIZE;
}

void *GetFighterClothBufferBase(int slot)
{
    return gFighterClothBufferBases[slot];
}

void RelocateFighterClothBuffer(int slot, int delta)
{
    gFighterClothBufferBases[slot] += delta / 4;
}

void RelocateStageCameraWalls(int delta)
{
    gLoadedStageCameraWalls += delta / 4;
}

void RelocateStagePropMarkers(int delta)
{
    gLoadedStagePropMarkers += delta / 4;
}

void InvalidateStageArenaSlot(void)
{
    if (gArenaGroupResourceIds[ARENA_GROUP_STAGE] != ARENA_UNLOADED) {
        gArenaGroupResourceIds[ARENA_GROUP_STAGE] = ARENA_INVALIDATED;
    }
}

void InvalidateArenaGroup(int group)
{
    if (gArenaGroupResourceIds[group] != ARENA_UNLOADED) {
        gArenaGroupResourceIds[group] = ARENA_INVALIDATED;
    }
}

void *GetGpuPacketBuffer(int buffer)
{
    unsigned char *base = gAuxRegion.packets.buffers;

    base += buffer * GPU_PACKET_BUFFER_SIZE;
    return base;
}

int GetGpuPacketBufferRemaining(void)
{
    unsigned int bufferStart = (unsigned int)GetGpuPacketBuffer(1);
    unsigned int cursor = (unsigned int)gPacketCursor;
    unsigned int shiftedCursor;

    if (cursor < bufferStart) {
        bufferStart -= GPU_PACKET_BUFFER_SIZE;
    }
    shiftedCursor = cursor - GPU_PACKET_BUFFER_SIZE;
    return bufferStart - shiftedCursor;
}

void *GetAuxGpuPacketBuffer(int buffer)
{
    unsigned int base = (unsigned int)gAuxGpuPacketBuffers;

    return (void *)(base + ((unsigned int)buffer << AUX_GPU_PACKET_BUFFER_SHIFT));
}

void *GetAuxResourceBuffer(void)
{
    return gAuxRegion.resourceBuffer;
}

/* Nothing calls it. */
static int GetAuxResourceBufferSize(void)
{
    return AUX_RESOURCE_BUFFER_SIZE;
}

void SetFighterInjuryOverlayMask(int side, int mask)
{
    gFighterInjuryOverlayMasks[side] = mask & INJURY_OVERLAY_ALL;
    LoadFighterBaseTextures(side, mask);
}

void SetPracticeWeaponTextureMode(int enabled)
{
    if (enabled != 0) {
        gPracticeWeaponTextures = 1;
    } else {
        gPracticeWeaponTextures = 0;
    }
}

void CheckModelArenaCanary(void)
{
    if (gAuxRegion.effectTextures.words[MODEL_ARENA_CANARY_INDEX] != MODEL_ARENA_CANARY) {
        TriggerDebugTrap();
    }
}
