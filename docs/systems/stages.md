# Stages

A stage is one archive (`DATA2/S00.DAT` to `S25.DAT`) laid over a 32 by 32 grid of square cells, 3,200 units a
side, centred on the origin: X and Z run from -51,200 to +51,200 (`STAGE_CELL_SIZE`, `STAGE_GRID_ORIGIN_OFFSET`,
`src/stage_cull.c`). The grid organises everything: which floor tiles and props are drawn, how finely, which are
hidden or made see-through, and where the floor is. Code specific to one stage hangs on a table of callbacks.
Stages are numbered by their archive. `include/stage_id.h` names the nine the stage select shows by a plate, 12
Executioner's Cove, 14 Bamboo Thicket, 15 Meikyokan, 16 Cherry Blossom Grove, 17 Yagura Point, 19 Dozaemon Moat,
21 Warrior's Dwelling, 24 Yahiro Road and 25 Underground Grotto, and 23, the stage only the opening demo loads;
the others go by their numbers.

## Loading

`LoadCombatStage` (`src/match_setup.c`) calls `LoadStageAndLighting` (`src/stage_env.c`): the archive into the
model arena's stage slot ([files-and-memory.md](files-and-memory.md)), the stage's lights
([lighting.md](lighting.md)), its fog palettes, and stage 4's thrown-item slot. `LoadStageArchive`
(`src/model_arena.c`) reads the sections:

| Section | Kept for | Read by |
| --- | --- | --- |
| 1: stage models | the bout | `RegisterModelTable`, model slot 10 |
| 2: floor collision | the bout | `SetFloorCollisionData` (`src/floor_collision.c`) |
| 3: camera walls | the bout | `gStageCameraWalls`, the battle camera ([camera.md](camera.md)) |
| 4: prop markers | the bout | `InitStagePropMarkers` (`src/stage_prop_hit.c`) |
| 5: extra models (stages 3, 9, 11, 12, 14, 17, 19) | the bout | model slot 11, or the cuttable props on stages 3 and 14 |
| next to last: scene layout | parsing only | `ParseStageSceneLayout` (`src/stage_cull.c`) |
| last: texture group | uploading only | `LoadTimGroup` (`src/tim.c`) |

The loader returns the address of the scene layout as the end of what it keeps, so the layout and the textures,
once parsed and uploaded, are overwritten by whatever the arena loads next. Dozaemon Moat also registers its
billboard models (`InitMoatBillboards`, `src/stage_moat.c`).

## The layout

The scene layout is three lists of halfwords, each ended by -1, which `ParseStageSceneLayout` turns into
records:

1. **Floor tiles**, up to 430 (`gStageTiles`): a cell index (row times 32 plus column) and a mesh index; bit
   15 of the mesh word makes the tile hideable (`SCENE_TILE_FLAG_CONDITIONAL`). A tile becomes a 16-byte
   `SceneTile` (`include/stage_cull.h`) at its cell's corner.
2. **Props**, up to 60 (`gStageObjects`): a mesh index, a position as three 32-bit values in halfword pairs,
   and three rotation words. The props are full scene nodes in the stage's light slot; a comparison of the
   rotation words chooses render type 0 or the world-aligned type 1 ([rendering.md](rendering.md)).
3. **Cell lists**: for each cell that holds anything, its index, then the indices of its tiles and props
   (props numbered after the tiles), the last with bit 15 set; at most 625 entries in all
   (`gStageCellLists`, `gStageCellHeads`). A prop standing over several cells is listed in each.

Parsing ends by clearing the effect pool and then running the stage's init callback.

## Culling

`CullAndEnqueueStageScene`, called from `EndSceneFrame` through `CullStageAndQueueFog`, decides each frame
which tiles and props enter the render queue.

The camera's cell is found from its position, and its heading from its rotation (the GTE turns a forward unit
vector, `ratan2` gives the angle). `gStageVisibilityMask` holds, for each of 64 headings, a 31 by 31 bit mask
centred on the camera's cell, a word per row with the leftmost column in the top bit: a set bit lets that cell
through. The game ships two sets, loaded over the table at the start of a bout: `DATA3/O16.DAT` for the
normal camera and `O17.DAT` for the first-person view (`LoadVisibilityMaskPreset`). The masks are data on the
disc; how they were made is not in the code.

For every cell the mask passes, the cell's list is walked. A tile takes the cell's detail level as its
`lodLevel` and is queued, unless it is hidden (bit 15 of its type, which `HideStageTilesBeyondZ` sets on stage
4's lower level) or hideable in an occluded cell. A prop is collected once, however many cells list it, and the
props are queued after the tiles. The stage's post-cull callback runs last; most draw their stage's weather and
effects there.

## Detail and see-through cells

`gStageCellLodGrid` keeps four bits per cell, rebuilt every frame:

| Bits | Meaning | Set by |
| --- | --- | --- |
| 0 to 1 | Detail level 0 to 2: how many times a tile's faces are split into four | `StampPackedCellGrid`; `SetPackedCellLod` |
| 2 | Semi-transparent | `MarkCellSemiTrans` |
| 3 | Occluded | `MarkCellOccludedIfOpaque` |

`StampPackedCellGrid` (from `BeginSceneFrame`) clears the grid and lays `gCameraCellLodStamp` over the 16 by 16
cells around the camera: level 2 in a disc about two cells round the camera, level 1 in a ring beyond, 0
elsewhere. The opening demo then sets a fixed block of cells to level 2 (`SetPackedCellLod`,
`src/opening_demo.c`); `MarkCellsHighDetailAroundPoint`, which would raise the four cells round a point, is never
called. The battle camera's wall markers and the camera-to-fighter sightlines then mark cells
semi-transparent and occluded ([camera.md](camera.md)). At cull time a hideable tile is skipped in a cell that
is occluded and not semi-transparent, and drawn semi-transparent in one that is both; an ordinary tile is drawn
semi-transparent only where the cell is semi-transparent and not occluded.

The detail level and the faces' size on screen together choose the mip level of a tile's texture
([rendering.md](rendering.md)).

## The floor

`QueryFloorHeight(x, y, z)` (`src/floor_collision.c`) answers what floor lies under a point. The floor section
starts with 32 by 32 halfword offsets from its own start, -1 for an empty cell; a cell holds a count of
polygons, each a flags word (vertex count, a 7-bit surface attribute, and two bits: a level floor with one
height, and one covering the whole cell), then its corners in the cell's own XZ coordinates unless it covers
the cell, then a height or a plane `(nx, ny, nz, d)` in Q12.

A polygon applies when the point is inside it: no edge has the point on its outer side (a 2D cross product).
Its height is the plane solved for Y at the point, `(nx x + nz z + d * 4096) / -ny`. World Y points down, so the
floor wanted is the nearest one at or below the point: the smallest height not above it. If there is none,
the nearest floor above the point is returned instead. A level floor covering the cell answers at once. A plane
with `ny` of 0 is a wall: the query returns -30,000 with a steep normal (`FLOOR_VERTICAL_WALL_HEIGHT`). The
normal and attribute of the floor found are kept for `GetLastFloorNormal` and `GetLastFloorAttr`; a miss returns
`FLOOR_QUERY_MISS`. How fighters use the answer (slopes, ledges, walls, falls) is [fighter.md](fighter.md).

## Cuttable props

Stages 3 and 14 have props a blade can cut (`src/cuttable_prop.c`). Their section 5 holds a model table, a
table of keyframes (a count, then six halfwords a frame: a step in X, Y and Z and three angles) and the prop
markers mark which props can be cut ([combat.md](combat.md) says when a cut happens). `InitCuttableProps`
stands 34 scene nodes over the first 34 stage props. `TriggerCuttablePropFall` turns the stage prop into a
stump mesh and starts its node falling: each frame `UpdateCuttableProps` applies the next keyframe, turned by
the heading of the cut; after the last it switches to a mesh drawn by the fading drawers and fades by 64 of
4096 a frame. On a Continue the stage is reloaded so the props stand again.

## Stage code

`gStageCallbacks` (`src/stage_cull.c`) gives each of the 26 stages an init, a post-cull and a frame-begin
routine, any of them empty; `gStagePreCullCallbacks` adds one run before culling. The routines are in
`src/stage_callbacks.c` unless said otherwise.

| Stages | What runs |
| --- | --- |
| 0 to 8, 10, 22 | Snow and footprint decals (`InitStageSnowAndDecals` and variants); stage 1 adds its waterfall, stage 4 two light glows, stages 10 and 22 animate textures in VRAM |
| 1 | The waterfall: scrolling water textures and splash columns (`src/water_anim.c`) |
| 3, 14 | The cuttable props (14 is Bamboo Thicket) |
| 9 | Three wind-blown cloth strips, one simulated mesh drawn at three places (`src/cloth_strip.c`) |
| 10 | Before culling, hides one of two groups of props by the camera's X (`UpdateStage10ObjectZones`, `src/stage_cull.c`) |
| 11 | Six prop meshes placed at fixed points, and a red light pulsing on the fighters' and the stage's light slots |
| 12 | Executioner's Cove: the sea and shoreline (`src/ocean.c`) |
| 17 | Yagura Point: night props |
| 19 | Dozaemon Moat: rain with puddle splashes (`src/weather.c`), the swaying foliage run by the cloth solver (`src/foliage.c`), and the moat's water (`src/stage_moat.c`): a piecewise floor height, wading billboards, the special light slot |
| 25 | Underground Grotto: a stream |

Snow and rain are particles kept around the camera (192 snow particles, 128 raindrops, wrapped in a 0x8000-unit
box), drawn as packets linked by depth. They, the stage cloth and the foliage drift with the stage's wind: each
stage has a strength and direction in `gStageWindTable` (`src/combat_state.c`), set every frame into cloth
wind channel 2 ([cloth.md](cloth.md)).

## Moving between stages

Twelve stages are joined by portals (`gStageTransitions`, `src/stage_tables.c`); which stages they join and
when a crossing is taken are in [match-rules.md](match-rules.md). For the engine a crossing is a fresh start: the
bout stops presenting frames, re-enters its initial state, and that loads the other stage's archive into the
arena, sets its lights and fog again, and places both fighters shifted by the difference of the two anchors.
Nothing of the old stage is drawn during the change.
