# Lighting and fog

All lighting is done by the GTE's colour pipeline: per face (or per corner) the drawers run NCS, NCCS, NCT
or NCCT on the face's normal ([rendering.md](rendering.md)), which needs three matrices and a colour that this
page describes. Fog is not done by the GTE at all; it is a palette swap.

## Light slots

`gLightSlots[8]` holds eight `LightSlot` records (`include/light.h`, 96 bytes). Each has three directional
lights (`directions[3]`: pitch, yaw, on or off), the GTE light matrix built from them (`lightMatrix`), the
light colour matrix (`colorMatrix`, column `i` the RGB of light `i`, 4096 for full), an ambient colour
(`ambientRgb`, 0 to 255, loaded as the GTE's back colour) and a length `scale` (4096 by default).

| Slot | Used for |
| ---: | --- |
| 0, 1 | The two fighters (`LIGHT_SLOT_P1`, `LIGHT_SLOT_P2`); a fighter's joints and clones carry its side as `lightSlot` |
| 2 | Rebuilt each frame by the opening demo and story cutscenes |
| 4 | The stage: tiles, props and the stage's effects (`LIGHT_SLOT_STAGE`) |
| 6 | The special slot (`LIGHT_SLOT_SPECIAL`), Dozaemon Moat (stage 19) only |
| 3, 5, 7 | Initialised; no node uses them |

`InitLightSlots` (`src/light_init.c`) gives every slot light 0 at 0xE00 grey (0.875), pitch 0xE00 and yaw
0xA00, the other two lights off, ambient 48. `RebuildLightDirectionMatrix` turns each light into a row of the
light matrix:

    row i = scale * ( -sin(yaw) cos(pitch),  sin(pitch),  -cos(yaw) cos(pitch) )     (zero when off)

with angles of 4096 to the turn ([fixed-point.md](fixed-point.md)). `BeginSceneFrame` rebuilds slots 0, 1
and 4 every frame.

Light 0 is the stage's light, light 1 the point light below, light 2 an auxiliary light; no table in the source
turns light 2 on.

## How a node is lit

The renderer's node handlers (`src/render.s`) load the GTE for each node: the light matrix is the slot's light
matrix times the node's world rotation (`MulMatrix3x3Fast`), so the mesh's normals, stored in model space,
need no transform; the colour matrix and back colour are the slot's. Nodes drawn without a rotation of their
own (world-aligned props, the ocean, stage tiles) take the slot's light matrix as it is. A skinned body is lit
once per bone group before any face is drawn (`PrelightSkinnedMeshNormals`), with the light matrix times the
bone's rotation ([rendering.md](rendering.md)).

Code that draws outside the renderer loads a slot itself: stage 9's cloth strips call `ApplyLightSlotToGte`
(`src/stage_cull.c`), which loads the stage slot into the GTE and also lights an upward normal into
`gStageLitColor`, a colour nothing reads.

## Stage lighting

`ApplyStageLighting` (`src/stage_light.c`) sets the slots when a stage loads, from one of two tables, day or
night (`gStageLightPresetsDay`, `gStageLightPresetsNight`, 27 rows: the 26 stages and the select scene). The
day or night choice is the Options menu's daytime setting, passed in through `SetRendererDaytime`
(`src/options_ui.c`, `src/select_scene.c`, `src/opening_demo.c`). A row has
two `SceneLightPreset`s, one for the fighters and one for the stage, each a light colour and an ambient
colour in HSV, in Q12, and the light's pitch. `SetLightSlotFromHsvPreset` converts both colours with
`ConvertHsvToRgbQ12` (the light colour into column 0 of the colour matrix, the ambient scaled to 0 to 255)
and rebuilds the directions. All three slots share one yaw per stage, `gStageLightYaws`: 0x180 for most
stages, 0x400 for stage 12 (Executioner's Cove), 0x200 for stage 20, 0xA00 for the select scene.

By day, when a fighter's fog is on (below), both fighters take the stage's preset instead of the fighters'.

The stage's clear colour and the GTE's far colour are set together (`SetStageClearAndFogColor`): by day from
`gStageSkyClearRgb`: a grey 0x9D9D9D on most stages, black on stages 11, 15, 18, 20 and 23 and the select scene,
0x8B3006 on Executioner's Cove (12), 0x222424 on Dozaemon Moat (19), 0x140400 on Yahiro Road (24) and 0x0E0014 on
Underground Grotto (25); by night black.

## The special slot

On Dozaemon Moat (stage 19), a fighter whose pelvis and head are both below a set depth in the water is switched
to slot 6 (`UpdateMoatFighterEffects`, `src/stage_moat.c`, through `SetFighterUseSpecialLight`). The slot is
filled from `gSpecialLightPreset` (`src/stage_light.c`), an array of seven ints that `SetLightSlotFromHsvPreset`
reads as preset words. Those words are ints in Japan and shorts elsewhere (`SceneLightValue`,
`include/scene_light.h`). Japan therefore reads no direct light, a dim ambient (H 1683, S 3384, V 725) and pitch
3650; the North American and European builds read zeros for both colours and 1683 as the pitch, so a fighter
under the special slot there has no light at all.

## The point light

Hits, clashes, parries and the blade lock flash a coloured light on both fighters (`src/point_light.c`).
`TriggerPointLightFlash` takes one of 20 presets in `gPointLightFlashPresets`, each a start and end colour in
Q12 and a fade rate (rows 0 to 14 for the billboard effect kinds, 15 to 19 for the 3D effects through
`TriggerAltPointLightFlash`; a negative rate marks an unused row). Each frame `UpdatePointLight` blends the
colour from start to end (`LoadAverageShort12`, progress in Q12, advancing by the rate until 4096) and, for
each fighter that registered as a target this frame (`RegisterPointLightTarget`, from
`UpdateAndEnqueueFighterPose`), points that slot's light 1 from the flash to the fighter: yaw `ratan2(dx, dz)`,
pitch from the height difference over the horizontal distance. A fighter more than 5,000 units from the flash
(the squared length against 25,000,000, measured only while each delta is under 0x7000) gets no light 1; one
with a delta of 0x7000 or more is lit however far it is ([oddities.md](oddities.md)).

## Depth cueing

No drawer uses the GTE's depth cueing (DPCS, NCDS and their kind): the far colour register is set only so that
the fog can read it back (`GetGteFarColor`, `src/render.s`).

## Fog by palette swap

Stage textures take their palettes from one VRAM row, 320 entries at (0, 0x1E0). At load time
`BuildFogClutTable` (`src/fog.c`) reads that row, and writes 63 copies of it to VRAM, step `k` blended
`k/62` of the way to the far colour in Q12, palette entry 0 (transparent) left alone; the last step is the far
colour itself. For each step it prepares a `DR_MOVE` that copies the step's row back over (0, 0x1E0). The
steps fill the rest of rows 0x1E0 to 0x1FF, two to a row.

Each frame `QueueStageFogClutMoves`, from `CullStageAndQueueFog` (`src/stage_env.c`), links those moves into
the ordering table at increasing depths. The fog starts at the camera's distance from its target (in the
opening, its distance from the origin) and spreads over the smaller of 32,000 units (`gStageFogDepthSpans`)
and what is left before 48,000; the steps are evenly spaced, and the last, the full fog colour, is linked at
entry 4091. The ordering table is drawn from the back, so the GPU copies the full-fog palette first, then ever
less fogged ones as it moves forward, and each primitive is drawn with whichever copy was last moved into the
row. The depth of the last spaced step is also the renderer's far limit for floor tiles
(`GetFogFarOtzBucket`): tiles beyond it would be drawn in the fog colour, which is the clear colour, and are
skipped. Stage 23, the opening demo's, has no fog (`gStageFogConfigs`).

One fighter can be fogged the same way. In the first-person view the opponent's fog is turned on
(`EnableCharFogSlot`, from `src/menu_state.c`): its character and weapon archives are loaded with their
palettes moved to the block at (0, 0x1F8), 80 by 8 (`OffsetModelSlotTPageClut`, `LoadCharacterArchive`,
`src/model_arena.c`; the fogged weapons come from `DATA1/H13.DAT` to `H18.DAT`), the stage gets 31 steps
instead of 63 to make room in VRAM, and that block gets 15 steps of its own, moved at the same depths.

## Monochrome

The monochrome option (`gRendererMonochrome`, `SetRendererMonochrome`, `src/tim.c`) acts when things are set
up, not in the drawers: palettes turn grey as they are uploaded, `ConvertHsvToRgbQ12` drops the saturation of
every light colour, and the clear colour, text tints and flashes use the luminance of their colours
(`ConvertRgbToLuminance`). The first-person mannequin has a grey model of its own on the disc (`DATA5/T1.DAT`).
