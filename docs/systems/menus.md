# The menus

The main menu, the options menu and the Japanese build's analog controller menu are flat screens of textured
rectangles: each loads one picture file from `DATA3/` into the model arena, uploads it, and every frame draws lists
of `TexturedRectEntry` sprites (`DrawTexturedRectList`, `src/sprite2d.c`) over a backdrop. Which modes the main
menu offers and what the options do are in [game-flow.md](game-flow.md); character, weapon and stage selection have
a page of their own, [character-select.md](character-select.md). The states are in `src/menu_state.c` and
`src/story_state.c`, the drawing in `src/main_menu_ui.c`, `src/options_ui.c` and `src/analog_menu.c`.

## Sprite lists

A `TexturedRectEntry` (`include/sprite2d.h`) is a ready `POLY_FT4` header (tint, four UV corners, palette and texture
page, `GlyphTexture`) followed by a count and that many screen rectangles; bit 31 of the count marks the last entry
of a list. `DrawTexturedRectList(list, x, y, layer)` turns each rectangle into a `POLY_FT4` at the packet cursor,
offset by (x, y), linked into ordering-table entry `layer` (0, the front, for most menu sprites). The UVs stay
fixed and the rectangle sets the size on screen, so a sprite is stretched or squashed by changing its rectangle.
Menus dim and brighten a sprite by writing its tint (0x80 is the texture's own brightness), and mirror a piece by
swapping its U coordinates. Large flat areas are a few texels stretched over the screen. The Japanese build also
has `DrawTexturedRectListAtOrigin`, linked at the head of the table.

Behind the menus lies `SubmitMenuGradientBackdrop` (`src/gradient_bg.c`): one `POLY_G4` from Y 16 to the bottom of
the screen in the last ordering-table entry, red (0xC0, 0, 0) at the top shading to yellow (0xC0, 0xC0, 0) at the
bottom, or solid black with palette 1, in grey when the monochrome option is on.

## The main menu

`InitMainMenuState` (state 7) disconnects the link cable, stops the CD, resets the story and Continue state, loads
the menu sound bank and calls `InitMainMenu`, which reads `DATA3/O12.DAT`. `UpdateMainMenuState` reads up and down
from player one's pad, or from player two's when player one pressed neither, and confirm from both, and calls
`UpdateAndDrawMainMenu`:

- **The strip.** One entry is shown at a time, at Y 189 inside a frame of eight pieces (`gMainMenuFrameSprites`)
  under the two halves of the logo (`gMainMenuLogoSprites`), with arrows above and below. Moving rolls the strip:
  over 18 lines, 4 a frame, the outgoing entry's sprite is squashed toward one edge while the incoming one grows
  from the other (`SetMenuSpriteListYAndHeight`). The entries' sprites are `gMainMenuItemSprites`, in the order of
  `gMainMenuEntries`: Story, VS, Slash, Practice, first-person VS, Link, the movie theatre, Options and, in Japan,
  the analog controller menu. Japanese labels wider than 255 pixels are two sprites.
- **The count.** Eight entries, nine in Japan when either pad reports an analog type (`IsPadAnalog`). When the pad
  is unplugged with the cursor on the ninth, the strip rolls back up by itself.
- **Confirm.** A one-player entry takes confirm from player one only; Link too; the other entries from either pad,
  and outside Japan two-player VS needs both pads connected. An entry whose `alwaysUnlocked` is 0 needs
  `gAllMenuModesEnabled` ([oddities](oddities.md)). Accepted, the entry sets the play mode, the first-person flag
  and the player count, clears the VS win counts for VS and Practice, and goes to its state; refused, a sound
  plays.
- **Idle.** 900 frames (30 seconds; 36 in Europe) with no button held return to the opening.

Sounds come from `PlayMenuSound(index)`, which looks the index up in `gMenuSoundIds` (36 entries, 255 for none; the
table lives in `src/memcard.c`) and queues it at volume 0x60.

## Options

`InitOptionsMenu` (state 23) puts the cursor on the first row and loads `DATA3/O04.DAT` (`LoadOptionsMenuAssets`,
which also sets the red-to-yellow backdrop). Each frame `UpdateOptionsMenu` draws the screen (`DrawOptionsMenu`)
and handles one of three sub-screens (`gOptionsSubScreen`): the rows, a memory card status box, or a yes-or-no box
([memory-card.md](memory-card.md)).

The eight rows are the pointers of `gOptionValuePtrs`: difficulty, wall bump, music, monochrome, Continue, daytime,
the memory card action and auto-save. Outside Japan each row shows its two choices, the active one at shade 0x80
and the other at 0x40; in Japan each row shows only its present value, and the selected row's two choices appear
in two boxes, the active one at 0xFF and the other at 0x20. The selected row's label is at 0xC0 and the others at
0x50. Which choice a set value lights is
`gOptionsInvertMask`; outside Japan the daytime row is passed inverted, so its pair reads night first. Leaving with
cancel (or triangle outside Japan) saves first when auto-save is on.

The two layouts differ:

| | Japan | North America and Europe |
| --- | --- | --- |
| Rows | columns side by side, each a panel with a vertical caption | lines down the screen |
| Move between rows | left and right | up and down |
| Change a value | down toggles | left and right set the value (`OPTIONS_LEFT_VALUE_MASK` gives each row's value on the left) |
| The selected row's choices | shown below in two boxes (`gOptionsChoiceBoxSprites`) | both shown on the row |
| Memory card action | save is 0, load is 1 | load is 0, save is 1 |
| Leave | cancel | cancel or triangle |

The card messages are sprite lists indexed through `gMemoryCardStatusToMsg` into `gOptionsMsgSprites`, drawn with a
box (`gOptionsMsgBoxSprite`); the two questions add the prompt and a highlight on yes or no. Message 15
(`gOptionsMsgCheckingCardSprites`), whose texture in `DATA3/O04.DAT` reads "Checking memory card, Just one
moment." (in Japan メモリーカードを　調べておる　しばし　待たれよ), is mapped from no status and never shows. The
auto-save reuses the options art to show its error (`DrawAutoSaveErrorOverlay`). In the Japanese build the
values are dimmed through their red channel alone ([oddities](oddities.md)).

## The analog controller menu (Japan)

Main menu entry 8 leads to states 45 and 46, empty outside Japan. `LoadAnalogControllerAssets` reads
`DATA3/O13.DAT`; `UpdateAnalogControllerMenuState` (`src/story_state.c`) has two rows, up and down moving between
them and cancel returning to the main menu:

- **Rumble**: left turns it on, right off (`gOptionRumbleEnabled`), the active word bright and the other dim.
- **Stick test**: for each pad that reports an analog type, its panel is lit and two dots show the left and right
  sticks, moved from their rest place by the stick's deflection, an eighth of it across and a sixteenth down
  (`DrawAnalogControllerMenu`). Confirm on this row records both pads' current stick positions as their centres
  (`CapturePadStickCentres`, `src/pad.c`, kept inside each stick's measured range). How the centres are used is in
  [input.md](input.md).

The backdrop repeats a 32 by 16 texel pattern through a texture window, as the movie theatre's does.
