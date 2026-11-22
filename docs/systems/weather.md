# Weather

Snow falls on eleven stages (0 to 8, 10 and 22) and rain on Dozaemon Moat. Both are particles kept in a box
around the camera rather than placed in the stage, moved and drawn by `src/weather.c` from the stage's post-cull
routine (`src/stage_callbacks.c`, [stages.md](stages.md)), with their state in a 0x15F0-byte work area that
`gParticleWork` points to (`gSnowWorkStorage` or `gRainWorkStorage`). They drift with the stage wind:
`gWindState` points at cloth wind channel 2, which `gStageWindTable` sets each frame ([cloth.md](cloth.md)).
Distances are world units and Y grows downward. The drops and flakes are linked straight into the ordering table
by depth, like the other effects ([effects.md](effects.md)).

## Snow

The stages with snow (0 to 8, 10, 22) call `InitSnowSystem` with the texture at (0x280, 0x100). 192 flakes are kept
in a box 0x8000 units wide around the camera; each frame `UpdateAndDrawSnow` moves the first `density x 4` of them
(Stages 0, 2, 4 to 8 and 22 pass 0x40, so all 192; Stage 1 0x20, Stage 10 0x10, Stage 3 4): down 16 to 23 units, a
random step of -8 to 7 across, the stage wind ([stages.md](stages.md)), less the camera's own motion. A flake that
was off screen last frame falls 1,000 a frame instead, and one below the stage's snow floor (`gStageSnowGroundY`)
starts again 6,000 above the camera at a random place. A flake off screen is also tried at its positions wrapped
half the box along X, Z or both. At most 48 are drawn, as semi-transparent rectangles 2 by 2 to 16 by 8 pixels by
depth (`gSnowQuadExtents`), from two packet sets used on alternate frames.

`InitStageSnowIfEnabled` restarts the snow when a bout starts and when the replay, the victory scene or the story
states reset the scene, but only on Stages 0, 1, 3, 4, 5 and 10; on 2, 6, 7, 8 and 22 the flakes carry on
([oddities](oddities.md)).

## Rain

Dozaemon Moat has rain (`InitRainParticles`): 128 drops as gouraud lines 720 units long from `gRainColorTop`
(0x40, 0x60, 0x90) to `gRainColorBottom` (0xA0, 0xA0, 0xC0), or their luminance in monochrome. A drop falls 720
a frame with the wind (2,880 before it is first drawn), and where it meets the floor (`GetMoatFloorHeight`) it
becomes a 200-unit splash showing three 16-texel frames, one a frame, then waits. A waiting drop is restarted
with a chance of 1 in 8 a frame while a counter of tries (`restartTries`) is below 128; each waiting drop tried
adds one, and a frame that draws fewer than 64 drops sets it to the number drawn. A drop starts 4,096 to 5,119
above the camera, within an eighth of a turn either side of the camera's heading and up to 16,383 away, set
upwind by four times the wind.
