#ifndef SELECT_SCENE_H
#define SELECT_SCENE_H

#include <sys/types.h>
#include <libgte.h>

struct SceneNode;

/* InitCharacterSelectScene's selectMode, kept in gCharSelectMenuType. */
typedef enum SelectGameMode {
    SELECT_MODE_STORY = 0,
    SELECT_MODE_VS = 1,               /* Katze not yet unlocked */
    SELECT_MODE_STORY_CHALLENGER = 2, /* a second player joins Story; P1 keeps the choice */
    SELECT_MODE_POV_VS = 3,
    SELECT_MODE_PRACTICE = 4,
    SELECT_MODE_LINK_VS = 5,
    SELECT_MODE_SLASH = 6,
    SELECT_MODE_VS_KATZE = 7
} SelectGameMode;

typedef enum SelectPlayerMode {
    SELECT_PLAYERS_1P = 0,
    SELECT_PLAYERS_2P = 1,
    SELECT_PLAYERS_CHALLENGER = 2
} SelectPlayerMode;

char *InitCharacterSelectScene(int costumeLayout, int selectMode, char *mainCursor, struct SceneNode *workBuffer);
void ResetCharacterSelectState(int twoPlayer);
#ifdef VERSION_JP
int UpdateCharacterSelectFrame(int p1Move, int p2Move, int p1Confirm, int p2Confirm);
#else
int UpdateCharacterSelectFrame(int p1Move, int p2Move, int p1Confirm, int p2Confirm, int p1Vertical, int p2Vertical);
#endif
struct SceneNode *GetSelectAuxModelParts(void);
MATRIX **GetSelectAuxMatrixTable(void);
void ApplyMenuShowcasePose(int slot, struct SceneNode *parts);
void FinalizeCharacterWeaponSelect(int *p1CharacterId, int *p2CharacterId, int *p1WeaponId, int *p2WeaponId);
int GetSelectedStageIndex(void);
void SetDefaultSelectIndices(int characterId, int weaponId);
unsigned int *LoadSelectAuxModel(unsigned int *cursor);
#ifdef VERSION_JP
int UpdateCharacterSelectMenu(int p1Move, int p2Move, int p1Confirm, int p2Confirm);
#else
int UpdateCharacterSelectMenu(int p1Move, int p2Move, int p1Confirm, int p2Confirm, int p1Vertical, int p2Vertical);
#endif
void UpdateMenuAnimScript(int slot, int twoPlayer);
void EnqueueSelectAuxShadows(void);
int ReloadSelectFighterModels(int reloadP1, int reloadP2);

#endif
