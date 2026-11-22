#ifndef SHADOW_PROJ_H
#define SHADOW_PROJ_H

void BuildShadowProjectionMatrix(void);
void ProjectMatrixToShadowPlane(MATRIX *src, MATRIX *dst, int planeY);

#endif
