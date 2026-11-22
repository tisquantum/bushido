/* Stage 9's cloth strips: three wind-blown 2x7 cloth meshes, one simulated mesh drawn at three
 * origins as lit, double-sided POLY_GT3 pairs. */
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <inline_o.h>
#include <rand.h>

#include "cloth.h"
#include "cloth_strip.h"
#include "gpu_code.h"
#include "display_buffer.h"
#include "light_init.h"
#include "main.h"
#include "rot_matrix.h"
#include "otz.h"

/* Cloth vertex massOrPin values stored in SVECTOR.pad (read by InitClothPointsAndLinks). */
typedef enum ClothPinMode {
    CLOTH_PIN_RIGID = 0,
    CLOTH_PIN_FREE = -256
} ClothPinMode;

/* Front/back normal slot index in StageClothNormalPair (selected by GTE nclip sign bit). */
typedef enum StageClothNormalSide {
    STAGE_CLOTH_NORMAL_FRONT = 0,
    STAGE_CLOTH_NORMAL_BACK = 1
} StageClothNormalSide;

#define STAGE_CLOTH_ROWS       2
#define STAGE_CLOTH_COLS       7
#define STAGE_CLOTH_VERT_COUNT (STAGE_CLOTH_ROWS * STAGE_CLOTH_COLS)
#define STAGE_CLOTH_LINK_COUNT 19

typedef struct StageClothTexture {
    unsigned short tpage;
    unsigned short clut;
    unsigned short uv[STAGE_CLOTH_VERT_COUNT];
} StageClothTexture;

typedef struct StageClothNormalPair {
    SVECTOR side[2];
} StageClothNormalPair;

typedef struct StageClothSimWork {
    ClothPoint points[STAGE_CLOTH_VERT_COUNT];
    ClothLink links[STAGE_CLOTH_LINK_COUNT];
} StageClothSimWork;

/* 1796 bytes. */
typedef struct StageClothMesh {
    SVECTOR vertices[STAGE_CLOTH_VERT_COUNT]; /* rest-pose 2x7 vertices (.pad = ClothPinMode) */
    short modelIndex;                         /* gClothModels[16] slot from AllocClothModel */
    MATRIX matrix;
    VECTOR points[STAGE_CLOTH_VERT_COUNT];    /* simulated world-space output positions (>> 7) */
    StageClothSimWork simWork;                /* 14 ClothPoint (0x380) + 19 ClothLink (0x130) */
    StageClothNormalPair normals[STAGE_CLOTH_ROWS][STAGE_CLOTH_COLS];
} StageClothMesh;

#define STAGE_CLOTH_SAVED_CTX ((StageClothRenderContext *)getScratchAddr(0xda))

StageClothTexture gStageClothTexture;   /* texture page, CLUT and the 2x7 UVs */
StageClothMesh gStageClothMesh;         /* the one simulated mesh, drawn at each origin */
VECTOR gStageClothOrigins[STAGE_CLOTH_STRIP_COUNT]; /* the three strips' world origins */

Wind *gStageClothWindPtr = &gClothWindSlots[CLOTH_WIND_SLOT_STAGE];

void InitStageClothStrip(StageClothConfig *config)
{
    int row, col;
    short faceList[64];
    short *face;
    int topLeft, bottomLeft;
    int origin;
    SVECTOR *vertex;
    int baseY;
    StageClothMesh *mesh;
    MATRIX *matrix;
    ClothModelSpec spec;
    StageClothTexture *texture;

    texture = &gStageClothTexture;
    texture->tpage = config->tpage;
    texture->clut = config->clut;
    for (row = 0; row < STAGE_CLOTH_ROWS; row++) {
        for (col = 0; col < STAGE_CLOTH_COLS; col++) {
            int u = (config->u0 * ((STAGE_CLOTH_COLS - 1) - col) + config->u1 * col) / (STAGE_CLOTH_COLS - 1);
            int v = config->v0 * ((STAGE_CLOTH_ROWS - 1) - row) + config->v1 * row;
            texture->uv[row * STAGE_CLOTH_COLS + col] = (v << 8) | u;
        }
    }

    /* Face-link list entries: { vertexCount, stiffnessDiv, v0, v1, v2 }, 0-terminated. */
    face = faceList;
    for (row = 0; row < STAGE_CLOTH_ROWS - 1; row++) {
        for (col = 0; col < STAGE_CLOTH_COLS - 1; col++) {
            topLeft = row * STAGE_CLOTH_COLS + col;
            bottomLeft = (row + 1) * STAGE_CLOTH_COLS + col;
            face[0] = 3;
            face[1] = 3;
            face[2] = topLeft;
            face[3] = topLeft + 1;
            face[4] = bottomLeft;
            face += 5;
            face[0] = 3;
            face[1] = 3;
            face[2] = topLeft + 1;
            face[3] = bottomLeft;
            face[4] = bottomLeft + 1;
            face += 5;
        }
    }
    *face = 0;

    for (origin = 0; origin < STAGE_CLOTH_STRIP_COUNT; origin++) {
        gStageClothOrigins[origin] = config->origins[origin];
    }

    mesh = &gStageClothMesh;
    vertex = mesh->vertices;
    for (row = 0, baseY = -8; row < STAGE_CLOTH_ROWS; row++, baseY += 3500) {
        for (col = 0; col < STAGE_CLOTH_COLS; col++, vertex++) {
            int jitter = rand() & 15;
            int baseX = col * 3500 / (STAGE_CLOTH_COLS - 1) - 8;
            vertex->vx = (jitter + baseX) >> 4;
            vertex->vy = ((rand() & 15) + baseY) >> 4;
            vertex->vz = 0;
            if (row != 0) {
                vertex->pad = CLOTH_PIN_FREE;
            } else {
                vertex->pad = CLOTH_PIN_RIGID;
            }
        }
    }

    matrix = &mesh->matrix;
    InitPropIdentityMatrix3x3(matrix);
    RotMatrixY((rand() & 15) + 1016, matrix);
    matrix->t[0] = 0;
    matrix->t[1] = 0;
    matrix->t[2] = 0;
    spec.pointCount = STAGE_CLOTH_VERT_COUNT;
    spec.faceLinkLists = faceList;
    spec.workBuffer = &mesh->simWork;
    spec.sourceVertices = mesh->vertices;
    spec.initialTransform = matrix;
    spec.damping = 0xe80;
    mesh->modelIndex = AllocClothModel(&spec);
}

static void ComputeStageClothNormals(StageClothRenderContext *ctx, StageClothMesh *mesh)
{
    int row, col;
    int index;
    int neighborA, neighborB;
    unsigned int lengthSq;
    int leadingZeros;
    unsigned int shift;
    int length;

    for (row = 0; row < STAGE_CLOTH_ROWS; row++) {
        for (col = 0; col < STAGE_CLOTH_COLS; col++) {
            index = row * STAGE_CLOTH_COLS + col;
            if (col < STAGE_CLOTH_COLS - 1) {
                if (row < STAGE_CLOTH_ROWS - 1) {
                    neighborA = row * STAGE_CLOTH_COLS + col + 1;
                    neighborB = (row + 1) * STAGE_CLOTH_COLS + col;
                } else {
                    neighborA = (row - 1) * STAGE_CLOTH_COLS + col;
                    neighborB = row * STAGE_CLOTH_COLS + col + 1;
                }
            } else {
                if (row < STAGE_CLOTH_ROWS - 1) {
                    neighborA = (row + 1) * STAGE_CLOTH_COLS + col;
                    neighborB = row * STAGE_CLOTH_COLS + col - 1;
                } else {
                    neighborA = row * STAGE_CLOTH_COLS + col - 1;
                    neighborB = (row - 1) * STAGE_CLOTH_COLS + col;
                }
            }
            ctx->work.normalCalc.edgeA.vx = mesh->points[neighborA].vx - mesh->points[index].vx;
            ctx->work.normalCalc.edgeA.vy = mesh->points[neighborA].vy - mesh->points[index].vy;
            ctx->work.normalCalc.edgeA.vz = mesh->points[neighborA].vz - mesh->points[index].vz;
            ctx->work.normalCalc.edgeB.vx = mesh->points[neighborB].vx - mesh->points[index].vx;
            ctx->work.normalCalc.edgeB.vy = mesh->points[neighborB].vy - mesh->points[index].vy;
            ctx->work.normalCalc.edgeB.vz = mesh->points[neighborB].vz - mesh->points[index].vz;
            gte_ldopv1(&ctx->work.normalCalc.edgeB);
            gte_ldopv2(&ctx->work.normalCalc.edgeA);
            gte_op0();
            gte_stlvnl(&ctx->work.normalCalc.normal);
            gte_sqr0();
            gte_stlvnl(&ctx->work.normalCalc.edgeA);
            lengthSq = ctx->work.normalCalc.edgeA.vx + ctx->work.normalCalc.edgeA.vy + ctx->work.normalCalc.edgeA.vz;
            if (lengthSq < 1024) {
                length = gSqrtLutQ3[lengthSq] >> 3;
            } else {
                gte_ldlzc(lengthSq);
                gte_nop();
                gte_nop();
                gte_stlzc(&leadingZeros);
                shift = leadingZeros;
                shift = 22 - (shift & ~1);
                lengthSq = gSqrtLutQ3[lengthSq >> shift];
                length = (lengthSq << 16) >> (19 - (shift >> 1));
            }
            if (length == 0) {
                length = 1;
            }
            length = 0x1000000 / length;
            ctx->work.normalCalc.normal.vx = ctx->work.normalCalc.normal.vx * length >> 12;
            ctx->work.normalCalc.normal.vy = ctx->work.normalCalc.normal.vy * length >> 12;
            ctx->work.normalCalc.normal.vz = ctx->work.normalCalc.normal.vz * length >> 12;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_FRONT].vx = ctx->work.normalCalc.normal.vx;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_FRONT].vy = ctx->work.normalCalc.normal.vy;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_FRONT].vz = ctx->work.normalCalc.normal.vz;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_BACK].vx = -ctx->work.normalCalc.normal.vx;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_BACK].vy = -ctx->work.normalCalc.normal.vy;
            mesh->normals[row][col].side[STAGE_CLOTH_NORMAL_BACK].vz = -ctx->work.normalCalc.normal.vz;
        }
    }
}

int UpdateAndDrawStageClothStrips(StageClothRenderContext *ctx)
{
    StageClothRenderContext *savedCtx;
    ClothItem item;
    int savedStrength, savedAngle;
    POLY_GT3 *packet;
    StageClothMesh *mesh;
    StageClothTexture *texture;
    int index;
    int origin, row, col, cornerRow, cornerCol;
    int dx, dz;
    long depth, mantissa;

    savedCtx = STAGE_CLOTH_SAVED_CTX;
    *savedCtx = *ctx;
    mesh = &gStageClothMesh;
    item.modelIndex = mesh->modelIndex;
    item.flags = CLOTH_ITEM_FLAG_NONE;
    item.windSlot = CLOTH_WIND_SLOT_STAGE;
    item.outputPositions = mesh->points;
    item.transform = &mesh->matrix;
    item.colliderInput = 0;
    savedStrength = gStageClothWindPtr->strength;
    gStageClothWindPtr->strength = savedStrength / 4;
    savedAngle = gStageClothWindPtr->angle;
    gStageClothWindPtr->angle += (rand() & 127) - 64;
    UpdateClothModelsWithOutput(&item, 1);
    gStageClothWindPtr->angle = savedAngle;
    gStageClothWindPtr->strength = savedStrength;
    *ctx = *savedCtx;

    ComputeStageClothNormals(ctx, mesh);
    SetRotMatrix(&ctx->matrix);
    SetTransMatrix(&gZeroTransMatrix);
    packet = ctx->packet;
    texture = &gStageClothTexture;
    for (origin = 0; origin < STAGE_CLOTH_STRIP_COUNT; origin++) {
        ctx->work.draw.offset.vx = gStageClothOrigins[origin].vx - ctx->eye.vx;
        ctx->work.draw.offset.vz = gStageClothOrigins[origin].vz - ctx->eye.vz;
        /* Frustum cull outside +/-384 (+/-33.75 deg) of camera yaw and beyond max XZ distance. */
        if ((unsigned int)(((ratan2(ctx->work.draw.offset.vz, ctx->work.draw.offset.vx) - ctx->yaw + 0x400) &
                            0xfff) - 384) < 3329)
            continue;
        dx = ctx->work.draw.offset.vx >> 4;
        dz = ctx->work.draw.offset.vz >> 4;
        if (dx * dx + dz * dz > 0x340000)
            continue;
        ctx->work.draw.offset.vy = gStageClothOrigins[origin].vy - ctx->eye.vy;
        for (row = 0; row < STAGE_CLOTH_ROWS - 1; row++) {
            for (col = 0; col < STAGE_CLOTH_COLS - 1; col++) {
                for (cornerRow = 0; cornerRow < 2; cornerRow++) {
                    for (cornerCol = 0; cornerCol < 2; cornerCol++) {
                        index = (row + cornerRow) * STAGE_CLOTH_COLS + (col + cornerCol);
                        ctx->corner[cornerRow * 2 + cornerCol].vx = (mesh->points[index].vx << 4) +
                            ctx->work.draw.offset.vx;
                        ctx->corner[cornerRow * 2 + cornerCol].vy = (mesh->points[index].vy << 4) +
                            ctx->work.draw.offset.vy;
                        ctx->corner[cornerRow * 2 + cornerCol].vz = (mesh->points[index].vz << 4) +
                            ctx->work.draw.offset.vz;
                    }
                }
                gte_ldv3(&ctx->corner[0], &ctx->corner[1], &ctx->corner[2]);
                gte_rtpt();
                SetPolyGT3(packet);
                index = row * STAGE_CLOTH_COLS + col;
                gte_stsxy3(&ctx->screen[0], &ctx->screen[1], &ctx->screen[2]);
                gte_ldv0(&ctx->corner[3]);
                gte_rtps();
                *(long *)&packet->x0 = ctx->screen[0];
                *(long *)&packet->x1 = ctx->screen[1];
                gte_stsxy(&ctx->screen[3]);
                gte_avsz4();
                *(unsigned short *)&packet->u0 = texture->uv[index];
                *(unsigned short *)&packet->u1 = texture->uv[index + 1];
                *(unsigned short *)&packet->u2 = texture->uv[index + STAGE_CLOTH_COLS];
                gte_stotz(&ctx->work.draw.depth);
                /* Piecewise-logarithmic compression of GTE OTZ into 4096 OT buckets. */
                depth = ctx->work.draw.depth;
                mantissa = depth >> 1;
                depth >>= 12;
                ctx->work.draw.depth = ((unsigned int)((mantissa & 0x7ff) | -4096) >> depth) & 0xfff;
                if (ctx->work.draw.depth < OTZ_BUCKET_COUNT) {
                    gte_ldsxy3(ctx->screen[0], ctx->screen[1], ctx->screen[2]);
                    gte_nclip();
                    *(long *)&packet->x2 = ctx->screen[2];
                    gte_stopz(&ctx->work.draw.normalSide);
                    ctx->work.draw.normalSide = (unsigned long)ctx->work.draw.normalSide >> 31;
                    gte_ldv3(&mesh->normals[row][col].side[ctx->work.draw.normalSide],
                             &mesh->normals[row][col + 1].side[ctx->work.draw.normalSide],
                             &mesh->normals[row + 1][col].side[ctx->work.draw.normalSide]);
                    gte_nct();
                    packet->tpage = gStageClothTexture.tpage;
                    packet->clut = gStageClothTexture.clut;
                    gte_strgb3(&packet->r0, &packet->r1, &packet->r2);
                    packet->code = GPU_CODE_POLY_GT3;
                    AddPrim(&gCurrentOt[ctx->work.draw.depth], packet++);

                    SetPolyGT3(packet);
                    gte_ldsxy3(ctx->screen[2], ctx->screen[1], ctx->screen[3]);
                    gte_nclip();
                    *(long *)&packet->x0 = ctx->screen[1];
                    *(long *)&packet->x1 = ctx->screen[2];
                    *(long *)&packet->x2 = ctx->screen[3];
                    gte_stopz(&ctx->work.draw.normalSide);
                    ctx->work.draw.normalSide = (unsigned long)ctx->work.draw.normalSide >> 31;
                    gte_ldv3(&mesh->normals[row][col + 1].side[ctx->work.draw.normalSide],
                             &mesh->normals[row + 1][col].side[ctx->work.draw.normalSide],
                             &mesh->normals[row + 1][col + 1].side[ctx->work.draw.normalSide]);
                    gte_nct();
                    *(unsigned short *)&packet->u0 = texture->uv[index + 1];
                    *(unsigned short *)&packet->u1 = texture->uv[index + STAGE_CLOTH_COLS];
                    *(unsigned short *)&packet->u2 = texture->uv[index + STAGE_CLOTH_COLS + 1];
                    packet->tpage = gStageClothTexture.tpage;
                    packet->clut = gStageClothTexture.clut;
                    gte_strgb3(&packet->r0, &packet->r1, &packet->r2);
                    packet->code = GPU_CODE_POLY_GT3;
                    AddPrim(&gCurrentOt[ctx->work.draw.depth], packet++);
                }
            }
        }
    }
    ctx->packet = packet;
    return 0;
}
