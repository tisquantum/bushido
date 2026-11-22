# Cloth

Fighters' garments, stage 9's cloth strips and the hanging foliage of Dozaemon Moat (stage 19) are mass-spring
cloth, stepped once a frame with the GTE's help (`src/cloth.c`; the fighters' side in `src/fighter_cloth.c` and
`src/fighter_cloth_setup.c`). The renderer draws a cloth part from its simulated points
(`RenderClothSoftbodyNode`, [rendering.md](rendering.md)); the skeleton that carries it is in
[models.md](models.md). The source has no code specific to hair: any part of a character that its cloth weight
list names is cloth.

## Models, points and links

Up to 16 `ClothModel`s (`gClothModels`, `include/cloth.h`) run at once. A model's work buffer holds its
points and then its links:

- `ClothPoint`, 64 bytes: position and velocity in units of 1/128 of a world unit, a weight (`massOrPin`), up
  to eight links out and eight in by index, and a wind weight.
- `ClothLink`, 16 bytes: the rest length, the two ends, a limit of three times the rest, and a stiffness
  divisor (3 for stage cloth; 2 for fighters, whose solver halves without reading it).

A point's weight says what it is:

| Weight | The point |
| --- | --- |
| 0 | Pinned: placed on its rest position on the bone every frame |
| 1 to 256 | Soft-pinned: keeps half its velocity and is pulled toward its rest position by weight/256; springs do not move it |
| -255 to -1 | Simulated, and drawn toward its rest position by the weight's magnitude/256 |
| -256 | Simulated freely |

## A fighter's cloth parts

Which meshes of a character are cloth, and the weight of each of their vertices, is a table per character in
`src/fighter_cloth.c` (`gClothWeightsKannuki` and the others, through `gCharClothMeshLists`): for each mesh of
the character's model in order, runs of a count, a weight and that many vertex indices, ended by -1 (a mesh
with nothing is just -1), the list ended by -2. Vertices not named are free (-256). The weight is stored in
the unused fourth halfword of each vertex in the model itself (`ApplyClothVertexWeights`).

`BuildFighterClothParts` turns each named mesh into a cloth model when the fighter is set up
(`InitClothPartFromMesh`). Every vertex of the mesh becomes a point. A cloth mesh must be a single batch: a
second one is reported to `TriggerDebugTrap` with the string "Multipul Model", which does nothing on a retail
console. The faces become lists of corner indices, written in the arena's free space past its end, and
`InitClothPointsAndLinks` links every pair of corners of every face. No link joins two points that are both
pinned or soft-pinned, and each link runs from the point nearer the pinned points' centre to the one farther
away. The work buffer is the space the character archive's textures occupied before they were uploaded
(`CalcFighterClothWorkSize` reserves it, [models.md](models.md)). The mesh's node becomes render type 13.

Per character, `SetupFighterClothAndColliders` (`src/fighter_cloth_setup.c`) adds up to three limb colliders to
a cloth part (`AddClothLimbCollider`): two points in a limb's space and a radius, carried to world space each
frame (`TransformClothColliders`). For Mikado, for one, two cloth parts each collide with both thighs and the
pelvis.

`EndSceneFrame` rebuilds a fighter's cloth when it is marked dirty (`RebuildFighterClothIfDirty`) and steps
every part (`UpdateAllFighterCloth`). `RequestClothSnapReset`, called when the bout puts the fighters back at
their start positions, makes the next step place every point at its rest position instead of simulating
(`ResetFighterClothPositions`).

## A step

`StepFighterClothModel` (for stage cloth, `StepStageClothModel`, with its own damping and gravity):

1. **Carry.** The cloth's bone has moved since last frame; that movement is added to every free and soft point
   at 75 percent, rising to 100 percent as the movement approaches 500 units a frame. Cloth therefore trails a
   fast move only a little.
2. **Springs.** `SolveFighterClothLinks` computes each link's force once: the separation scaled by
   `(length - rest) / length`, through the GTE. A link stretched past three times its rest on any axis first
   moves its free end to the midpoint and quarters that end's velocity, until it is in range.
3. **Free points.** A point's force is its velocity, plus the wind, plus the forces of its links out, minus
   those of its links in. Below the floor it is pushed up by an eighth of its depth while that depth is at most
   12,800 (100 world units), and by 1,024 when it is deeper, so the push drops from 1,600 to 1,024 there. Near a
   limb collider it is pushed away from both of the collider's points when the sum of its distances to them is
   less than the radius (so the shape tested is an ellipsoid with the two points as foci), by an amount growing
   with the overlap. The new velocity is seven eighths of the force, plus 400 downward; the position moves by
   it and by the carry.
4. **Soft and pinned points.** A soft point keeps half its velocity, takes the wind and is pulled toward its
   rest position (and sags a little in Y by `(256 - weight) * 25 / 256`); a pinned point is placed on its rest
   position.

The solver's working values live on the scratchpad (`ClothScratch`). `SaveScratchpad` and `RestoreScratchpad`
would save and restore the scratchpad around it, but nothing calls them.

## Wind

There are three wind channels (`gClothWindSlots`, `include/cloth.h`): one for each fighter's cloth and one for
the stage, which the stage cloth, the foliage and the snow and rain read. A channel has a strength, an angle,
and an origin that moves by (153, 827) every step across a 16 by 16 gust table (`gClothWindFieldLut`). Each
point reads the table at its position less the origin, so gusts roll across the cloth: stage cloth at (X + Y,
Z + Y) turned into the wind's frame, a fighter's at X and height. The gust times the channel's strength gives the
push along the wind's direction; on a fighter's cloth it is also multiplied by the point's wind weight (its
distance from the pinned centre, up to 256), which the stage cloth stores but does not read. Pinned points take
no wind.

Each frame `ResetClothWindWithJitter` (from `BeginSceneFrame`) sets all three channels to strength 1024 and
moves a shared angle by -8 to +7 units at random; the bout then gives the stage channel the stage's own
strength and direction (`gStageWindTable`, [stages.md](stages.md)).

## Stage cloth

Stage 9's strips (`src/cloth_strip.c`) are one 2 by 7 mesh simulated once and drawn at three places as lit,
double-sided `POLY_GT3` pairs. Dozaemon Moat's foliage (`src/foliage.c`) is an 18-vertex crossed mesh under a
pulsing wind, drawn as `POLY_FT4`s. Both step through `UpdateClothModelsWithOutput`, which writes the points,
divided by 128, to the caller's buffer for drawing.
