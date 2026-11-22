# Building

Bushido Blade was built with SN Systems' PsyQ development kit and Sony's Runtime Library 3.6. The tree is built
with the same tools, each named below with the evidence that it is the one the game was built with; Sony's
libraries are built from the decompiled modules in `src/lib`; one link input is not source. The tree holds no
build scripts, only the build as facts (the last section lists them).

## The pipeline

Each game unit in `src/` becomes one object, and the objects are linked in the order
`config/bushidoblade/SCUS.LNK` lists them, followed by the libraries:

1. **Preprocess** a C unit with CPPPSX, under the build's define (`VERSION_JP` for Japan, `VERSION_EU` for
   Europe, none for North America).
2. **Compile** with CC1PSX at `-O2 -G8`.
3. **Assemble** the compiler's output with ASPSX 2.21.
4. **Post-process** the one unit that uses the GTE macros of `inline_c.h` (`src/gte_ir.c`) with DMPSX, which
   rewrites the macros' placeholder words into coprocessor instructions. Every other GTE unit spells its
   instructions through `inline_o.h`, one `asm` statement each, and needs no post-processing.
5. **Assemble** the five hand-written units (`src/*.s`) with ASMPSX.
6. **Build each library** from `src/lib/<library>/` by its own recipe (below) into an archive whose members are
   those its `.LST` file lists.
7. **Link** with PSYLINK 2.27: the 103 game objects in order, the libraries in the order the original's linker
   read them, and the declared order of uninitialized globals. The entry point is LIBSN's startup.
8. **Convert** the linked image into a PlayStation executable, whose SHA-256 is the retail executable's
   (`config/bushidoblade/builds.tsv`).

One array is not in the tree: the loading-screen picture (`gNowLoadingTim` in `src/font_hud.c`, a TIM that
differs in the Japanese build), which the unit includes from a file holding the bytes at its address in the
retail executable. `config/bushidoblade/assets.tsv` gives each build's include file, address, size and SHA-256.

## The tools

All hashes are SHA-256; `config/bushidoblade/tools.tsv` lists each by hash.

| Role | Tool | Version | SHA-256 | Where it ships |
| --- | --- | --- | --- | --- |
| Preprocessor | CPPPSX | GNU C 2.7.2.SN32.3.7 Build 0001 | `130748835d42c59880ee0d33dd0313d4c9b1a48ad5c2d67e2a3c34419590d4ec` | Japanese PsyQ 4.0 disc (DTL-S2320), Win32 |
| Compiler (game) | CC1PSX | GNU C 2.7.2.SN32.3.7 Build 0001, linked 1997-02-10 | `5594a2241d1ddaa26a61a68597d529faf9a843f89b7c288671bb325b8bc72f90` | Japanese PsyQ 4.0 disc (DTL-S2320), Win32 |
| Assembler (game, LIBSN) | ASPSX | 2.21, 1995-06-22, 60,665 B | `b52bf3d40a516333e679f18e9297883158e3015e8d97ac501e108b049617ce41` | PsyQ 3.3 kit, DOS behind the X-32 extender |
| GTE post-processor | DMPSX | 3.04 | `d4b19e89d485fb5536303200f7dd53e328178d9b5e80fe079a7d4ce36955eb92` | Japanese PsyQ 4.3 disc (DTL-S2340), Win32 |
| Assembler (hand-written units) | ASMPSX | 2.34 | `a004d4befb77bcbadfd6b042bd5f384bd33ee16061f694812ccb9a8a4c661fe5` | Japanese PsyQ 4.0 disc, Win32 |
| Linker | PSYLINK | 2.27, 1995-08-30, 55,881 B | `39c9e16ba84ddfb366004dc36f9c0b5a30f9908e56f8fca4b06ddb6eb748243b` | PsyQ 3.3 kit, DOS behind the X-32 extender |
| Compiler (Runtime Library 3.6) | CC1PSX | GNU C 2.6.0 [AL 1.1, MM 40] Sony Playstation, 1994-10-07 | `6587ed37f9f8795f8e47601d3a71167e93c832959f2f314e08d11c53be04661e` | PsyQ 3.3 and 3.5 kits (Japanese Runtime Library 3.5 disc, DTL-S2300), DOS (DJGPP) |
| Assembler (Runtime Library 3.6) | ASPSX | 2.34 | `313b950580ab63eeed5c69f1adc96628c65b20ab5f6e5438cb414eae22c8c68b` | PsyQ 3.5 and 3.6 kits, DOS |
| Assembler (LIBGPU 3.7) | ASPSX | 2.56 | `7f928fa89e143facd75737822b08bb63fb41a06415ba943393db727aa398efc8` | Japanese PsyQ 4.0 disc, Win32 |

The headers are those of Sony's Runtime Library 3.6 (Japanese disc, DTL-S2310, 1996-10).

### The compiler and its flags

The game was compiled by GNU C 2.7.2 as SN Systems shipped it with Runtime Library 3.6. The kit itself carries
the DOS build of that compiler (2.7.2.SN.1); the build uses the earliest Win32 build, 2.7.2.SN32.3.7, which
reproduces every routine of the three builds. Whether the authors ran this build or the DOS one is not
established: where the two were compared, they emitted the same code.

- **`-G8`:** the North American build's code reaches 1,392 times through `$gp` (loads and stores relative to the
  global pointer, hand-written units included), so it was not compiled at `-G0`. `-G8` is the kit's default and
  puts every global of eight bytes or less in small data.
- **`-O2`:** reproduces every routine. At this level GCC 2.7.2 inlines nothing, so which file a routine is in
  never changes its code.
- **Divide checks:** every variable divide is followed by the assembler's checks: 103 `div`/`divu` in the North
  American game code, 103 `break 7` (divide by zero) and 99 `break 6` (overflow, signed divides only). The game
  was not assembled with the option that drops them.

### The assembler and its two telltale expansions

CC1PSX leaves two expansions to the assembler: loading a constant (`li`) and addressing a global array by a
register (`lw $2,table($4)`). How an assembler writes them identifies it:

- **`li` of a constant 0..65535 is `ori $r,$zero,N`.** The North American build's compiled code holds 3,604
  such `ori` and not one `addiu $r,$zero,N` with a positive N. Every Win32 ASPSX examined (2.56, 2.67, 2.77,
  2.79, 2.81, 2.86) writes `addiu`.
- **An indexed global is four instructions:** `lui $at,%hi(table)`, `addiu $at,$at,%lo(table)`,
  `addu $at,$at,$r`, then the access at `0($at)` (672 such sequences in the North American build's compiled
  code). The Win32 builds write three, folding the low half into the access.

ASPSX 2.34 of the 3.5 and 3.6 kits writes `ori` but the three-instruction access; so do Sony's 3.6 libraries,
which it assembled. Of every assembler build examined, only **ASPSX 2.21 of the PsyQ 3.3 kit** writes both of the
game's forms, and with it every routine of the three builds reproduces. It is a DOS program; the build runs the
original executable.

### The linker, and how its order was established

Eight PSYLINK builds were run against controlled objects and then on the game's own objects: 1.15 and 1.22 of
the 3.0 kit, 2.27 of 3.3, 2.37 of 3.5, 2.42 of 3.6, and 2.52, 2.60D and 2.73D of the 4.0 to 4.3 kits. Three
behaviours single out **PSYLINK 2.27**, the linker of the kit whose assembler built the game:

- **It takes a library's modules by its symbol table,** not in archive order: in 20 random test links whose two
  rules disagree it follows the symbol-table walk every time, and the later builds follow archive order. On the
  game's objects, 2.27 lays out Sony's modules in the original's order wherever the library source and archives
  say what Sony's said.
- **It places uninitialized globals ("commons") by the same walk,** after every object's own storage: the 62
  library commons whose original addresses the game's code binds lie in exactly its walk's order.
- **It aligns every common to 4 bytes and rounds its size to 4.** Ten library commons of 16 to 384 bytes lie at
  addresses 4 modulo 8 in the original (`SsFCALL` at `0x801273F4`, `_svm_vab_pg` at `0x80129D54`, among them),
  which 2.42 and later, aligning large commons to 8, cannot produce.

PSYLINK 2.27 holds 256 modules unless told otherwise, and the game links more (its 103 objects and about 180
library modules): the original link was run with `/n` to raise the limit.

### The Runtime Library: versions and recipes

The library modules in the image were identified by searching every module of every Runtime Library release
available (3.0, 3.5 Japanese and North American, 3.6 Japanese and North American, 3.7 (the 3.7.3 beta of
1997-02-25), 4.0 to 4.3) for its bytes with its relocated words masked. They come from these sources:

| Library | Release the image holds | Evidence |
| --- | --- | --- |
| LIBC, LIBCARD, LIBCD, LIBETC, LIBGTE, LIBPRESS, LIBSN, LIBSPU | Runtime Library 3.6, Japanese disc (DTL-S2310) | Their modules occur exactly, for example LIBC's `SPRINTF` and LIBPRESS's `VLC`, the last module of the code. The startup at the entry point is LIBSN's `SNMAIN`. |
| LIBSND | Runtime Library 3.6, North American release (DTL-S2002 Release 1.0, 1996-11-06) | Its `MIDIREAD` occurs; the Japanese 3.6 release spells that module otherwise. Its other modules equal the Japanese ones. |
| LIBGPU | Runtime Library 3.7 | Its modules `SYS`, `PRIM` and `EXT` occur exactly; no 3.6 LIBGPU module does. |
| LIBCOMB (the link cable driver) | 3.7a, a release between 3.7 and 4.0 that no kit examined ships | Its read-only data and data equal those of 3.7's `COMB` byte for byte; its code does not: `_comb_control`, `HandleSio` and the synchronous serial read and write differ in substance, its routines lie in another order, and it has one routine more. 4.0.1's `COMB` is further from it. Like 3.7's and unlike 3.6's and 4.0.1's, it carries no identification stamp. A Sony bulletin on the 4.1 disc dates 3.7a to 1997-03-06. |
| LIBAPI | Runtime Library 3.6, with three modules from 3.7 and two from a build no kit examined ships | `COUNTER` occurs as 3.6's and differs from 3.7's; `SEND`, `PATCH` and `SENDPAD` occur exactly as 3.7's. `PAD` is 3.7's with every small constant loaded by `ori` and one more instruction in `RemovePatchPad`; its data equals 3.7's. `C114` equals 3.7's except its first eight bytes, the library's identification stamp, whose last two bytes read `36 00` where 3.6's read `36 10` and 3.7's `37 00`. |

Apart from `C114`'s stamp, every module's data, compared byte for byte, equals that of the release its row
names (for `PAD` and `COMB`, 3.7's).

Every module is decompiled in `src/lib`, and each library is built by the recipe that reproduces Sony's
objects (`config/bushidoblade/flags.tsv`):

| Recipe | Libraries | Compiler and flags | Assembler | Divide checks |
| --- | --- | --- | --- | --- |
| Runtime Library 3.6 | LIBAPI, LIBC, LIBCARD, LIBCD, LIBCOMB, LIBETC, LIBGTE, LIBPRESS, LIBSND, LIBSPU | CC1PSX 2.6.0, `-O2 -G0` | ASPSX 2.34 | yes |
| LIBSN | LIBSN | CC1PSX 2.6.0, `-O2 -G0` | ASPSX 2.21 | yes |
| Runtime Library 3.7 | LIBGPU | CC1PSX 2.7.2.SN32.3.7, `-O2 -G0 -fno-builtin` | ASPSX 2.56 | no |

So Sony built its 3.6 libraries with an older compiler (2.6.0) than the one it shipped to developers, and at
`-G0`: no library global lives in small data.

## The one input that is not source: the order of uninitialized globals

A global defined without an initializer (`int gFrameCounter;` at file scope) is a *common*: the compiler emits
only its name and size and leaves its placement to the linker. PSYLINK 2.27 keeps every symbol in a hash table of
512 buckets, where a name's bucket is

    bucket(name) = (length of name + sum of its bytes) mod 512

After laying out every object's own storage it walks the buckets from 0 to 511 and places the commons it finds,
small ones (eight bytes or less) and large ones separately; within one bucket the name entered last comes
first, and the libraries' names enter before the game's. **So where each common lies in the original is decided
by the authors' names.**

Those names are lost: the executables keep no symbols, and no symbol file of the game is known. With the
names this tree uses, the linker would place the commons in another order and every address after the first
would move. The original's order is therefore declared instead:
`config/bushidoblade/commons.tsv` lists every common, in the order the original holds them, with each build's
address and extent. It was derived from the image itself: each row is a global whose address the original's
code binds, read from that code, so the table records what the image shows and nothing the image does not. It
has 334 rows: 192 small and 142 large, of which 62 are the libraries' (they interleave with the game's, so they
must be listed too) and 272 the game's. At link time the table becomes one object, linked last, that defines
the commons in that order; PSYLINK 2.27 then allocates them where the original has them. Renaming a global
never moves it.

Of the game's 272 rows, 24 carry the authors' names, which the order proves: 21 proven and 3 probable.
[names.md](names.md) lists them with their evidence and says how the others were sought; they cannot be
recovered from the evidence available, so the order stays declared.

## What the compiler does with source

These habits of CC1PSX 2.7.2 at `-O2 -G8` with ASPSX 2.21 decide bytes throughout the game; a reader comparing
the source with the original's code will meet each of them.

- **`$gp` means "defined in this file".** A global of eight bytes or less is reached through `$gp` only from the
  unit that defines it; a unit that declares it `extern` reaches it through `lui`, whatever its size.
- **A small initialized object in `.data` was an unsized array.** `int x = 1;` or `unsigned char x[8] = {...};`
  goes to small data, but `unsigned char x[] = {...};` goes to `.data` at any size, because its type is incomplete
  when the compiler chooses the section.
- **An array never starts at an odd or halfword address:** the compiler aligns every array to 4, so a global read
  at such an address is an element of an array that starts before it.
- **An address built with `lui` and `ori` is a constant,** not a symbol: the motion archive's fixed load address
  `0x8014C000` is one.
- **A unit's string literals reach `.rdata` in the order the compiler expands them,** function by function as the
  file is read; a `static inline` function's literals come where it is defined.
- **Then-arms are laid out first,** so the order of blocks shows the order of the tests in the source.
- **Registers are allocated in order of first use,** weighed by loop depth: two loops that share a counter variable
  compile differently from two loops with their own.
- **Array indexing and pointer walking compile differently.** Where the original advances a counter and a pointer
  together, the source indexed the array.
- **GTE code is C.** The routines that use the geometry coprocessor inside a normal frame are written with the
  kit's `inline_o.h` macros (`gte_SetRotMatrix`, `gte_ldv0`, `gte_rtpt`, `gte_stlvnl`, ...), each one `asm`
  statement, so the compiler schedules its own work between them. A unit that uses the macros must include
  `inline_o.h`; without it they compile as calls. Routines whose arithmetic traps on overflow (`add`, `addi`,
  `sub`, which CC1PSX never emits) are hand-written assembly.

## The build as facts

The tree holds everything the build consumes except the tools, the kit's headers and the loading-screen
picture, and states those by hash:

| File | Holds |
| --- | --- |
| `config/bushidoblade/tools.tsv` | Each tool by file name, version and SHA-256, and what it is used for. |
| `config/bushidoblade/flags.tsv` | For the game's code and each library: the compiler and its options, the assembler, and whether divides are checked. |
| `config/bushidoblade/builds.tsv` | Each executable: its command file, define and SHA-256. |
| `config/bushidoblade/SCUS.LNK` | The link order: the 103 objects, then the libraries. |
| `config/bushidoblade/libraries.tsv` | Each library's member list, and the SHA-256 of the library Sony shipped. |
| `config/bushidoblade/commons.tsv` | The declared order of the uninitialized globals. |
| `config/bushidoblade/assets.tsv` | The loading-screen picture each executable embeds: address, size and SHA-256. |
| `config/bushidoblade/headers.sha256` | The Runtime Library 3.6 headers by SHA-256. |

With exactly these tools and inputs, the source was built into the three executables, and each has its retail
executable's SHA-256 (listed in the [README](../README.md)). Four of the tools are DOS programs (ASPSX 2.21,
PSYLINK 2.27, CC1PSX 2.6.0 and ASPSX 2.34), the others Win32 programs; how to run them is the reader's choice.
