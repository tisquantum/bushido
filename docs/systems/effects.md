# Effects

Sparks, blood, dust, splashes, footprints and trails are drawn outside the scene-node renderer: each kind keeps its
own pool, builds its own GPU packets at `gPacketCursor` and links them into the ordering table by depth itself
([rendering.md](rendering.md)). Nearly all are started by the combat and frame events of
[combat-events.md](combat-events.md) or by a stage's own routines ([stages.md](stages.md)).

| Pool | Holds | Unit | Size |
| --- | --- | --- | --- |
| `gEffectPool` | 3D effects: sprite sheets, splashes, debris, blood, spray | `src/effect.c` | 24 `EffectSlot`s |
| `gBillboardEffects` | Wound and clash billboards; a delayed sound | `src/billboard.c`, `src/billboard_driver.c` | 16 |
| `gGroundDecals` | Footprints | `src/decal.c`, `src/decal_driver.c` | 16, a ring |
| `gWeaponTrails` | Blade slash trails | `src/weapon_trail.c` | 3 per fighter |
| `gRibbonTrails` | The opening's ribbon trails | `src/ribbon_trail.c`, `src/ribbon_driver.c` | 10 |

A full pool drops the new effect: `AllocEffectSlot` and `SpawnBillboardEffect` return nothing when every slot is
taken. `EndSceneFrame` (`src/scene.c`) runs the billboards and then the 3D effects after the stage is culled; the
decals run from the stage's post-cull routine, as does the weather ([weather.md](weather.md)). The 3D effects,
billboards, decals and ribbons halve the Y row of the view matrix, because the 640 by 240 screen's pixels are twice
as tall as wide (`UpdateAspectCorrectedCameraMatrix`, `src/stage_callbacks.c`). Distances are world units and Y
grows downward.

## 3D effects

An `EffectSlot` (`include/effect_slot.h`, 44 bytes) holds a state (free, active, or updated but not drawn), a
position, a mesh, an update routine, two counters and a union of per-kind parameters: a sprite's frame count and
rate, a particle's velocity in sixteenths of a unit, or a blood emitter's angles. Each frame `UpdateAndDrawEffects`
calls every slot's update and, if the slot is active, `DrawEffectMesh`.

**The mesh stream.** An effect mesh (`EFFECT_MESH`) is a header (mode flags and a position), a vertex list, and a
quad list of which `quadCount` quads from `quadStart` are drawn, each quad a ready `POLY_FT4` tint, its UVs,
texture page, palette and four vertex indices. Meshes may be chained, bit 31 of the flags ending the chain.
`SetupEffectTransform` puts the mesh's position in camera space, scaled down by 2 or 4 when it is farther than
0x5A00 or 0xB500 on any axis so the offsets fit the GTE's 16 bits; a billboard mesh (mode 0) gets an identity
rotation with Y halved, so its quad always faces the camera, and a world mesh (mode 1) the camera's rotation.
Billboard quads are sorted 500 depth units nearer than their anchor. Most updates animate by moving `quadStart`
through the frames of a sprite sheet; meshes are shared by every slot of their kind, so each update writes its
slot's position into the mesh just before it is drawn.

**Sprite effects.** `SpawnEffect(type, x, y, z)` starts one of the fourteen types of `gEffectTypeTable`; most play
a sheet once and free the slot (`UpdateSpriteEffect`):

| Type | Mesh | Frames x ticks | Quad | Started by |
| ---: | --- | --- | --- | --- |
| 0 | `gParrySparkMesh` | 6 x 1 | 900 square | guard parry and guard break |
| 1 | `gWallHitSparkMesh` | 5 x 1 | 900 square | a weapon or thrown item striking scenery |
| 2 | `gDustPuffMesh` | 10 x 1 | 300 square | foot dust events, a bullet striking scenery |
| 3 | `gBladeLockFlashMesh` | own update | 600 square | the blade lock |
| 4 | `gUnusedEffect4Mesh` | 8 x 1 | 400 square | nothing |
| 5, 6 | `gGunMuzzleFlashP1Mesh`, `gGunMuzzleFlashP2Mesh` | 6 x 1 | 200 by 300 | Katze's shot, by side |
| 7 | `gUnusedEffect7Mesh` | 6 x 1 | 400 by 400 | nothing |
| 8 | ring and splash | own update | | a fighter entering Dozaemon Moat's water |
| 9 | `gWaterSurfaceSplashMesh` | 5 x 3 | 600 by 600 | a blade or item breaking that water |
| 10 | `gWaterRippleEffectMesh` | own update | 800 square | a fighter wading in the moat, every eighth frame (`src/stage_moat.c`) |
| 11 | `gWaterFootstepSplashMesh` | 5 x 2 | 400 by 500 | footsteps on splashing ground |
| 12 | `gSprayParticleMesh` | 7 x 1 | 600 square | nothing through `SpawnEffect` |
| 13 | `gSnowFootstepPuffMesh` | 5 x 2 | 400 by 500 | footsteps on stage 4's splashing ground |

The muzzle flash is also a model: `SpawnGunMuzzleFlash` (`src/muzzle_flash.c`) attaches a node to Katze's right
hand that shows meshes 16 to 19 of his model, one a frame, and places sprite type 5 or 6 at an offset of (-38, 138,
91) from the hand.

**Water.** Type 8 (`SpawnWaterSplashEffect`) takes two slots or none: a world-space ring of nine vertices at the
water surface (`MOAT_WATER_SURFACE_Y`, 1200) whose radius closes an eighth of the way to 3,000 each frame while its
shade falls from 128 to 0 over 30 frames, and a 2,400 by 1,600 billboard 500 above it showing eight frames, one
every second frame. Type 10 (`SpawnWaterRippleEffect`) is a flat 800-unit square of four quads stepping through
four frames, two frames each.

**The blade lock flash.** `SpawnBladeLockFlashEffect` draws two chained billboards (one sheet each) for three
frames, shown only on even ticks, so it flickers.

**Debris.** A thrown burst of surface debris (`SpawnParticleBurst`, from a thrown sub-weapon of id 8 to 11,
[weapons.md](weapons.md)) starts eight ballistic particles in four rings of two: pitch 320 and 512 for the first
ring, 64 more per ring (in 4096ths of a turn), speed 800 rising by 720, all turned to the throw's heading. They
start 800 below the item and each falls under gravity 180 (in sixteenths) a frame, steps its five-frame sheet
every four frames and holds the last, and is freed when it falls back past its starting height. The four kinds
(0 to 3, the sub-weapon id less 8) share one sheet under four palettes (`gSurfaceDebrisParticleMeshes`); kind 0
is what stage 4's snow gives.

**Blood.** A fatal hit calls `SpawnBloodSpray(position, direction)`, the direction being the reverse of the
attacker's blade tip velocity (`gBloodSprayDir`, [combat.md](combat.md)). It takes one slot as an emitter that is
updated but never drawn: every third frame it rotates the three velocities of `gBloodSprayLocalVelocities` (three
directions about 11 degrees off the axis, speed about 2,560) into the spray's direction and starts three blood
drops, twice, then frees itself. A drop (`UpdateBloodDropParticle`) falls under gravity 140 and plays a five-frame
sheet, four frames each. Its tint is chosen when the spray starts: `gDefaultBloodColor` (the record carries the
text "BLOOD COLOR"), which is 0xE0, 0, 0 opaque in the North American and European builds and 0xA0, 0x80, 0
semi-transparent in the Japanese, or a semi-transparent grey 0x74 when the monochrome option is on.

**Spray.** `SpawnDirectionalSpray(kind, position, direction, scale)` turns the velocities of one of the
`gSprayVelocityTables` into a direction and starts a particle for each. The kinds (`SprayParticleKind`):

| Kind | Started by | Particles | Gravity | Ends |
| --- | --- | ---: | ---: | --- |
| 1 stream | Stage 25, each foot lower than 1800, every fourth frame | 1 | 200 | after 6 frames of 2 ticks |
| 2 waterfall | Stage 1's two columns every fourth frame, and `src/water_anim.c` | 1 | 140 | after 7 frames of 2 ticks |
| 3 water surface | nothing | 3 | 200 | below the water surface |
| 4 shoreline | Executioner's Cove, each foot on the low-X side of the wave edge (`SampleOceanWaveEdgeX`), every fourth frame, scaled by the wave's speed | 1 | 200 | below height 400 |

Stream and shoreline spray is drawn opaque, the others semi-transparent.

**Stage 4's light glows.** `SpawnStage4LightGlows` places two 1,600-unit glows at fixed points. Their brightness is
the camera's distance beyond 6,000 (halved, at most 4096) times a flickering 64 to 79, over 4096; nearer than 6,000
they are not drawn.

`ClearEffectPool` empties the pool when a stage is set up, at the replay, and when the story states reset the
scene.

## Billboards

`SpawnBillboardEffect(kind, position)` takes the first free of 16 slots; `UpdateAndBuildBillboardEffects` calls the
kind's handler in `gBillboardEffectHandlers`, which builds one camera-facing `POLY_FT4` from the kind's
`SpriteDesc` (width, height, ticks, colour word, UV frames, texture, a Y offset), then
`UpdateAndDrawBillboardEffects` (`src/billboard_driver.c`) links them 200 depth units nearer. A billboard is turned
only about the vertical axis, to face the eye along the ground.

`InitBillboardEffects` loads the common effect texture file (`LoadCommonEffectTextures`, `src/model_arena.c`)
and builds each kind's texture page and palette from `gEffectTextureSources`, then each frame's UVs from the
frame size and sheet layout (`BuildEffectTextureUVs`). Only six kinds are given a texture, and five of them are
spawned; kind 15 is spawned too but draws nothing:

| Kind | Used for | Size | Ticks |
| ---: | --- | --- | ---: |
| 2 | a limb wound (six frames, the later ones held) | 400 | 12 |
| 4, 6 | a fatal hit, two together | 800 | 6, 8 |
| 5 | a weapon clash | 800 | 3 |
| 7 | a torso wound | 800 | 8 |
| 15 | a delayed sound, for a cut prop | none | 2 |
| 3 | nothing: its texture is the parry spark's sheet under palette row 511, which colours it orange (the parry spark's row 510 colours it green) | 800 | 6 |
| 1, 8 to 14 | nothing (no texture) | | |

Kind 15 draws nothing: two frames after it starts, `UpdateDelayedAmbientTrigger` plays the stage's first ambient
sound from its position and frees the slot ([oddities](oddities.md)).

## Footprints

A footstep frame event lays a footprint (`SpawnFighterFootprint`, `src/decal_driver.c`): a 200-unit square at the
foot, turned to the foot's heading, in a ring of 16 (`SpawnGroundDecal`, `src/decal.c`), so the seventeenth
replaces the first. The texture is a 2 by 2 atlas of 8-texel tiles (`InitGroundDecalAtlas`): fighter id 0 uses
tiles 0 and 1 (left, right), every other fighter tiles 2 and 3. A footprint lasts 300 frames, drawn subtractively
so it darkens the ground, at full strength until its last 128 frames and then fading; on Executioner's Cove one on
the low-X side of the wave edge fades nine times as fast. A print is drawn only when its projected winding has
one sign (`gte_nclip`), so it is one-sided, and buckets nearer than the floor tiles' clip are dropped. The stages
with snow and Executioner's Cove set the atlas up (`InitStageGroundDecals`); on the others nothing draws the ring.
`UpdateFootDecalContact`, which would lay a print when a foot rises above height -200, is never called.

## Slash trails

Each fighter has one trail per blade edge pair of its weapon (`gWeaponTrailCount`, `gWeaponTrailEndpoints`,
`src/fighter.c`): two for the Nodachi and weapon 9, none for the gun ([weapons.md](weapons.md)).
`DrawAllWeaponTrails` (`src/weapon_trail.c`) runs after the collisions and in the replay; a trail is a ring of
eight edges (a hilt and tip point each). Every frame `DrawWeaponTrail` writes the new edge two places ahead and
fills the place between with a point smoothed from the edges before and after (`SmoothTrailPoint`: the turn between
the two segments weights them), so the ring advances by two. It draws the newest six segments as semi-transparent
`POLY_GT4`s, additive, shading from 128 at the newest edge to 0 at the oldest; when the clip is flagged
`ANIM_FLAG_RED_TRAIL` the colour is red (up to 255 with green and blue at half), grey in monochrome. Across the
blade each segment is cut into 8, 4, 2 or 1 slices as the blade's middle lies within 500, 1,000, 2,000 or 4,000 of
the camera (`TrailSubdivisionLevel`). The texture is a 64-texel strip at row 200 of the fighter's texture page, at
X 0x300 for player one and 0x3C0 for player two, split along the blade between the fighter's trails; the
first-person opponent's trail uses the palette at (0x40, 0x1FF) instead.

No trail is drawn during a first-person bout (its replay draws them), and a dead or defeated fighter's trails are
reset.

The opening's ribbon trails (`src/ribbon_trail.c`) are the same method on ten slots fed a hilt and tip each frame,
from a bone (`UpdateRibbonTrailFromBone`) or directly, always white ([opening.md](opening.md)). A thrown item's
trail is simpler: one gouraud quad from its last position to its current one, the item's `trailRgb` fading to
black, and a flat quad at the head (`DrawSubWeaponTrail`, `src/subweapon.c`).

## Limits

Effects that find their pool full are lost without trace; the 24 3D slots are shared by blood, dust, splashes,
debris and spray, and a fatal hit alone takes up to seven. Each effect is drawn only where its depth falls inside
the ordering table. What nothing spawns: sprite types 4, 7 and 12, spray kind 3, billboard kinds 1, 3 and 8 to 14,
and `SpawnUvAnimEffect` and `SpawnBodyJointUvEffects`, which would place a small UV-scrolled sheet at twelve of a
fighter's joints.
