#ifndef TIM_H
#define TIM_H

/* TintVramClut modes. */
#define CLUT_TINT_MODE_RGB       0 /* per-channel Q12 scaling */
#define CLUT_TINT_MODE_LUMINANCE 1 /* grey luminance, then tinted per channel in Q12 */

void LoadTimGroup(unsigned short *cursor, int groupIndex);
void LoadTimGroupWithOffset(const void *cursor, int groupIndex, int xOffset, int yOffset);
void LoadTimGroupWithSeparateOffsets(const void *cursor, int groupIndex, int xOffset, int yOffset,
                                     int clutXOffset, int clutYOffset);
void LoadTimClutGroupWithOffset(const void *cursor, int groupIndex, int xOffset, int yOffset);
void UploadTimToVram(unsigned long *tim, int x, int y, int clutX, int clutY);
int ConvertRgbToLuminance(int red, int green, int blue);

void UploadTimClutToVram(unsigned long *tim, int x, int y);
int IsRendererMonochrome(void);
void SetRendererMonochrome(int monochrome);
void ConvertClutToMonochrome(const unsigned short *source, int count, unsigned short *dest);
void TintVramClut(int mode, int sourceX, int sourceY, int count,
                  int rScale, int gScale, int bScale, int destX, int destY);
void ScaleVramClutInPlace(int vramX, int vramY, int colorCount, int redScale, int greenScale, int blueScale);

extern int gRendererMonochrome;

#endif
