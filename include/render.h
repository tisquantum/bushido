#ifndef RENDER_H
#define RENDER_H

#include <sys/types.h>
#include <libgte.h>

/* The polygon drawers of the dispatch tables (otz.c) take their inputs in registers. */
void DrawPolyF3Lit(void);
void DrawPolyFt3Lit(void);
void DrawPolyFt3Unlit(void);
void DrawPolyFt3DoubleLit(void);
void DrawPolyF4Lit(void);
void DrawPolyFt4Lit(void);
void DrawPolyFt4Unlit(void);
void DrawPolyFt4DoubleLit(void);
void DrawPolyG3Lit(void);
void DrawPolyGt3Lit(void);
void DrawPolyGt3DoubleLit(void);
void DrawPolyG4Lit(void);
void DrawPolyGt4Lit(void);
void DrawPolyGt4DoubleLit(void);
void DrawPolyF4WithDrawMode(void);
void IgnoreSkinnedPolyF3(void);
void IgnoreSkinnedPolyF4(void);
void DrawSkinnedPolyFt3Prelit(void);
void DrawSkinnedPolyFt4Prelit(void);
void DrawSkinnedPolyGt3Prelit(void);
void DrawSkinnedPolyGt4Prelit(void);
void DrawPolyFt3LitDynamic(void);
void DrawPolyFt3DoubleLitDynamic(void);
void DrawPolyFt4LitDynamic(void);
void DrawPolyFt4DoubleLitDynamic(void);
void DrawPolyGt3LitDynamic(void);
void DrawPolyGt4LitDynamic(void);
void DrawShadowPolyF3Dynamic(void);
void DrawShadowPolyF4Dynamic(void);

void RenderSceneDisplayList(void);
void *RenderSkinnedSkeletalMesh(MATRIX **boneMatrices, void *modelData, MATRIX *viewMatrix, int scaleShift);
short *DrawSubdivPolyFt4Dynamic(int count, void *vertices, short *faces, int lodAndFlags);

void BuildRotationMatrixFast(SVECTOR *angles, MATRIX *matrix);
void MulMatrix3x3Fast(MATRIX *left, MATRIX *right, MATRIX *out);
void InterpolateOrthonormalMatrix(const MATRIX *from, const MATRIX *to, int weight, MATRIX *out);
void TransformSvectorByMatrix(MATRIX *matrix, SVECTOR *in, VECTOR *out);
void SetGteRotAndTransMatrix(MATRIX *matrix);
void GetGteFarColor(CVECTOR *color);
int ComputeVectorLength3D(int x, int y, int z);
int ComputeVectorLengthSq3D(int x, int y, int z);
void CopyWordsFast(const void *source, void *dest, unsigned int byteCount);

/* Reads no arguments; some callers pass what they report. */
void TriggerDebugTrap();

#define SKINNED_PRELIT_COLOR_MAX 500
extern CVECTOR gSkinnedPrelitColors[SKINNED_PRELIT_COLOR_MAX];

#endif
