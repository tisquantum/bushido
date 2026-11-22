/*
 * Rotation matrices: Euler-angle builders, the table scene nodes select them from, a 3x3 transpose.
 * The Japanese build has a builder for every axis order, the planar light projection onto the ground
 * and the C double-sided textured triangle submitters.
 */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#include "light.h"
#include "packet_build.h"
#include "scene_node.h"
#include "render_scratch.h"
#include "gpu_code.h"
#include "rot_matrix.h"
#include "scene_math.h"
#include "globals.h"
#include "render.h"
#include "otz.h"

#define SIN(x) (gSinLutQ12[(x) & 0xfff])
#define COS(x) (gSinLutQ12[((x) + 1024) & 0xfff])

#ifdef VERSION_JP
void BuildRotMatrixZYX(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = (cosY * cosZ) >> 12;
    matrix->m[0][1] = ((((sinX * sinY) >> 12) * cosZ) + (cosX * -sinZ)) >> 12;
    matrix->m[0][2] = ((((cosX * sinY) >> 12) * cosZ) + (sinX * sinZ)) >> 12;
    matrix->m[1][0] = (cosY * sinZ) >> 12;
    matrix->m[1][1] = ((((sinX * sinY) >> 12) * sinZ) + (cosX * cosZ)) >> 12;
    matrix->m[1][2] = ((((cosX * sinY) >> 12) * sinZ) + (-sinX * cosZ)) >> 12;
    matrix->m[2][0] = -sinY;
    matrix->m[2][1] = (sinX * cosY) >> 12;
    matrix->m[2][2] = (cosX * cosY) >> 12;
}

static void BuildRotMatrixYZX(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = (cosY * cosZ) >> 12;
    matrix->m[0][1] = ((((cosX * cosY) >> 12) * -sinZ) + (sinX * sinY)) >> 12;
    matrix->m[0][2] = ((((sinX * cosY) >> 12) * sinZ) + (cosX * sinY)) >> 12;
    matrix->m[1][0] = sinZ;
    matrix->m[1][1] = (cosX * cosZ) >> 12;
    matrix->m[1][2] = (-sinX * cosZ) >> 12;
    matrix->m[2][0] = (-sinY * cosZ) >> 12;
    matrix->m[2][1] = ((((cosX * sinY) >> 12) * sinZ) + (sinX * cosY)) >> 12;
    matrix->m[2][2] = ((((-sinX * sinY) >> 12) * sinZ) + (cosX * cosY)) >> 12;
}
#endif

void BuildRotMatrixZXY(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = ((((sinX * -sinY) >> 12) * sinZ) + (cosY * cosZ)) >> 12;
    matrix->m[0][1] = (cosX * -sinZ) >> 12;
    matrix->m[0][2] = ((((sinX * cosY) >> 12) * sinZ) + (sinY * cosZ)) >> 12;
    matrix->m[1][0] = ((((sinX * sinY) >> 12) * cosZ) + (cosY * sinZ)) >> 12;
    matrix->m[1][1] = (cosX * cosZ) >> 12;
    matrix->m[1][2] = ((((-sinX * cosY) >> 12) * cosZ) + (sinY * sinZ)) >> 12;
    matrix->m[2][0] = (cosX * -sinY) >> 12;
    matrix->m[2][1] = sinX;
    matrix->m[2][2] = (cosX * cosY) >> 12;
}

#ifdef VERSION_JP
static void BuildRotMatrixXZY(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = (cosY * cosZ) >> 12;
    matrix->m[0][1] = -sinZ;
    matrix->m[0][2] = (sinY * cosZ) >> 12;
    matrix->m[1][0] = ((((cosX * cosY) >> 12) * sinZ) + (sinX * sinY)) >> 12;
    matrix->m[1][1] = (cosX * cosZ) >> 12;
    matrix->m[1][2] = ((((cosX * sinY) >> 12) * sinZ) + (-sinX * cosY)) >> 12;
    matrix->m[2][0] = ((((sinX * cosY) >> 12) * sinZ) + (-sinY * cosX)) >> 12;
    matrix->m[2][1] = (sinX * cosZ) >> 12;
    matrix->m[2][2] = ((((sinX * sinY) >> 12) * sinZ) + (cosX * cosY)) >> 12;
}
#endif

void BuildRotMatrixYXZ(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = ((((sinX * sinY) >> 12) * sinZ) + (cosY * cosZ)) >> 12;
    matrix->m[0][1] = ((((sinX * sinY) >> 12) * cosZ) + (cosY * -sinZ)) >> 12;
    matrix->m[0][2] = (cosX * sinY) >> 12;
    matrix->m[1][0] = (cosX * sinZ) >> 12;
    matrix->m[1][1] = (cosX * cosZ) >> 12;
    matrix->m[1][2] = -sinX;
    matrix->m[2][0] = ((((sinX * cosY) >> 12) * sinZ) + (-sinY * cosZ)) >> 12;
    matrix->m[2][1] = ((((sinX * cosY) >> 12) * cosZ) + (sinY * sinZ)) >> 12;
    matrix->m[2][2] = (cosX * cosY) >> 12;
}

#ifdef VERSION_JP
static void BuildRotMatrixXYZ(SVECTOR *angles, MATRIX *matrix)
{
    short sinX, sinY, sinZ;
    short cosX, cosY, cosZ;

    sinX = SIN(angles->vx);
    cosX = COS(angles->vx);
    sinY = SIN(angles->vy);
    cosY = COS(angles->vy);
    sinZ = SIN(angles->vz);
    cosZ = COS(angles->vz);

    matrix->m[0][0] = (cosY * cosZ) >> 12;
    matrix->m[0][1] = (cosY * -sinZ) >> 12;
    matrix->m[0][2] = sinY;
    matrix->m[1][0] = ((((sinX * sinY) >> 12) * cosZ) + (cosX * sinZ)) >> 12;
    matrix->m[1][1] = ((((sinX * sinY) >> 12) * -sinZ) + (cosX * cosZ)) >> 12;
    matrix->m[1][2] = (-sinX * cosY) >> 12;
    matrix->m[2][0] = ((((cosX * -sinY) >> 12) * cosZ) + (sinX * sinZ)) >> 12;
    matrix->m[2][1] = ((((cosX * sinY) >> 12) * sinZ) + (sinX * cosZ)) >> 12;
    matrix->m[2][2] = (cosX * cosY) >> 12;
}
#endif

/* Rotation-matrix builders by Euler order, indexed by SceneNode.rotOrder.
 * The US/EU builds keep only the even orders; the Japanese build fills all six. */
RotMatrixBuilderFn gRotMatrixBuilders[SCENE_ROT_ORDER_COUNT];

void InitRotMatrixDispatchTable(void)
{
    gRotMatrixBuilders[SCENE_ROT_ORDER_FAST] = BuildRotationMatrixFast;
#ifdef VERSION_JP
    gRotMatrixBuilders[SCENE_ROT_ORDER_YZX] = BuildRotMatrixYZX;
#endif
    gRotMatrixBuilders[SCENE_ROT_ORDER_ZXY] = BuildRotMatrixZXY;
#ifdef VERSION_JP
    gRotMatrixBuilders[SCENE_ROT_ORDER_XZY] = BuildRotMatrixXZY;
#endif
    gRotMatrixBuilders[SCENE_ROT_ORDER_YXZ] = BuildRotMatrixYXZ;
#ifdef VERSION_JP
    gRotMatrixBuilders[SCENE_ROT_ORDER_XYZ] = BuildRotMatrixXYZ;
#endif
}

#ifdef VERSION_JP

/* Projects a transform onto the Y=0 ground plane along the primary light direction of lightSlot
 * (the Y=0 counterpart of ProjectTransformToHeight). Nothing calls it. */
static void ProjectTransformToGround(int lightSlot, MATRIX *source, MATRIX *dest)
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

    MulMatrix3x3Fast(&projection, source, dest);

    dest->t[0] = source->t[0] + ((source->t[1] * projection.m[0][1]) >> 12);
    dest->t[1] = 0;
    dest->t[2] = source->t[2] + ((source->t[1] * projection.m[2][1]) >> 12);
}
#endif

/* Nothing calls it. */
static void TransposeMatrix3x3(MATRIX *matrix)
{
    short saved;

    saved = matrix->m[2][0];
    matrix->m[2][0] = matrix->m[0][2];
    matrix->m[0][2] = saved;

    saved = matrix->m[1][0];
    matrix->m[1][0] = matrix->m[0][1];
    matrix->m[0][1] = saved;

    saved = matrix->m[2][1];
    matrix->m[2][1] = matrix->m[1][2];
    matrix->m[1][2] = saved;
}

void SubmitSceneDisplayListNoOp(void)
{
}

#ifdef VERSION_JP

#define DOUBLE_SIDED_MAX_OTZ      0x4000
#define DOUBLE_SIDED_TRI_Z_BIAS   17
#define DOUBLE_SIDED_QUAD_Z_BIAS  20
#define POLY_FT3_TAG_LEN          0x07000000

/*
 * Submits double-sided flat-lit POLY_FT3 primitives from a mesh stream into gPacketCursor.
 * Each record holds a double-buffered POLY_FT3 template pair (16 words), packed vertex
 * indices (1 word), and an SVECTOR face normal (2 words). Nothing calls it.
 */
static u_long *DrawDoubleSidedPolyFt3(u_short primCount, u_long *primStream)
{
    CVECTOR litColor;
    SVECTOR faceNormal;
    POLY_FT3 *templateTri;
    u_long vertexIndices;
    u_long vert0, vert1, vert2;
    long screenXy0, screenXy1, screenXy2;
    long *screenXy;
    short *screenZ;
    long otz;
    SVECTOR *sourceNormal;

    screenXy = MESH_SCRATCH->screenXy;
    screenZ = MESH_SCRATCH->screenZ;
    MESH_SCRATCH->packetCursor = gPacketCursor;
    while (primCount--) {
        templateTri = (POLY_FT3 *)primStream;
        SetRGBcd((CVECTOR *)&templateTri->r0);
        primStream += 16;
        vertexIndices = *primStream;
        sourceNormal = (SVECTOR *)(primStream + 1);

        vert0 = vertexIndices & 0xff;
        vert1 = (vertexIndices >> 8) & 0xff;
        vert2 = (vertexIndices >> 16) & 0xff;
        otz = AverageZ3(screenZ[vert0], screenZ[vert1], screenZ[vert2]);
        if (otz > 0 && otz < DOUBLE_SIDED_MAX_OTZ) {
            screenXy0 = screenXy[vert0];
            screenXy1 = screenXy[vert1];
            screenXy2 = screenXy[vert2];
            if (NormalClip(screenXy0, screenXy1, screenXy2) > 0) {
                faceNormal.vx = -sourceNormal->vx;
                faceNormal.vy = -sourceNormal->vy;
                faceNormal.vz = -sourceNormal->vz;
                otz += DOUBLE_SIDED_TRI_Z_BIAS;
            } else {
                faceNormal.vx = sourceNormal->vx;
                faceNormal.vy = sourceNormal->vy;
                faceNormal.vz = sourceNormal->vz;
                otz -= DOUBLE_SIDED_TRI_Z_BIAS;
            }
            NormalColor(&faceNormal, &litColor);
            MESH_SCRATCH->packetCursor->tag = POLY_FT3_TAG_LEN;
            *(CVECTOR *)&MESH_SCRATCH->packetCursor->r0 = litColor;
            *(long *)&MESH_SCRATCH->packetCursor->x0 = screenXy0;
            *(long *)&MESH_SCRATCH->packetCursor->x1 = screenXy1;
            *(long *)&MESH_SCRATCH->packetCursor->x2 = screenXy2;
            *(u_long *)&MESH_SCRATCH->packetCursor->u0 = *(u_long *)&templateTri->u0;
            *(u_long *)&MESH_SCRATCH->packetCursor->u1 = *(u_long *)&templateTri->u1;
            *(u_long *)&MESH_SCRATCH->packetCursor->u2 = *(u_long *)&templateTri->u2;
            AddPrim(MESH_SCRATCH->orderingTable + CompressDepthToOtz(otz << 2, 0), MESH_SCRATCH->packetCursor);
            MESH_SCRATCH->packetCursor++;
        }

        primStream += 3;
    }
    gPacketCursor = MESH_SCRATCH->packetCursor;
    return primStream;
}

/*
 * Submits double-sided flat-lit quad records split into two POLY_FT3 triangles (0,1,2) and
 * (1,3,2) into gPacketCursor. Each record holds a double-buffered POLY_FT4 template pair
 * (20 words), packed 4-vertex indices (1 word), and an SVECTOR face normal (2 words).
 * Nothing calls it.
 */
static u_long *DrawDoubleSidedPolyFt4AsTris(u_short primCount, u_long *primStream)
{
    CVECTOR litColor;
    SVECTOR faceNormal;
    POLY_FT4 *templateQuad;
    u_long vertexIndices;
    long screenXy0, screenXy1, screenXy2, screenXy3;
    long *screenXy;
    short *screenZ;
    long otz;
    SVECTOR *sourceNormal;

    screenXy = MESH_SCRATCH->screenXy;
    screenZ = MESH_SCRATCH->screenZ;
    MESH_SCRATCH->packetCursor = gPacketCursor;
    while (primCount--) {
        templateQuad = (POLY_FT4 *)primStream;
        primStream += 20;
        vertexIndices = *primStream;
        sourceNormal = (SVECTOR *)(primStream + 1);

        screenXy0 = screenXy[vertexIndices & 0xff];
        screenXy1 = screenXy[(vertexIndices >> 8) & 0xff];
        screenXy2 = screenXy[(vertexIndices >> 16) & 0xff];
        screenXy3 = screenXy[vertexIndices >> 24];

        otz = AverageZ3(
            screenZ[vertexIndices & 0xff],
            screenZ[(vertexIndices >> 8) & 0xff],
            screenZ[(vertexIndices >> 16) & 0xff]);
        if (otz > 0 && otz < DOUBLE_SIDED_MAX_OTZ) {
            if (NormalClip(screenXy0, screenXy1, screenXy2) > 0) {
                faceNormal.vx = -sourceNormal->vx;
                faceNormal.vy = -sourceNormal->vy;
                faceNormal.vz = -sourceNormal->vz;
                otz += DOUBLE_SIDED_QUAD_Z_BIAS;
            } else {
                faceNormal.vx = sourceNormal->vx;
                faceNormal.vy = sourceNormal->vy;
                faceNormal.vz = sourceNormal->vz;
                otz -= DOUBLE_SIDED_QUAD_Z_BIAS;
            }
            NormalColor(&faceNormal, &litColor);
            litColor.cd = GPU_CODE_POLY_FT3;
            MESH_SCRATCH->packetCursor->tag = POLY_FT3_TAG_LEN;
            *(CVECTOR *)&MESH_SCRATCH->packetCursor->r0 = litColor;
            *(long *)&MESH_SCRATCH->packetCursor->x0 = screenXy0;
            *(long *)&MESH_SCRATCH->packetCursor->x1 = screenXy1;
            *(long *)&MESH_SCRATCH->packetCursor->x2 = screenXy2;
            *(u_long *)&MESH_SCRATCH->packetCursor->u0 = *(u_long *)&templateQuad->u0;
            *(u_long *)&MESH_SCRATCH->packetCursor->u1 = *(u_long *)&templateQuad->u1;
            *(u_long *)&MESH_SCRATCH->packetCursor->u2 = *(u_long *)&templateQuad->u2;
            AddPrim(MESH_SCRATCH->orderingTable + CompressDepthToOtz(otz << 2, 0), MESH_SCRATCH->packetCursor);
            MESH_SCRATCH->packetCursor++;
        }

        otz = AverageZ3(
            screenZ[(vertexIndices >> 8) & 0xff],
            screenZ[(vertexIndices >> 16) & 0xff],
            screenZ[vertexIndices >> 24]);
        if (otz > 0 && otz < DOUBLE_SIDED_MAX_OTZ) {
            if (NormalClip(screenXy1, screenXy3, screenXy2) > 0) {
                faceNormal.vx = -sourceNormal->vx;
                faceNormal.vy = -sourceNormal->vy;
                faceNormal.vz = -sourceNormal->vz;
                otz += DOUBLE_SIDED_QUAD_Z_BIAS;
            } else {
                faceNormal.vx = sourceNormal->vx;
                faceNormal.vy = sourceNormal->vy;
                faceNormal.vz = sourceNormal->vz;
                otz -= DOUBLE_SIDED_QUAD_Z_BIAS;
            }
            NormalColor(&faceNormal, &litColor);
            litColor.cd = GPU_CODE_POLY_FT3;
            MESH_SCRATCH->packetCursor->tag = POLY_FT3_TAG_LEN;
            *(CVECTOR *)&MESH_SCRATCH->packetCursor->r0 = litColor;
            *(long *)&MESH_SCRATCH->packetCursor->x0 = screenXy1;
            *(long *)&MESH_SCRATCH->packetCursor->x1 = screenXy3;
            *(long *)&MESH_SCRATCH->packetCursor->x2 = screenXy2;
            MESH_SCRATCH->packetCursor->tpage = templateQuad->tpage;
            MESH_SCRATCH->packetCursor->clut = templateQuad->clut;
            /* the original reads each (u, v) pair as a word and stores its low half */
            *(u_short *)&MESH_SCRATCH->packetCursor->u0 = *(u_long *)&templateQuad->u1;
            *(u_short *)&MESH_SCRATCH->packetCursor->u1 = *(u_long *)&templateQuad->u3;
            *(u_short *)&MESH_SCRATCH->packetCursor->u2 = *(u_long *)&templateQuad->u2;
            AddPrim(MESH_SCRATCH->orderingTable + CompressDepthToOtz(otz << 2, 0), MESH_SCRATCH->packetCursor);
            MESH_SCRATCH->packetCursor++;
        }

        primStream += 3;
    }
    gPacketCursor = MESH_SCRATCH->packetCursor;
    return primStream;
}
#endif
