#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

/* Rows of gResourceGroupIds that LoadResourceGroup is given. */
#define RESOURCE_GROUP_SLASH_ENEMIES     2 /* characters 19..22 */
#define RESOURCE_GROUP_SELECT_VS_MAIN    3 /* characters 0, 1, 2, 4, 5 */
#define RESOURCE_GROUP_SELECT_VS_AUX     4 /* character 3 */
#define RESOURCE_GROUP_SELECT_STORY_MAIN 5 /* characters 6, 7, 8, 10, 11 */
#define RESOURCE_GROUP_SELECT_STORY_AUX  6 /* character 9 */

void ClearCachedResources(void);
int *LoadResourceGroup(int group, int *dest);
int FindCachedResourceIndex(int resourceId);
const void *GetCachedResourceBase(int resourceId);
unsigned int GetCachedResourceSize(int resourceId);

#endif
