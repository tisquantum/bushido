#ifndef FIGHTER_TEXTURE_H
#define FIGHTER_TEXTURE_H

void LoadFighterAltTextures(int player);
void LoadFighterBaseTextures(int player, int injuryMask);
void ApplyFighterTextureOverlays(int player, int character, int mode, int injuryMask, void *buffer);

#endif
