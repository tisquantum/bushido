#ifndef MODEL_TABLE_H
#define MODEL_TABLE_H

#include <sys/types.h>
#include <libgte.h>

/* Registered model tables: each slot points at an archive's mesh offset table (made absolute when
 * registered) and keeps how many meshes it holds. */
#define MODEL_TABLE_COUNT 20

/* A table's first word: its entry count, with the top bit set once its offsets are made absolute. */
#define MODEL_TABLE_RELOCATED  0x8000
#define MODEL_TABLE_COUNT_MASK 0x7fff

extern unsigned short **gModelTables[MODEL_TABLE_COUNT];
extern unsigned short gModelTableCounts[MODEL_TABLE_COUNT];

void RegisterModelTable(int *table, int slot);
void RelocateModelTable(int slot, int delta);
void *CompactArchiveSubset(void *archive, ...);
void ClearModelSlotCounts(void);
void InitGpuPrimCodeCache(void);

#ifdef VERSION_JP
void ScaleModelPairVertices(short slot, short pairIndex, int scaleX, int scaleY, int scaleZ);
void ScaleModelMeshVertices(int scaleX, int scaleY, int scaleZ);
#endif

extern unsigned char gPolyF3Code;
extern unsigned char gPolyG3Code;
extern unsigned char gPolyF4Code;
extern unsigned char gPolyG4Code;
extern unsigned char gLineF2Code;
extern unsigned char gPolyFT3Code;
extern unsigned char gPolyGT3Code;
extern unsigned char gPolyFT4Code;
extern unsigned char gPolyGT4Code;
extern MATRIX gCombinedViewMatrix;

#endif
