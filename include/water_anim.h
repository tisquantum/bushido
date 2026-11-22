#ifndef WATER_ANIM_H
#define WATER_ANIM_H

/* What the stage callbacks call of the water animation: VRAM texture scroll and frame swaps,
 * and Stage 1's waterfall splashes. */
void ResetWaterScrollOffset(void);
void UpdateWaterTextureScroll(int index);
void UpdateStage1WaterfallSplashes(void);
void QueueAnimatedVramTextures(int animIndex, int slot);

#endif
