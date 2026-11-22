#ifndef FIGHTER_CLOTH_H
#define FIGHTER_CLOTH_H

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"

/* The fighters' garment cloth (src/fighter_cloth.c): what other units call. */

void ResetClothWindWithJitter(void);
void SetClothWindTarget(int magnitude, int angle, int windSlot);
void UpdateAllFighterCloth(void);
void InitClothSystem(void);
void FreeAllFighterCloth(void);
int CalcFighterClothWorkSize(int slot);
int BuildFighterClothParts(int slot);
void AddClothLimbCollider(struct SceneNode *node, struct SceneNode *limb,
                          int x1, int y1, int z1, int x2, int y2, int z2,
                          int radius);
void RequestClothSnapReset(void);
void SetSlotClothFloorY(int slot, int floorY);
void RelocateFighterClothSlot(int slot, int deltaBytes);
void FreeFighterClothSlot(int slot);

void TransformClothColliders(void);
void InitClothPartFromMesh(int slot, struct SceneNode *node, short *weightStream);
void ApplyClothVertexWeights(SVECTOR *vertices, int vertexCount, short *weightStream);
int AlignUp4(int size);
void SetAllClothResetFlags(void);
void ClearAllClothResetFlags(void);

#endif
