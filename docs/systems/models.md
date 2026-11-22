# Models and animation

Garments are not animated but simulated as cloth ([cloth.md](cloth.md)). The motion graph that chooses which
animation plays is the fighter's ([fighter.md](fighter.md)); the archives' outer layout is in
[formats.md](../formats.md).

## Model tables

A model is reached as `gModelTables[slot][mesh]` (`src/model_table.c`): 20 slots, each pointing at an
archive's table of mesh offsets. `RegisterModelTable` turns the offsets into addresses in place and sets bit 15
of the table's count (`MODEL_TABLE_RELOCATED`) so a table registered twice is not moved twice;
`RelocateModelTable` shifts every pointer when the arena moves the archive.

| Slot | Holds | Registered by |
| ---: | --- | --- |
| 4, 5 | Player one's and two's character meshes | `LoadCharacterArchive` |
| 6, 7 | Their shadow meshes | `LoadCharacterArchive` |
| 8, 9 | Their weapons | `LoadWeaponArchive` |
| 10, 11 | The stage's models and its extra models | `LoadStageArchive`, `InitCuttableProps` |
| 15 | The first-person mannequin | `LoadPovOverlayModel` |
| 16, 17 | The select scene's weapons and hands | `src/select_scene.c` |

## The model arena

Characters, weapons and the stage live in `gModelArena` (`src/model_arena.c`), 167,936 bytes, as five groups:
each fighter's character, each fighter's weapon, and the stage (`ArenaGroup`, `include/model_arena.h`). Groups
are packed one after another in load order. `LoadArenaGroup` returns at once when the group already holds the
requested resource. Reloading the last group, or the only one, reuses its place; reloading one in the middle
slides every later group down over it (`CopyWordsFast`), fixes their pointers (`RelocateArenaGroup`: model
tables, override tables, cloth buffers, the floor, camera walls and prop markers) and loads the new resource at
the end. After each load the eight characters "MdlTable" are written at the end of the used space, and the
canary at the end of the effect texture buffer is checked. Where the arena lies in memory is in
[files-and-memory.md](files-and-memory.md).

A character archive (`CharacterArchiveHeader`) holds the meshes, a pose-part override table, the shadow meshes
and a texture group. Its textures are uploaded to VRAM at once (player two's three texture pages to the right of
player one's, the meshes moved to match by `OffsetModelSlotTPageClut`, `src/tpage_reloc.c`), and the space they
occupied becomes the fighter's cloth work buffer, sized by `CalcFighterClothWorkSize` (`src/fighter_cloth.c`). A
weapon archive holds every weapon of a character; `CompactArchiveSubset` keeps only the meshes the bout needs
(three for an ordinary weapon), in place, before the textures are uploaded. Which archive a fighter or weapon
uses is decided in `LoadWeaponArchive` and `src/match_setup.c`.

## Meshes as the drawers read them

A mesh is a stream of batches ([rendering.md](rendering.md)): a word with a count and a kind, the vertices,
then groups of faces, each group a halfword count, a halfword type and that many records, ended by a count of
0, then a halfword saying whether another batch follows. The face records are declared for the assembly in
`include/mesh_prims.inc`, and their sizes in halfwords for C in `gPrimRecordHalfwords` (`src/tpage_reloc.c`):

| Record | Types | Holds |
| --- | --- | --- |
| `MF3`, `MF4` | 0, 8 | Two `POLY_F3` or `POLY_F4` packets, the vertex word, a normal and a colour |
| `MFT3`, `MFT4` | 4, 12 (5, 13 unlit; 6, 14 two-sided) | Two textured packets, the vertex word, a normal |
| `MG3`, `MG4` | 16, 24 | Two packets, the vertex word, a normal and a colour per corner |
| `MGT3`, `MGT4` | 20, 28 (22, 30 two-sided) | Two packets, the vertex word, a normal per corner |
| `MF4DM` | 37 | A flat quad's two packets, the vertex word, a depth offset and two `DR_MODE`s |
| `DTRI`, `DQUAD` | dynamic 4, 6, 12, 14 | Texture page, flags, packet code, palette, texture coordinates, vertices, a normal |
| `DGT3`, `DGT4` | dynamic 20, 28 | The same with a normal per corner |

A static record carries its GPU packet twice, one per frame parity; the drawer writes the screen positions
and colour into one copy and links it. The vertex word holds 8-bit vertex indices (three, or four for a quad).
Normals are Q12 `SVECTOR`s. A dynamic record carries no packet; its drawer builds one from these fields.
Moving a mesh to other texture pages (player two, the fogged fighter) rewrites the texture page and palette
fields of every record in place (`OffsetMeshStreamTPageClut` and its kin).

## The skeleton

A fighter is `gFighterSkeletons[side]` (`src/fighter_pose.c`): node 0 places the fighter (position and
facing), and nodes 1 to 18 are the 18 joints, each a scene node parented as `gSkeletonParentIndices`
(`src/fighter_skeleton.c`) says:

```
0 root -- 1 torso -- 2 head
       |          |- 3 right shoulder - 4 upper arm - 5 forearm - 6 hand
       |          `- 7 left shoulder  - 8 upper arm - 9 forearm - 10 hand
       `- 11 pelvis -- 12 right thigh - 13 shin - 14 foot
                    `- 15 left thigh  - 16 shin - 17 foot
```

The rest pose is a translation per joint from its parent (`gSkeletonRestOffsets`), scaled by the character's
height (`gCharHeightScales`). A character's override table pairs mesh indices with joints; where two meshes
hang on one joint the second is a clone node that copies its source's world matrix each frame
(`GetSkeletonCloneParts`). The weapon meshes hang on the hands; the shadow parts are a second set of nodes of
render type 3 drawn from the shadow meshes (`InitFighterShadowParts`).

`UpdateAndEnqueueFighterPose` poses and queues it all once a frame. In the normal mode it copies joint angles
from the decoded pose: `gAnimToSkeletonJointMap` says which pose entry drives each of joints 1 to 17 (joint 0
takes no rotation), with the Y and Z angles negated. It then rebuilds the world matrices, fills the 18-entry
skin matrix list in the order the skinned body expects (joints 5, 4, 3, 9, 8, 7, 2, 1, 16, 15, 13, 12, 11, the
13 that bend the body, then 0, 6, 10, 17, 14), and queues the skinned body, the joints that carry meshes, the
clones and the shadows (not where Dozaemon Moat's floor dips, `GetMoatFloorHeight`). Other modes take the angles
from the opening demo, the select screen or a story cutscene. `BlendPoseJointRotations` instead blends two poses
joint by joint through `InterpolateOrthonormalMatrix` and leaves the result as precomputed local matrices.

## The skinned body

The skinned mesh, as `RenderSkinnedSkeletalMesh` (`src/render.s`) reads it, starts with groups of normals,
each a bone index, a count and the normals, ended by -1; they are lit into `gSkinnedPrelitColors` before
anything is drawn. Then, for each of the 13 bones in turn, five vertex lists, each a count and records:

1. vertices turned by the bone and kept unprojected, with an offset (12 bytes);
2. vertices placed at a kept vertex plus an offset in the bone's frame, then projected (14 bytes);
3. vertices turned by the view and the bone and kept (6 bytes);
4. vertices placed at a kept vertex plus the bone's translation and an offset, then projected (8 bytes);
5. ordinary vertices, projected through the bone's matrix three at a time (6 bytes).

Then the bone's face groups: types 16 and 24 lit from their normals, 38 to 41 coloured from the prelit table by
10-bit index, 0 and 8 skipped. Then seam records (source, destination, count; ended by 0xFFFF) that copy
projected vertices from one place in the buffer to another, and the index where the next bone's vertices
begin. Kept and placed vertices are how a vertex on one bone is positioned from another's: the purpose of each
list beyond what the code does with it is not stated anywhere in the source.

## The pose stream

A clip is a run of frames in the motion archive's pose stream ([formats.md](../formats.md)); `DecodePoseFrame`
(`src/pose_stream.c`) decodes one frame into 54 halfwords: three root values, then 51 joint angles (17 joints
by three). `InitPoseStream` reads the header: 51 default angles of 12 bits and 51 delta modes of 2 bits, padded
to 92 bytes, followed by a table of 24-bit big-endian bit offsets, one per group of eight frames.

The first frame of a group is a key frame: each root value is a flag bit and, if set, 16 bits (else 0); each
angle a flag bit and, if set, 12 bits (else its default). The seven frames after it are second-order deltas.
Root values are coded again, absolute, in every frame. Each angle channel whose mode is not 0 reads a change
`c`; on the first delta frame the channel's velocity becomes `c` and the angle gains it, and after that the
angle gains velocity plus `c` and the velocity gains `c`. The change is coded by the channel's mode:

| Mode | Coding |
| ---: | --- |
| 0 | None: the channel keeps its key value |
| 1 | Exp-Golomb: a run of zeros (at most 12) gives the length of the value that follows; odd values are negative |
| 2 | A flag bit; if set, 4 bits: 15, 14, 13 and 0 mean +1, -1, +2 and -2, any other value is the width of a signed value that follows |
| 3 | A flag bit; if set, 4 bits (a sign and the low three bits) and an Exp-Golomb high part (at most 8); the change is `((high << 3) + low) + 1` |

Decoding frame `n` from its key frame would cost up to seven frames' work, so `gPoseDecodeCache` keeps the last
four decoded frames with their velocities and the bit reader's position, and a frame whose predecessor is
cached is decoded in one step. `DecodePoseFramePair` decodes two frames with the modes read once; the fighter
uses it for the current frame and the next ([fighter.md](fighter.md)).

## Attached parts

`gAttachedPartPool` (`src/attached_part.c`) lends out 30 scene nodes for parts hung on a fighter for a while,
updated each frame by `UpdateAttachedPartPool`. The one kind is Katze's muzzle flash (`src/muzzle_flash.c`): a
node on the right hand stepping through meshes 16 to 19, one a frame, with a billboard flash at the barrel.
