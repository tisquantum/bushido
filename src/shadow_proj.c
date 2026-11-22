/* Planar shadows: the shear that flattens a fighter's transform onto the floor along the primary
 * light, and the projection of a transform onto a horizontal plane with it or a light slot's. */
#include <sys/types.h>
#include <libgte.h>

#include "light.h"
#include "render.h"
#include "shadow_proj.h"

static MATRIX gShadowProjectionMatrix;

/* x' = x - (lx / ly) * y, y' = 0, z' = z - (lz / ly) * y, from P1's primary light (Q12). */
void BuildShadowProjectionMatrix(void)
{
    int shearX = (short)((gLightSlots[LIGHT_SLOT_P1].lightMatrix.m[LIGHT_DIR_PRIMARY][0] << 12) /
                         gLightSlots[LIGHT_SLOT_P1].lightMatrix.m[LIGHT_DIR_PRIMARY][1]);
    int shearZ = (short)((gLightSlots[LIGHT_SLOT_P1].lightMatrix.m[LIGHT_DIR_PRIMARY][2] << 12) /
                         gLightSlots[LIGHT_SLOT_P1].lightMatrix.m[LIGHT_DIR_PRIMARY][1]);

    gShadowProjectionMatrix.m[0][2] = 0;
    gShadowProjectionMatrix.m[1][0] = 0;
    gShadowProjectionMatrix.m[1][1] = 0;
    gShadowProjectionMatrix.m[1][2] = 0;
    gShadowProjectionMatrix.m[2][0] = 0;
    gShadowProjectionMatrix.m[0][0] = ONE;
    gShadowProjectionMatrix.m[2][2] = ONE;
    gShadowProjectionMatrix.m[0][1] = -shearX;
    gShadowProjectionMatrix.m[2][1] = -shearZ;
}

void ProjectMatrixToShadowPlane(MATRIX *src, MATRIX *dst, int planeY)
{
    MulMatrix3x3Fast(&gShadowProjectionMatrix, src, dst);
    dst->t[0] = src->t[0] + (((src->t[1] - planeY) * gShadowProjectionMatrix.m[0][1]) >> 12);
    dst->t[1] = planeY;
    dst->t[2] = src->t[2] + (((src->t[1] - planeY) * gShadowProjectionMatrix.m[2][1]) >> 12);
}

/* Nothing calls it. */
static void ProjectTransformToHeight(int lightSlot, MATRIX *src, MATRIX *dst, long height)
{
    MATRIX projection;

    projection.m[0][0] = ONE;
    projection.m[0][1] = -((gLightSlots[lightSlot].lightMatrix.m[LIGHT_DIR_PRIMARY][0] << 12) /
                           gLightSlots[lightSlot].lightMatrix.m[LIGHT_DIR_PRIMARY][1]);
    projection.m[0][2] = 0;
    projection.m[1][0] = 0;
    projection.m[1][1] = 0;
    projection.m[1][2] = 0;
    projection.m[2][0] = 0;
    projection.m[2][1] = -((gLightSlots[lightSlot].lightMatrix.m[LIGHT_DIR_PRIMARY][2] << 12) /
                           gLightSlots[lightSlot].lightMatrix.m[LIGHT_DIR_PRIMARY][1]);
    projection.m[2][2] = ONE;

    MulMatrix3x3Fast(&projection, src, dst);

    dst->t[0] = src->t[0] + (((src->t[1] - height) * projection.m[0][1]) >> 12);
    dst->t[1] = height;
    dst->t[2] = src->t[2] + (((src->t[1] - height) * projection.m[2][1]) >> 12);
}
