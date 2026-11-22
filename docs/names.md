# Names

The executables keep no symbols, so almost every name in the game's own code had to be chosen. A name in this
tree is one of two things:

- **The authors' own spelling,** taken only with evidence that the original used it, as set out below.
- **A descriptive name chosen here,** which says what the routine, global, record or field is or does, as far
  as that is known (where all that is known is that nothing uses it, or that nothing reads what is written
  to it, the name says so: `...Unused`, `...Unreached`, `...Unread`). It makes no claim about what the
  authors called it.

Names that Sony's development kit defines (library routines and globals, the types and constants of its headers,
such as `DRAWENV`, `POLY_FT4` or `CdlModeStream`) are Sony's, as its archives and headers spell them.

## The linker's order as a test of a name

The original linker, PSYLINK 2.27, placed every uninitialized global ("common") by walking a hash table of 512
buckets, where

    bucket(name) = (length of name + sum of its bytes) mod 512

It places the commons bucket by bucket from 0 to 511, small ones (eight bytes or less) and large ones
separately; inside one bucket the name entered last comes first, and Sony's library names enter before the
game's. ([building.md](building.md) says how this linker was identified.) So the order of the commons in the
original executable is a trace of the authors' names, and a candidate name can be tested: it must hash into the
range of buckets its neighbours leave open.

The rule was checked before it was used. The 62 library commons, whose names are Sony's and known, ascend
through the buckets with no exception, from `_svm_okon1` (bucket 6) to `_svm_okof2` (bucket 511). Both pairs of
them that share a bucket obey last-entered-first: `_snd_seq_t_max` (LIBSND) precedes `StEmu_Addr` (LIBCD), and
the linker read LIBCD first; `StRingBase` precedes `StRingAddr`, the later of the two in LIBCD's dictionary.

A global between two library commons is pinned to the buckets between theirs. A candidate's chance of fitting
by accident is the width of that interval over 512, multiplied by the number of candidates tried; every
candidate list was written down before its buckets were computed. Among the small commons there are no library
anchors, so a name there is tested by the order of a whole chain of candidates together.

None of the descriptive names fits its interval more often than chance predicts, as expected of names chosen
for meaning.

## Units that descend from the kit's samples

Sony shipped sample programs with its development kit, and some of the game's units descend from them: the
memory card units, the movie player, parts of the CD driver and the sound start-up, and the link cable code. The
source of these units is written from the game's bytes in this tree's own names and words like every other unit;
where its statements resemble a sample's, the bytes require it. The samples served one purpose here: the names
they give the corresponding globals were candidates for the linker's test, and the spellings that pass it are
kept as the authors'. For the link cable's globals the samples' names fail, and those globals keep descriptive
names.

## The authors' names in this tree

**Proven by the linker's order (21).** The small commons, in the order the original holds them, with their
buckets: every one ascends.

| Name | Unit | Bucket |
| --- | --- | ---: |
| `result` | `cd.c` | 165 |
| `imgbuf0`, `imgbuf1` | `movie.c` | 177, 178 |
| `vlcbuf0`, `vlcbuf1` | `movie.c` | 185, 186 |
| `fd` | `memcard_io.c` | 204 |
| `ev0`, `ev1`, `card_sts_tmp`, `ev2`, `ev3` | `memcard_io.c` | 270, 271, 271, 272, 273 |
| `dec` | `movie.c` | 303 |
| `ev10` to `ev13` | `memcard_io.c` | 320 to 323 |
| `card_sts` | `memcard_io.c` | 347 |
| `Ring_Buff` | `movie.c` | 379 |
| `event_time_out` | `memcard_io.c` | 501 |

`ev1` and `card_sts_tmp` share bucket 271, and `ev1` comes first. That is what the linker does with two names
of one unit when the first was declared first (the compiler emits a unit's commons in reverse order of
declaration, and the linker puts the last entered first). Two large commons are pinned between library anchors:

| Name | Unit | Bucket | Interval left by its neighbours |
| --- | --- | ---: | --- |
| `slot_dir` | `memcard_io.c` | 360 | 359 to 362, between `_svm_cur` (358) and `_svm_vab_used` (362) |
| `seq_table` | `sound.c` | 441 | exactly 441, between `StRingIdx2` (440) and `StRingIdx3` (441), before the latter as a later entry must be |

The memory card globals were also tested in uppercase (`SLOT_DIR` 136, `CARD_STS` 123, `CARD_STS_TMP` 463,
`EVENT_TIME_OUT` 117, `FILE_DSCRPTR` 429): every one falls out of order, where the spellings above fit.

**Probable (3).** `db` (declared in `include/display_buffer.h`, the two display buffers, the kit's own idiom)
hashes to 200 in the interval 193 to 201 left by `_spu_RQ` and `StCHANNEL`. `ot0` and `ot1` (`movie.c`, the two
ordering tables) hash to 278 and 279, where the chain above leaves room between `ev3` and `dec`, and the link
order of the globals beside them independently fixes their pair's position. The odds that these fit by chance
are about 1 in 57 and 1 in 41.

Each of these names is defined in the unit given, except `db`, which `src/globals.c` defines with the other
uninitialized globals no unit of the game defines.

Initialized globals, statics, routines and records are not commons, so the linker's order cannot test their
names: all of them carry descriptive names, in the units that descend from the samples as everywhere else.

## What the order shows without names

Families of globals declared together keep their shape in the order. The nine GPU primitive codes cached in
`src/model_table.c` (`gPolyF3Code` to `gPolyGT4Code`, `gLineF2Code`) lie exactly as one stem with the tokens
`F3`, `G3`, `F4`, `G4`, `FT3`, `GT3`, `FT4`, `GT4` (an uppercase `T`, as in Sony's `POLY_FT3`) and `L2` would
place them; the chance of that arrangement is far below one in a billion, but the stem itself is not
identifiable. The rain colours of `src/stage_callbacks.c` lie at most eight buckets apart, which no spelling of
"top" and "bottom" allows: they were numbered or lettered. Such findings constrain the names; they do not
supply them, and the tree's names stay descriptive.

## The later games by the same studio

Lightweight's later games were examined for symbols that might carry over:

- **Bushido Blade 2** (1998) keeps no symbols, map or source file names on its disc. It shares part of this
  game's engine (the renderer, CD, cloth, memory card, TIM, link cable and geometry code) and none of its
  fighter, AI, effects, sound, menu, story, combat or movie code. Its own globals obey the same linker rule.
- **Kengo: Master of Bushido** (2000, PlayStation 2) ships a symbol table with its source file names. It is a new
  program: no routine or table of this game recurs in it, and its global names fall into this game's intervals
  only as often as chance predicts. It shows the studio's later conventions (lower_snake and lowerCamel case,
  romaji words) but no name of this game.

Neither recovers a name here. Of the game's 272 commons, the 24 above carry the authors' names; the rest are
descriptive, and their order is declared ([building.md](building.md)).

## The runtime library's names

Sony's object files keep a symbol table, and every name one carries is kept as it gives it: exported routines
and globals, and the static routines and data the compiler named (`MDEC_reset`, `getintr`, `Result`,
`HeaderArea`).

- LIBC, LIBCARD, LIBCD, LIBETC, LIBGTE, LIBPRESS, LIBSN and LIBSPU take their names from the Runtime Library 3.6
  objects, LIBGPU from 3.7's, LIBSND from the US 3.6 release's, and LIBAPI's other modules from 3.6's: the
  objects each module equals. LIBAPI's `C114` equals no release's object, differing from 3.7's only in its
  identification stamp; its one name, `_96_remove`, is the same in 3.6 and 3.7.
- LIBAPI's `SEND`, `PATCH` and `SENDPAD` are byte for byte Runtime Library 3.7's and take their names from its
  objects (`SendPAD`, `EnablePAD`, `DesablePAD`, `_patch_pad`, `_SendPAD`, `_send_pad`). LIBAPI's `PAD` and
  LIBCOMB's `COMB` match no release compared (3.6, 3.7, 4.0.1). Their names are those of the 3.7 objects of
  the same modules, which carry every routine and named static of the two (`patch0`, `SetPatchPad`, `_Pad1`,
  `regs`, `sen`, `rec`, `SioAnsyncRead`, `HandleSio`, ...), except `reset_graph`, a 32-byte routine that 3.7's
  `COMB` lacks, whose name is chosen here. The 3.6 `COMB` object is an older implementation and names other
  things.
- `DesablePAD` is 3.7's spelling; 4.0.1's `PATCH`, whose bytes are the same, spells it `DisablePAD`. The bytes
  cannot decide between the two. The game's LIBAPI is of 3.7's line (its `SEND` is 3.7's and differs from
  4.0.1's), so the tree keeps 3.7's spelling.
- Static data and inlined helpers that no object names, and the types, macros and constants that the kit's
  headers do not define, have names chosen here (`vram_width`, `GpuQueueEntry`, `CD_BANK_INTR`). The kit's
  headers are included, never copied.
- Strings are bytes of the image: the `$Id` lines, and the CD command names, which give command `0x0a` the name
  `CdlReset` that `libcd.h` lacks.

## Conventions of the descriptive names

- Routines are `PascalCase`, verb first (`UpdateFighterFrame`, `LoadStageArchive`).
- Globals are `g` followed by `PascalCase` (`gGameState`, `gFighters`).
- Records are `PascalCase` typedefs (`Fighter`, `SceneNode`, `MotionPlacement`); constants and enumerators are
  `UPPER_SNAKE_CASE`.
- Words follow the game: `Fighter` for a combatant at run time and `Character` for roster identity, `Stage` for a
  playable place, `SubWeapon` for a thrown item, `BladeLock` for the locked-blades struggle, `Pov` for the
  first-person view.
- Weapons and stages carry the names the select screens draw for them: the weapon cards, drawn by weapon id
  (`include/weapon_id.h`), and the stage select's name plates, each shown for the stage the mode's table gives its
  row (`include/stage_id.h`). Weapon 9 and the stages the select screens never list have no such name and keep
  their numbers, or a name that says what the code does there.
