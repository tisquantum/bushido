# Video memory

The GPU's memory is 1,024 by 512 halfwords. Most of what lands in it is placed by the files: every TIM group
(`LoadTimGroup`, `src/tim.c`) carries, for each picture, the VRAM position of its pixels and of its palette, and
the code uploads it there ([formats.md](../formats.md)). What the code itself fixes are the display buffers, the
palette rows the fog works in, a few offsets applied on upload, and the texture pages and palettes its own
primitives name, each page as `GET_TPAGE(depth, blend, x, y)` and each palette as `GET_CLUT(x, y)`
(`include/gpu_code.h`). X is in halfwords: a 4-bit texture page is 64 halfwords (256 texels) wide, a palette of
16 colours 16 halfwords.

## The plan

```
        x 0                                 640                               1023
  y   0 +-----------------------------------+---------------------------------+
        | display buffer: rows 0 to 239     | textures and their palettes:    |
    240 +-----------------------------------+ fighters, stages, effects,      |
        | display buffer: rows 240 to 479   | menus, movies (placed by the    |
    480 +-----------------------------------+ files, some positions fixed     |
        | palettes: stage row, fog steps,   | by code, below)                 |
    511 | the fogged fighter's block        |                                 |
        +-----------------------------------+---------------------------------+
```

## The display area

The two display buffers take (0, 0) to (639, 479), each half drawn while the other is shown ([frame.md](frame.md));
`InitSystem` clears that rectangle at boot (`gDisplayClearRect`). The movie players and the splash screen set up
their own display buffers in the same area, and the Japanese movie player decodes its two 24-bit frames straight
into VRAM at (0, 0) and (480, 240), the second reaching past X 640 ([movies.md](movies.md)).

## The palette rows, Y 480 to 511, X 0 to 639

- **The stage palette row**, 320 colours at (0, 0x1E0). Every stage texture's palette is part of it; the fog
  rewrites it during the frame ([lighting.md](lighting.md)). Effects drawn on the stage take theirs from its last
  32 entries: snow and footprints from X 304 (`SetupSnowBuffers`, `InitStageGroundDecals`), rain splashes and the
  sea from X 288 (`InitMoatRainAndFoliage`, `src/ocean.c`), so they fog with it.
- **The fog steps.** `BuildFogClutTable` (`src/fog.c`) writes 63 blended copies of the row, the first at (320,
  0x1E0) and then two to a row, (0, 0x1E1), (320, 0x1E1) and so on to (320, 0x1FF). With a fogged fighter the stage
  gets 31 steps, ending at row 0x1EF.
- **The fogged fighter's palettes**, an 80 by 8 block at (0, 0x1F8), with 15 steps of their own written at X 80,
  160 and on, continuing from (0, 0x1F0) when they reach X 640 (`BuildCharFogClutTable`). A fogged fighter's
  archive is uploaded with its palettes moved 640 left and 256 down (`FOG_CLUT_PAGE_DX`, `FOG_CLUT_PAGE_DY`,
  `LoadCharacterArchive`, `src/model_arena.c`).
- **The opening's rows.** The opening copies the stage row to rows 0x1E1 and 0x1E2 and tints the second
  (`BuildOpeningTintedCluts`); its stage has no fog.
- **A strip at (320, 480)**, 256 by 5, that `BackupVramStrip` and `RestoreVramStrip` (`src/vram_backup.c`) save to
  and restore from memory. Nothing calls them.

## The right-hand area, X 640 to 1023

Everything else: textures and their palettes, placed by the files, with these positions fixed or implied by code.

**Fighters.** Player one's character textures are uploaded where its archive says; player two's go three texture
pages (192 halfwords) to the right, palettes included, and its meshes are rewritten to match (`TPAGE_P2_X_OFFSET`,
`OffsetModelSlotTPageClut`, `src/tpage_reloc.c`, [models.md](models.md)). The code names player one's page at X 768
and player two's at X 960 (Katze's muzzle flash meshes, `src/effect.c`; the slash trail strip at row 200 of those
pages, its palettes at (816, 201) and (1008, 201), [effects.md](effects.md)), and the fighters' palettes in rows
246 to 255 from X 640 (the Slash enemies' palettes, `gSlashEnemyClutLists`, tinted in place for each level by
`TintSlashEnemyCluts`, `src/fighter_pose.c`; and the fog's 640-left, 256-down move, which brings the block at (640,
248) to (0, 0x1F8)). Wound overlays and the story fighters' alternate textures are TIM groups uploaded over the
base textures in place (`ApplyFighterTextureOverlays`, `LoadFighterAltTextures`, `src/fighter_texture.c`). The
select screen's host is uploaded one page height (256 rows) down.

**Effects.** The common effect file (`DATA3/O00.DAT`) fills the pages at (896, 256) and (960, 256) and nothing
else, the billboard pictures and most 3D effects' alike; its palettes are 16-colour rows in one column at X 896,
rows 470 to 511 (the billboards' at (896, 504) to (896, 511)). Several effects step through palette rows from frame
to frame. The meshes of `src/effect.c` also name pages at X 768 and 832 of the lower half, which this file does not
fill, and a few take their palettes from the stage row at X 272 and 288 (`gEffectTextureSources`,
`src/billboard_driver.c`, and the meshes of `src/effect.c`).

**Stage extras.** Snow from (640, 256); the sea from (704, 384) and (704, 448); Dozaemon Moat's foliage at (832,
256) and rain splashes at (832, 384); stage 9's cloth at (864, 320) with its palette in the stage row at X 288
(`gStage9ClothImage`). The animated water copies blocks within X 672 to 895, rows 256 to 511:
`UpdateWaterTextureScroll` (`src/water_anim.c`) shifts a strip each frame with `MoveImage`, three halved copies of
it at once, and `QueueAnimatedVramTextures` links `DR_MOVE`s near the back of the ordering table (entry 4095) that
copy one of three frames, changing every fourth frame, over the texture before the stage is drawn.

**Screens.** Each 2D screen loads its own pictures into this area, and its sprite tables name where they are: the
main menu pages at (896, 256) and (960, 256), palettes at X 992 and 1008; the options pages from (640, 256),
palettes from (688, 481); the select screen pages at X 704 to 960 from row 128 down, palettes down X 1008 (rows
128 to 188) and, for two of its groups, at (880, 256) to (880, 267) and in rows 506 to 511 from X 768; the movie
theatre from (640, 0), palettes at (752, 221 to 255); the analog controller menu from (640, 0); the opening's pages
at X 832 to 960 of the upper half; the prologue's full-screen pictures, read as 8-bit images from X 640 with their
palettes at X 640. The credits strips lie anywhere from X 640, their palettes down X 1008. The movie frame outside
Japan is loaded to (640, 0) to (959, 239), and the boot splash to X 640.

## What changes it during play

- Uploads: `UploadTimToVram` puts a picture and its palette in place, greying the palette when the monochrome
  option is on (`ConvertClutToMonochrome`); `LoadTimGroupWithOffset` and `LoadTimGroupWithSeparateOffsets` shift
  them.
- Palette edits: `TintVramClut` and `ScaleVramClutInPlace` read a palette back (`StoreImage`), scale it and write
  it again; the fog builders do the same with the stage row.
- Moves in the ordering table: the fog's `DR_MOVE`s copy a step over the live palette row at each depth, and the
  animated water's copy frames over their textures.
- After a screen has gone, its area is simply overwritten by the next one's uploads; nothing allocates VRAM.
