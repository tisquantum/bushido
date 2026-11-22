/*
 * Disc files by id: the table of the game's data files, a synchronous read of one, and the loaders
 * of a character, weapon, stage or effect file; a character file comes from the resource cache when it holds it.
 */
#include "asset_load.h"
#include "cd.h"
#include "render.h"
#include "resource_cache.h"

/* File ids at which the table's groups start. */
#define CD_FILE_FIRST_WEAPON   35 /* DATA1 H00.DAT */
#define CD_FILE_FIRST_STAGE    55 /* DATA2 S00.DAT */
#define CD_FILE_COMMON_EFFECTS 81 /* DATA3 O00.dat */

/* The disc's data files by file id, as ReadCdFileSync takes it; no routine reads the names. */
char *gCdFileNames[] = {
    /* 0..34: character archives (DATA0 M00..M24, DATA6 A00..A09) */
    "DATA0\\M00.DAT",
    "DATA0\\M01.DAT",
    "DATA0\\M02.DAT",
    "DATA0\\M03.DAT",
    "DATA0\\M04.DAT",
    "DATA0\\M05.DAT",
    "DATA0\\M06.DAT",
    "DATA0\\M07.DAT",
    "DATA0\\M08.DAT",
    "DATA0\\M09.DAT",
    "DATA0\\M10.DAT",
    "DATA0\\M11.DAT",
    "DATA0\\M12.DAT",
    "DATA0\\M13.DAT",
    "DATA0\\M14.DAT",
    "DATA0\\M15.DAT",
    "DATA0\\M16.DAT",
    "DATA0\\M17.DAT",
    "DATA0\\M18.DAT",
    "DATA6\\A00.DAT",
    "DATA6\\A01.DAT",
    "DATA6\\A02.DAT",
    "DATA6\\A03.DAT",
    "DATA6\\A00.DAT",
    "DATA6\\A00.DAT",
    "DATA6\\A00.DAT",
    "DATA0\\M19.DAT",
    "DATA0\\M20.DAT",
    "DATA0\\M21.DAT",
    "DATA0\\M22.DAT",
    "DATA0\\M23.DAT",
    "DATA0\\M24.DAT",
    "DATA6\\A07.DAT",
    "DATA6\\A08.DAT",
    "DATA6\\A09.DAT",
    /* 35..54: weapon archives */
    "DATA1\\H00.DAT",
    "DATA1\\H01.DAT",
    "DATA1\\H02.DAT",
    "DATA1\\H03.DAT",
    "DATA1\\H04.DAT",
    "DATA1\\H05.DAT",
    "DATA1\\H06.DAT",
    "DATA1\\H07.DAT",
    "DATA1\\H08.DAT",
    "DATA1\\H09.DAT",
    "DATA1\\H10.DAT",
    "DATA1\\H11.DAT",
    "DATA1\\H12.DAT",
    "DATA1\\H13.DAT",
    "DATA1\\H14.DAT",
    "DATA1\\H15.DAT",
    "DATA1\\H16.DAT",
    "DATA1\\H17.DAT",
    "DATA1\\H18.DAT",
    "DATA1\\H19.DAT",
    /* 55..80: stage archives */
    "DATA2\\S00.DAT",
    "DATA2\\S01.DAT",
    "DATA2\\S02.DAT",
    "DATA2\\S03.DAT",
    "DATA2\\S04.DAT",
    "DATA2\\S05.DAT",
    "DATA2\\S06.DAT",
    "DATA2\\S07.DAT",
    "DATA2\\S08.DAT",
    "DATA2\\S09.DAT",
    "DATA2\\S10.DAT",
    "DATA2\\S11.DAT",
    "DATA2\\S12.DAT",
    "DATA2\\S13.DAT",
    "DATA2\\S14.DAT",
    "DATA2\\S15.DAT",
    "DATA2\\S16.DAT",
    "DATA2\\S17.DAT",
    "DATA2\\S18.DAT",
    "DATA2\\S19.DAT",
    "DATA2\\S20.DAT",
    "DATA2\\S21.DAT",
    "DATA2\\S22.DAT",
    "DATA2\\S23.DAT",
    "DATA2\\S24.DAT",
    "DATA2\\S25.DAT",
    /* 81..101 (81..100 JP): common effects, UI, overlays, visibility masks, subtitles */
    "DATA3\\O00.dat",
    "DATA3\\O01.dat",
    "DATA3\\O02.dat",
    "DATA3\\O03.dat",
    "DATA3\\O04.dat",
    "DATA3\\O05.dat",
    "DATA3\\O06.dat",
    "DATA3\\O07.dat",
    "DATA3\\O08.dat",
    "DATA3\\O09.dat",
    "DATA3\\O10.dat",
    "DATA3\\O11.dat",
    "DATA3\\O12.dat",
    "DATA3\\O13.dat",
    "DATA3\\O14.dat",
    "DATA3\\O15.dat",
    "DATA3\\O16.dat",
    "DATA3\\O17.dat",
    "DATA3\\O18.dat",
    "DATA3\\O19.dat",
#ifndef VERSION_JP
    "DATA3\\O20.dat",
#endif
    /* 102..125 (101..124 JP): fighter textures and injury overlays */
    "DATA4\\D00.DAT",
    "DATA4\\D01.DAT",
    "DATA4\\D02.DAT",
    "DATA4\\D03.DAT",
    "DATA4\\D04.DAT",
    "DATA4\\D05.DAT",
    "DATA4\\D06.DAT",
    "DATA4\\D07.DAT",
    "DATA4\\D08.DAT",
    "DATA4\\D09.DAT",
    "DATA4\\D10.DAT",
    "DATA4\\D11.DAT",
    "DATA4\\D12.DAT",
    "DATA4\\D13.DAT",
    "DATA4\\D14.DAT",
    "DATA4\\D15.DAT",
    "DATA4\\D16.DAT",
    "DATA4\\D17.DAT",
    "DATA4\\D18.DAT",
    "DATA4\\D19.DAT",
    "DATA4\\D20.DAT",
    "DATA4\\D21.DAT",
    "DATA4\\D22.DAT",
    "DATA4\\D23.DAT",
    /* 126..127 (125..126 JP): POV orientation mannequin */
    "DATA5\\T0.DAT",
    "DATA5\\T1.DAT",
    /* 128..136 (127..135 JP): Story 3D cutscenes */
    "DATA7\\V00.DAT",
    "DATA7\\V01.DAT",
    "DATA7\\V02.DAT",
    "DATA7\\V03.DAT",
    "DATA7\\V04.DAT",
    "DATA7\\V05.DAT",
    "DATA7\\V06.DAT",
    "DATA7\\V07.DAT",
    "DATA7\\V08.DAT",
};

int ReadCdFileSync(int fileId, void *dest)
{
    WaitCdIdle();
    StartCdFileRead(fileId, dest);
    return WaitCdIdle();
}

void LoadCharacterFile(int resourceId, void *dest)
{
    const void *source;
    unsigned int byteCount;

    if (FindCachedResourceIndex(resourceId) >= 0) {
        source = GetCachedResourceBase(resourceId);
        byteCount = GetCachedResourceSize(resourceId);
        CopyWordsFast(source, dest, byteCount);
    } else {
        ReadCdFileSync(resourceId, dest);
    }
}

void LoadWeaponFile(int weaponIndex, void *dest)
{
    ReadCdFileSync(CD_FILE_FIRST_WEAPON + weaponIndex, dest);
}

void LoadStageFile(int stageIndex, void *dest)
{
    ReadCdFileSync(CD_FILE_FIRST_STAGE + stageIndex, dest);
}

void LoadCommonEffectFile(void *dest)
{
    ReadCdFileSync(CD_FILE_COMMON_EFFECTS, dest);
}

void PreloadSlashEnemyGroup(int *dest)
{
    ClearCachedResources();
    LoadResourceGroup(RESOURCE_GROUP_SLASH_ENEMIES, dest);
}
