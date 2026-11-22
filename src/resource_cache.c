/*
 * The resource cache: character-file groups loaded back to back into one buffer, each file's
 * base and byte size remembered by resource id for later lookups.
 */
#include "asset_load.h"
#include "resource_cache.h"

#define RESOURCE_GROUP_END       0xFF
#define RESOURCE_GROUP_COUNT     7
#define RESOURCE_GROUP_MAX_ITEMS 8
#define RESOURCE_CACHE_CAPACITY  10

int gCachedResourceCount = 0;     /* files cached since the last clear */
int gResourceCacheUnusedWord = 0; /* 4 bytes; nothing reads or writes them */

void ClearCachedResources(void)
{
    gCachedResourceCount = 0;
}

/* Groups of character-file ids, each ended by RESOURCE_GROUP_END. Nothing loads groups 0 and 1. */
unsigned char gResourceGroupIds[RESOURCE_GROUP_COUNT][RESOURCE_GROUP_MAX_ITEMS] = {
    { 0, 1, 2, 3, 4, 5, RESOURCE_GROUP_END },
    { 6, 7, 8, 9, 10, 11, RESOURCE_GROUP_END },
    { 19, 20, 21, 22, RESOURCE_GROUP_END },
    { 0, 1, 2, 4, 5, RESOURCE_GROUP_END },
    { 3, RESOURCE_GROUP_END },
    { 6, 7, 8, 10, 11, RESOURCE_GROUP_END },
    { 9, RESOURCE_GROUP_END },
};
static int gCachedResourceIds[RESOURCE_CACHE_CAPACITY];
static const void *gCachedResourceBases[RESOURCE_CACHE_CAPACITY];
static unsigned int gCachedResourceSizes[RESOURCE_CACHE_CAPACITY];

int *LoadResourceGroup(int group, int *dest)
{
    unsigned char *ids = gResourceGroupIds[group];
    int resourceId;
    int byteCount;

    while ((resourceId = *ids++) != RESOURCE_GROUP_END) {
        LoadCharacterFile(resourceId, dest);
        gCachedResourceIds[gCachedResourceCount] = resourceId;
        gCachedResourceBases[gCachedResourceCount] = dest;
        /* Archive word 0 is the section count; word (sectionCount + 1) is the total byte size. */
        byteCount = dest[dest[0] + 1];
        dest += byteCount / 4;
        gCachedResourceSizes[gCachedResourceCount] = byteCount;
        gCachedResourceCount++;
    }
    return dest;
}

int FindCachedResourceIndex(int resourceId)
{
    int i;

    for (i = 0; i < gCachedResourceCount; i++) {
        if (gCachedResourceIds[i] == resourceId)
            return i;
    }

    return -1;
}

const void *GetCachedResourceBase(int resourceId)
{
    const void *base;

    resourceId = FindCachedResourceIndex(resourceId);
    if (resourceId < 0)
        base = 0;
    else
        base = gCachedResourceBases[resourceId];

    return base;
}

unsigned int GetCachedResourceSize(int resourceId)
{
    unsigned int byteCount;

    resourceId = FindCachedResourceIndex(resourceId);
    if (resourceId < 0)
        byteCount = 0;
    else
        byteCount = gCachedResourceSizes[resourceId];

    return byteCount;
}
