#ifndef ROT_MATRIX_H
#define ROT_MATRIX_H

#include <sys/types.h>
#include <libgte.h>
#include "scene_node.h"

extern RotMatrixBuilderFn gRotMatrixBuilders[SCENE_ROT_ORDER_COUNT];

extern MATRIX gZeroTransMatrix;

#ifdef VERSION_JP
void BuildRotMatrixZYX(SVECTOR *angles, MATRIX *matrix);
#endif
void BuildRotMatrixZXY(SVECTOR *angles, MATRIX *matrix);
void BuildRotMatrixYXZ(SVECTOR *angles, MATRIX *matrix);
void InitRotMatrixDispatchTable(void);
void SubmitSceneDisplayListNoOp(void);

#endif
