/* The attached part pool: thirty scene nodes lent out for parts hung on a fighter for a while
 * (only the gun's muzzle flash), updated each frame. */
#include <sys/types.h>
#include <libgte.h>
#include "attached_part.h"
#include "muzzle_flash.h"
#include "scene_node.h"

#define ATTACHED_PART_COUNT 30
static struct SceneNode gAttachedPartPool[ATTACHED_PART_COUNT];

void ClearAttachedPartPool(void)
{
    struct SceneNode *node = gAttachedPartPool;
    int i;

    /* The node never advances: only the first slot is cleared, 30 times. */
    for (i = 0; i < ATTACHED_PART_COUNT; i++) {
        node->param.attached.active = 0;
    }
}

struct SceneNode *AllocAttachedPartSlot(int kind)
{
    struct SceneNode *node;
    int i;

    node = gAttachedPartPool;
    for (i = 0; i < ATTACHED_PART_COUNT; i++) {
        if (node->param.attached.active == 0) {
            break;
        }
        node++;
    }
    if (i < ATTACHED_PART_COUNT) {
        node->param.attached.active = 1;
        node->param.attached.kind = kind;
        return node;
    }
    return 0;
}

void FreeAttachedPartSlot(struct SceneNode *node)
{
    node->param.attached.active = 0;
}

void UpdateAttachedPartPool(void)
{
    struct SceneNode *node;
    int i;

    node = gAttachedPartPool;
    for (i = 0; i < ATTACHED_PART_COUNT; i++) {
        if (node->param.attached.active != 0 && node->param.attached.kind == ATTACHED_PART_MUZZLE_FLASH) {
            UpdateGunMuzzleFlash(node);
        }
        node++;
    }
}
