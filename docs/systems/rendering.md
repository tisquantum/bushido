# Rendering

The 3D renderer is hand-written assembly: `src/render.s`, with the first-person mannequin's drawers in
`src/pov_tex.s` and `src/pov_flat.s` and the cut props' fading drawers in `src/cuttable_fade.s`. C code queues
what is to be drawn; the assembly turns each queued node into GPU packets linked into the frame's ordering
table. The records the drawers read are in [models.md](models.md); depth and number formats in
[fixed-point.md](fixed-point.md); lighting and fog in [lighting.md](lighting.md).

## Scene nodes and the render queue

Everything 3D is a `SceneNode` (`include/scene_node.h`, 132 bytes; `include/scene_node.inc` for the assembly):
a render type, Euler angles, a local and a world `MATRIX`, a matrix state, a parent, a model slot and mesh
index, a rotation order, a light slot, flags and four words whose meaning depends on the type.

`UpdateNodeWorldMatrix` (`src/scene_math.c`) builds a world matrix on demand: it brings the parent up to date
first, builds the local rotation with `gRotMatrixBuilders[rotOrder]` (skipped when the state is
`SCENE_MATRIX_LOCAL_PRECOMPUTED`, as after pose blending), multiplies by the parent's rotation and moves the
local translation through the parent's matrix (`RotTrans`). Order 0 is `BuildRotationMatrixFast`
(`src/render.s`); the North American and European builds fill only the even orders, the Japanese all six
(`InitRotMatrixDispatchTable`, `src/rot_matrix.c`).

A frame's drawing list is `gRenderQueue` (`src/scene.c`), 512 node pointers reset by `BeginSceneFrame`.
Skeletons, shadows, stage tiles and props, thrown items, Katze's muzzle flash, the moat's wading billboards, the
ocean and the mannequin append themselves; the 3D effects of `src/effect.c` write their packets directly. The queue's order does not decide what is in front; the ordering table does.

## From queue to drawer

`SubmitSceneDisplayList` (`src/display_list.c`) composes `gCombinedViewMatrix` from the main and secondary
camera nodes, builds the shadow projection, and fills the scratchpad header (`RenderScratch`,
`include/render_scratch.h`): the primitive table, the ordering table, the packet bank mask (-1 on odd frames,
0 on even), the far limit from the fog and the near limit for floor tiles. `RenderSceneDisplayList` walks the
queue, keeping the mesh stream in `s0`, the scratchpad in `s1`, the node in `s2`, its light slot in `s3`, the
primitive table in `s4`, the coordinate shift in `s5`, the view matrix in `s6` and the queue in `s7`.

```
gRenderQueue[i] --renderType--> gSceneNodeRenderDispatch      sets up the GTE for the node
  mesh batch word --kind------> gMeshBatchDispatch            projects the batch's vertices
    primitive group --type----> table in RS_primTable         draws the group's faces
```

| Type | Handler | Draws |
| ---: | --- | --- |
| 0 | `RenderStandardSceneNode` | A lit mesh in the node's orientation |
| 1 | `.render_world_scene_node` | A mesh with no rotation of its own (stage props the layout marks so) |
| 3 | `.render_shadow_scene_node` | A planar shadow: the parent's matrix flattened by `ProjectMatrixToShadowPlane` |
| 5 | `.render_reused_matrix_node` | A mesh under the GTE matrices the previous node left |
| 10 | `.render_skinned_scene_node` | A fighter's skinned body, through `DrawSkinnedFighterModel` (`src/otz.c`) |
| 12 | `.render_tile_scene_node` | A run of stage floor tiles, through `RenderStageTileBatch` |
| 13 | `.render_cloth_scene_node` | A cloth, through `RenderClothSoftbodyNode` |
| 14 | `.render_ocean_scene_node` | The sea of Executioner's Cove (stage 12), through `DrawOceanWaveMesh` (`src/ocean.c`) |
| 15 | `.render_pov_overlay_node` | A part of the first-person mannequin |

The other entries draw nothing; the camera nodes are types 0x64 and 0x65.

## What a node handler sets up

The GTE's registers hold 16 bits, and world coordinates do not fit. A handler therefore works relative to the
camera and scaled down. It ORs the absolute values of the node's offset from the camera and picks a shift `s`
of 0, 1 or 2 at 0x4A00 and 0xA500. The offset, shifted right by `s` and rotated by the view, becomes the GTE
translation; the node's rotation times the view's, also shifted by `s`, becomes the GTE rotation. Every vertex
is thereby scaled by 2^-s. Projection divides by depth, so the picture is unchanged; only the depths come out
smaller, and the drawers shift them back. Skinned nodes switch at 0x5A00 and 0xB500; world-aligned props and
shadows always use 2, the ocean and the mannequin 1.

Every view transform also has its second row halved, and here and in `DrawSkinnedFighterModel` its Y
translation too (`UpdateAspectCorrectedCameraMatrix`, `src/stage_callbacks.c`, halves the row only: its
translation is the camera's position): on the 640 by 240 frame a pixel is twice as tall as it is wide.

The handler then loads the light slot: its direction matrix times the node's rotation, its colour matrix, and
its ambient colour as the GTE's back colour.

## Batches

A mesh is one or more batches. A batch's first word holds a count in its low half and a kind in its high half;
a count of 0 ends the mesh.

| Kind | Routine | Vertices | Primitive table |
| ---: | --- | --- | --- |
| 0 | `RenderMeshBatchAlignedVerts` | 8-byte `SVECTOR`s, three per count | `gPrimRenderDispatch` |
| 1 | `.mesh_batch_packed_verts` | 6 bytes, one per count | `gDynamicPrimRenderDispatch` |
| 2 | `.mesh_batch_pov_overlay` | 8 bytes, projected about (560, 60) | `gPovPrimRenderDispatch` |

`TransformVertices8ByteRtpt` and `TransformVertices6ByteRtpt` project three vertices at a time with RTPT into
the scratchpad: packed screen positions from 0x1F800020, depths after them, room for 165 vertices (`MeshScratch`).
`DispatchMeshPrimitiveGroups` calls one drawer per group (count, type, records); a halfword after the groups
says whether another batch follows.

## The drawers

The primitive tables are in `src/otz.c`. Types come four to a family (F3, FT3, F4, FT4, G3, GT3, G4, GT4 at 0,
4, 8 ... 28), the low two bits choosing the lighting: 0 lit, 1 unlit, 2 lit on both sides. Unused entries are
`TrapInvalidPrimType`. A drawer takes one group, one face at a time:

1. **Fetch.** The record's vertex word holds 8-bit indices into the projected vertices.
2. **Reject.** A face whose corners all have negative Y, or all negative X, is off the top or left edge; of
   all the drawers only `DrawSubdivPolyFt4Dynamic` (stage tiles and the sea) also tests the bottom edge, and none
   tests the right. NCLIP gives the winding: a face turned away is skipped, except by the double-sided drawers,
   which invert its normal instead.
3. **Depth.** AVSZ3 averages a triangle's corner depths, and a quad's four are added and divided by 4; a mean
   of 0 or less is skipped, and several drawers also skip faces beyond a limit (`DEPTH_LIMIT`, `DEPTH_LIMIT_G`,
   `include/mesh_prims.inc`). Shifted by `3 - s`, the mean is the true screen depth divided by 8, which is
   compressed to an entry from 0 to 4095.
4. **Light.** NCS (textured), NCCS (flat, times the face colour), NCT or NCCT (Gouraud, a colour per corner)
   turn the face normal into the packet's colour.
5. **Link.** The entry's old link goes into the packet's tag with the packet's length, and the entry points at
   the packet.

Static meshes carry their packets inside the model: each face record holds its GPU packet twice, and the frame's
packet bank mask picks the copy, so the copy the GPU may still be drawing is never rewritten. Drawers of the
dynamic table build packets at `gPacketCursor` in the frame's packet buffer instead, one packet slot per face
whether it is drawn or not. Dynamic types 32 and 33 draw shadows: opaque black flat faces.

## Stage tiles: subdivision and mip levels

Floor tiles are `SceneTile` records (16 bytes, `include/stage_cull.h`) queued by the stage cull
([stages.md](stages.md)). `RenderStageTileBatch` draws every tile that follows in the queue in one call, with
shift 1 and the stage's light slot. A tile has only a grid position; its mesh's 6-byte vertices are projected
and each group goes through `gTilePrimRenderDispatch`, triangles to `DrawSubdivPolyFt3Dynamic` and quads to
`DrawSubdivPolyFt4Dynamic`.

These drawers sort by the deepest corner rather than the mean. A face beyond the last fog step is dropped, and
so is one nearer than entry 0x182 while the near limit is on; it is off in the first-person view and for a
frame when a story scene asks (`DisableStageTileNearClip`, `src/tile_clip.c`). A tile's `lodLevel`, 0 to 2, is
how many times each face is split into four: `SubdividePolyFt3Recursive` and `SubdividePolyFt4Recursive`
recurse on scratchpad frames, averaging corners and texture coordinates (`AverageVertexAndUv`), projecting the
leaves and chaining all of a face's pieces into its one ordering-table entry.

The same drawers pick a mip level by `gStageRenderFlags`, which every stage sets to "area less detail" with
triangles included (`InitStageRenderEnvironment`, `src/stage_env.c`). The face's screen area from NCLIP gives
level 2 below 512, 1 below 1,408 (quads: 1,024 and 2,816), else 0; the tile's subdivision depth is subtracted,
never going below 0. Level `k` halves the face's texture coordinates `k` times: a stage texture's smaller copies
lie in its own texture page at the halved offsets, and the animated water scrolls all three levels
(`UpdateWaterTextureScroll`, `src/water_anim.c`). Flag bit 2 (`STAGE_RENDER_DEBUG_MIP_TINT`) tints triangles
by mip level, white, yellow or orange, and only the quads that are split, cyan when split once and blue when
twice; no code sets it.

## Skinned fighters

A fighter's body is one skinned mesh drawn by `RenderSkinnedSkeletalMesh` with `gSkinnedPrimRenderDispatch`.
`PrelightSkinnedMeshNormals` first lights all its normals, a bone's group at a time under that bone's rotation,
into `gSkinnedPrelitColors`; faces then fetch their corners' colours by 10-bit index, so a shared vertex is
lit once. Vertices are then placed bone by bone through 13 bone matrices, and seam lists join one bone's
faces to the next's. The layout is in [models.md](models.md).

## Special cases

- **Shadows.** `BuildShadowProjectionMatrix` (`src/shadow_proj.c`) shears along player one's primary light,
  x' = x - (lx/ly)y, z' = z - (lz/ly)y, y' = 0. A shadow node flattens its parent's matrix onto the height in
  its `shadowFloorY` and is drawn with the shadow model's black faces.
- **First-person mannequin.** Its 15 parts (`src/pov_overlay.c`) are type-15 nodes whose drawers link into a
  256-entry table of their own, `gOverlayOt`. That table is chained at entry 0 of the frame's table, so the
  mannequin is drawn last, over the scene, in its own depth order.
- **Cut props.** A severed top's faces go through dynamic types 42 and 43 (`DrawFadingPolyFt3`,
  `DrawFadingPolyFt4`, `src/cuttable_fade.s`), which scale the lit colour by the node's fade.
- **Cloth.** `RenderClothSoftbodyNode` skips the mesh's rest vertices and projects the simulated points
  (stored in units of 1/128); faces go through `gClothPrimRenderDispatch`, double-sided, and the two
  flat-textured drawers bias the depth by 80 by side (`CLOTH_SIDE_DEPTH_BIAS`); the Gouraud ones do not.
- **The ocean.** The sea of Executioner's Cove (stage 12, `src/ocean.c`) is a 9 by 19 grid of wave points
  rebuilt each frame and drawn by `DrawSubdivPolyFt4Dynamic`.
- **Effects.** Billboards (`src/billboard.c`), 3D effects (`src/effect.c`), weather (`src/weather.c`), footprints
  (`src/decal.c`), slash trails (`src/weapon_trail.c`) and the opening's ribbons (`src/ribbon_trail.c`) write
  packets at `gPacketCursor` and link them by depth themselves, through `CompressDepthToOtz` or
  `InsertDeferredPrimsIntoOt` (`src/stage_callbacks.c`). The trails are strips of semi-transparent `POLY_GT4`,
  subdivided by camera distance ([effects.md](effects.md)).

## The 2D layer

Menus, the HUD and text are textured rectangles (`DrawTexturedRectList`, `src/sprite2d.c`): groups of
`POLY_FT4` sharing one 16-byte `GlyphTexture` (`include/sprite2d.h`), linked at a chosen entry, usually 0, in
front of everything. `DrawFormattedText` (`src/font_hud.c`) turns a string into such a list, a rectangle a
glyph, with `#` codes for position, tint, scale and font bank and `%s` and `%x` for strings and hexadecimal.
A full-screen fade is one semi-transparent `POLY_F4` with its own `DR_MODE` (`src/screen_fade.c`); the menus'
backdrop is a `POLY_G4` (`src/gradient_bg.c`).
