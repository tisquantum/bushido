# Files and memory

The list of the disc's files and the outer layout of each format are in [formats.md](../formats.md).

## Finding a file

The game has no file system code. `gCdFileTable` (`src/cd.c`) gives each file the game reads its start on the
disc (a `CdlLOC`: BCD minute, second, sector) and its size in bytes (`CdFileEntry`, `include/cd.h`). A file is
named by a category and an index: `ResolveCdFileHandle` adds the index to the category's first entry, taken
from a table that differs per build (`gCdCategoryFirstFile`):

| Category | | North America | Japan | Europe |
| ---: | --- | ---: | ---: | ---: |
| 0 | Data (`CD_CATEGORY_DATA`) | 0 | 0 | 0 |
| 1 | Motion archives | 137 | 136 | 137 |
| 2 | Sound archives | 182 | 180 | 183 |
| 3 | (empty: starts where 4 does) | 329 | 327 | 330 |
| 4 | XA music | 329 | 327 | 330 |
| 5 | Movies | 333 | 331 | 334 |

The lengths in sectors of the four music tracks that loop in bouts follow it (`gBgmTrackSectorCounts`,
[audio.md](audio.md)).

Data files are numbered from 0 in a fixed order (`gCdFileNames`, `src/asset_load.c`, a table of names nothing
reads): characters 0 to 34, weapons 35 to 54, stages 55 to 80, the interface and effect files from 81, then the
fighters' texture archives, the first-person mannequin and the story cutscenes, which in Japan are numbered one
lower (its interface group has one file fewer). `ReadCdFileSync` reads a data file by that number.
`LoadCharacterFile` first asks the resource cache (below).

## Reading

`StartCdFileRead(handle, buffer)` (`src/cd.c`) only records the request; the work is done by `UpdateCdSubsystem`,
one state per call, from the top of every frame or from `WaitCdIdle`, which loops on it, exchanging an idle
packet with the link cable and waiting `VSync(2)` between steps. The states of a read (`CdStreamState`):

1. Set double speed and whole sectors with their headers (`CdlSetmode`, `CdlModeSpeed | CdlModeSize1`); wait
   for the drive, up to 60 polls.
2. Wait three steps, then seek (`CdlSetloc`) and start reading (`CdlReadN`) with `HandleCdDataReadyIrq` as the
   data-ready callback.
3. For each sector the callback reads the 12-byte header, compares its position with the sector expected and
   copies the 2,048 bytes to the buffer; a sector out of order marks the read failed. After the last sector the
   drive is paused.
4. Done, or on an error: if the lid is open, wait for it with `CdlNop`; then `CdlGetTN` and start again from the
   first state.

A read covers whole sectors, so a buffer receives the file rounded up to 2,048 bytes. The drive does one thing
at a time: while music streams no read can start, and loading code stops the stream first
(`StopAndWaitCdIdle`). Music streaming is in [audio.md](audio.md).

The resource cache (`src/resource_cache.c`) keeps groups of character files loaded back to back in a buffer, up
to ten files, so they can be copied in memory instead of read again: the four Slash-mode enemies
(`PreloadSlashEnemyGroup`, loaded to 0x801B3000) and the select screen's characters. Seven groups are listed in
`gResourceGroupIds`; nothing loads the first two.

## Inside the archives

**Model tables.** A word count with each mesh's byte offset after it. `RegisterModelTable` adds the archive's
address to each offset and sets bit 15 of the count; `RelocateModelTable` adds a distance when the archive moves
([models.md](models.md)).

**Character archives** (`CharacterArchiveHeader`, `include/model_arena.h`): a section count, then
the offsets of the meshes, the override table, the shadow meshes, the texture group, and the end. The override
table pairs a mesh index with a joint index, two halfwords an entry, ended by 0xFFFF
(`CountFighterOverrideEntries`).

**Weapon archives** (`WeaponArchiveHeader`): the meshes and a texture group. `CompactArchiveSubset`
(`src/model_table.c`) rewrites the mesh table and its data in place so only the listed meshes remain, in the
order listed, and returns the end of what it kept; the arena keeps only that much.

**Stage archives**: [stages.md](stages.md).

**Motion archives** are read whole to 0x8014C000 and never relocated: every pointer in them is already an
address in that buffer. `MotionFileHeader` (`include/motion_file.h`) holds the start placements, the clip table,
each clip's first pose frame, the pose stream, the story intro and victory placements and the computer's attack
tables; `LoadWeaponPairMotions` (`src/match_setup.c`) picks the archive from `gWeaponPairMotionFiles` by the
two weapons, weapon 9 counting as 4. The motion graph is [fighter.md](fighter.md)'s, the pose stream
[models.md](models.md)'s.

**Sound archives** (`SoundArchive`, `src/sound.c`): two offsets bounding the block of VAB headers, then up to
twelve offsets of headers and bodies, as many as the archive's bank set needs ([audio.md](audio.md)).

**Texture groups** are uploaded by `LoadTimGroup` and its variants (`src/tim.c`), each with an optional offset
for the image and the palette, which is how player two's textures land three pages to the right and a fogged
fighter's palettes in the fog block ([lighting.md](lighting.md)). The space a group occupied is then free for
other use.

## Movies

The movie player (`src/movie.c`) streams STR files from category 5 into a workspace at 0x8014C000: its state, a
ring buffer of 32 sectors, the decoder's buffers and, outside Japan, the strips the picture is drawn with. While
a movie plays the CD driver is idle and the player keeps its own fields in the driver's record (`MovieCdSlot` in
`gCdDrive`); [movies.md](movies.md) follows the player. [history.md](../history.md) explains why the Japanese
build decodes straight to the screen and the
others do not.

## The memory map

Main memory is 2 MB, 0x80000000 to 0x80200000; the console's kernel keeps the first 64 KB. The executable is
loaded at 0x80010000 and the startup clears the uninitialised data. The rest of memory is used at fixed
addresses the code names; nothing allocates.

```
0x80010000  read-only data, then code, data, small data          (the executable image)
            uninitialised data, including:
              gModelArena            167,936 B   characters, weapons, stage
              gAuxGpuPacketBuffers    2 x 32 KB  the select scene's packets
              gAuxRegion             0x32800 B   the select scene's files; the two GPU packet buffers
                                                 (2 x 0x15800 B) are its last 176,128 B
0x80148000  gSharedWorkBuffer, 16 KB: the replay, memory card transfers, character select, subtitles
0x8014C000  the dynamic load area, to the top of memory: sound archives while loading, then the motion
            archive; the select screen, the opening demo, the splash screen and movies use it in turn
0x801B3000  Slash mode's preloaded enemies
0x801EF000  Slash mode's VAB headers
0x801FFFF0  top of the stack (0x801FFF00 in Japan, set by main)
```

| | North America | Japan | Europe |
| --- | --- | --- | --- |
| Image ends | 0x800A1000 | 0x800A3000 | 0x800A1000 |
| Code | 0x80012654 to 0x800789C4 | 0x800120D4 to 0x8007A3FC | 0x80012654 to 0x80078DD4 |
| Uninitialised data ends | 0x801466D0 | 0x80147A28 | 0x80146AC8 |
| `gModelArena` | 0x800AC6F0 | 0x800AE6C0 | 0x800ACAE8 |
| `gAuxGpuPacketBuffers` | 0x800D56F0 | 0x800D76C0 | 0x800D5AE8 |
| `gAuxRegion` | 0x800E56F0 | 0x800E76C0 | 0x800E5AE8 |
| GPU packet buffer 0 (`gAuxRegion.packets.buffers`) | 0x800ECEF0 | 0x800EEEC0 | 0x800ED2E8 |
| Effect textures (`gAuxRegion.effectTextures.words`) | 0x800F7EF0 | 0x800F9EC0 | 0x800F82E8 |

The source declares `gAuxRegion` as one union with three views of the same 0x32800 bytes: `resourceBuffer`, all
of it; `packets.buffers`, the two frame buffers of 0x15800 bytes that take its last 176,128 bytes
(`GetGpuPacketBuffer(n)` returns the n-th); and `effectTextures.words`, its last 131,072 bytes. The effect
textures are loaded there and uploaded to VRAM before any frame uses the bytes as packets. The canary in their
last word (`CheckModelArenaCanary`) is thereby also the last word of packet buffer 1. `GetAuxResourceBuffer`
returns the whole region and `GetAuxResourceBufferSize` its 0x32800 bytes, which the select scene can use
because it draws from the auxiliary packet buffers.

The scratchpad, 1 KB at 0x1F800000, is borrowed in turn by the renderer, the collision tests, the battle camera,
the cloth solver, the weather and the trails, each with its own layout (`RenderScratch`, `ClothScratch`,
`BattleCameraScratch` and others).

## The memory card

A save is one block (8 KB) on the card in the first port, named `BASCUS-94180BUSHIDO1` in North America,
`BISLPM-86020BUSHIDO1` in Japan and `BESCES-00899BUSHIDO1` in Europe (`gMemoryCardFilename`, `src/memcard.c`). It
holds 0x2A0 bytes:

- **The header** (`MemoryCardSaveHeader`, `include/memcard.h`, 0x200 bytes) the console's card screen
  reads: "SC", the icon format, one block, a 64-byte title in Shift JIS, a 16-colour palette and three 16 by
  16 icon frames of 4 bits a pixel (`BuildMemoryCardHeader`). Every build writes the three frames; the format,
  0x11 outside Japan, shows the first alone, and Japan's 0x13 cycles through all three. The
  title changes once Katze is unlocked: "Bushido Blade" becomes "Bushido Blade [+Katze (VS mode)]" in full-width
  letters, and Japan's ブシドーブレード gains ［対戦モードにカッツェ見参！］ (`gMemoryCardTitles`).
- **The save word**, 0xA0 bytes. Everything saved fits one 32-bit word (`PackSaveFlags`): bit 0 difficulty, 1
  wall bump, 2 music, 3 monochrome, 4 continue, 5 daytime, 6 auto-save, 7 rumble, 8 Katze unlocked, and from bit
  16 the unlocked movies (bits 0 to 5 the six endings, 6 to 11 the six true endings). `EncodeSavePayload` writes
  32 random key bytes and then 32 copies of the word, copy `i` rotated right by `(key[i] + i) & 31` bits and
  inverted when `key[i] & i` is not zero. `DecodeSavePayload` undoes each copy and accepts a value 17 copies agree
  on; otherwise the load ends as `MEMORY_CARD_STATUS_CORRUPT_DATA`.

The card is driven through the kernel's card events in `src/memcard_io.c` ([memory-card.md](memory-card.md)), and
the save, load and format
machine is `UpdateMemoryCard` (`src/memcard.c`); when the game saves and loads is [game-flow.md](game-flow.md).
