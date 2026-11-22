#ifndef OCEAN_H
#define OCEAN_H

/* What Stage 12's callbacks, the decals, the fighters' feet and the renderer (render.s) ask of the ocean. */
int InitOceanWaveMesh(void);
void UpdateOceanWaveMesh(void);
int SampleOceanWaveEdgeX(int worldZ);
int SampleOceanWaveVelocity(void);
void DrawOceanWaveMesh(void);

#endif
