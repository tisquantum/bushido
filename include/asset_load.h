#ifndef ASSET_LOAD_H
#define ASSET_LOAD_H

int ReadCdFileSync(int fileId, void *dest);

void LoadCharacterFile(int resourceId, void *dest);
void LoadWeaponFile(int weaponIndex, void *dest);
void LoadStageFile(int stageIndex, void *dest);
void LoadCommonEffectFile(void *dest);
void PreloadSlashEnemyGroup(int *dest);

#endif
