# Game flow

The whole game is one state machine. `gGameState` indexes the 51 handlers of `gGameStateHandlers`
(`src/main.c`, indices named in `include/game_state.h`), and the main loop calls the current one once a frame.
Almost every screen is a pair: an `Init` handler that loads and sets up, then moves `gGameState` to its
`Update` handler, which runs each frame until it chooses the next state. The play mode (`gPlayMode`: Story, VS,
Practice, Slash, Link) is separate and tells the shared states which rules apply.

## The states in groups

```
boot      0 version banner -> 33/34 load the save -> 3/4 opening
attract   3/4 opening (three variants in turn) -> 5/6 demo battle -> 3/4 ...   start: main menu
menus     7/8 main menu -> 9/10 character select | 21/22 link lobby | 23/24 options
                         | 49/50 movie theatre | 45/46 analog controller menu (Japan)
bout      1/2 combat -> 17/18 replay -> 19/20 victory | 27/28 Continue | story states
story     37/38 prologue, 25/26 cutscene, 47/48 approach cutscene,
          29 ending movie -> 39/40 credits, 35/36 dishonourable ending
slash     13/14 Slash clear (Katze's entrance)
saving    31/32 auto-save, then the state it was asked to return to
other     41 link abort; 30, 42 and 44 wait for a button and 43 leads to 44, but nothing enters them;
          11, 12, 15, 16 do nothing
```

The bout states are described in [match-rules.md](match-rules.md), the story states in [story.md](story.md).

## Boot and the attract loop

`main` ([frame.md](frame.md)) starts at state 0, which prints the build's version and moves to the boot load
(33, 34): the memory card is read once and, if a save is found, the options and unlocks are restored. The
opening follows (`InitOpeningState`, `src/opening_state.c`): one of three opening demos in turn, with its music
([opening.md](opening.md)). When it ends, a demo battle starts (`InitDemoBattle`, `src/menu_state.c`): two
computer fighters from `gDemoFighterPairs` (six pairs in turn) on the next of eleven stages in
`gDemoStageOrder`, each with a random weapon from 0 to 7. It ends when the post-kill count reaches 106
([match-rules.md](match-rules.md)) or after 900 frames, and the opening plays again. Start in the opening or the
demo goes to the main menu; the main menu returns to the opening after 900 frames (30 seconds; 36 in Europe)
without a button held.

## The main menu and the modes

`gMainMenuEntries` (`src/menu_state.c`) lists twelve entries, each a play mode, a first-person flag, the next
state, a player count and whether it is always available:

| Entry | Mode | Players | Goes to |
| ---: | --- | --- | --- |
| 0 | Story | 1 | character select |
| 1 | VS | 2 | character select (outside Japan only with both pads connected) |
| 2 | Slash | 1 | character select |
| 3 | Practice | 1 | character select |
| 4 | VS, first person | 1 | character select |
| 5 | Link | 2 | link lobby |
| 6 | | | movie theatre |
| 7 | | | options |
| 8 | | | analog controller menu (Japan) |
| 9 | VS, first person | 2 | character select |
| 10 | Practice | 2 | character select |
| 11 | Story, move-cycle computer | 1 | character select |

The menu's cursor (`UpdateAndDrawMainMenu`, `src/main_menu_ui.c`) covers entries 0 to 7, and 8 in Japan when an
analog pad is plugged in; entries 9 to 11 cannot be chosen. Entry 5 (Link) and the last three are also marked as not
always available, which `gAllMenuModesEnabled` would unlock: `InitMainMenuState` tests L1, R1 and R2 held on entry when
that variable is negative, but `InitSystem` sets it to 1 and nothing makes it negative.

- **Story**: one player against the computer through the story ([story.md](story.md)).
- **VS**: two players on a stage from `gVsStageSelectTable` (eight stages). The win counts appear once a rematch
  has been played; with VS's first-person entry, one player fights the computer on `gPovStageSelectTable`, and
  the computer grows faster with the player's wins ([ai.md](ai.md)). After a bout, the victory scene, then
  Continue or the main menu.
- **Practice**: player one against the computer on a stage from `gPracticeStageSelectTable`, with their own sound
  bank and, on every weapon, texture group 8 of its archive in place of the weapon's own (`LoadWeaponArchive`,
  `src/model_arena.c`): one brown texture streaked like wood grain, the same in every archive. A bout has no end: a win is counted 105 frames after a fall, and from frame
  106 a face button starts both fighters again, player one where it stands and the opponent at a clear spot
  5000 away if one is found (`FindPracticeRespawnPos`, `src/fighter.c`).
- **Slash**: one player against a hundred opponents in ten levels of ten, on stage 20, always with the katana.
  The opponents are fighter ids 19 to 22 with the katana, coloured by level (`SelectSlashOpponent`,
  `gSlashEnemyTints`), and faster with each level ([ai.md](ai.md)). The next opponent replaces
  the last 61 frames after it falls (106 after a level's tenth); player one is not reset between them, so wounds
  carry over. A Continue restarts the current level from its first opponent. Clearing level 10 leads to the
  victory scene and, if `TryUnlockKatze` allows, Katze's entrance (13, 14) and a save.
- **Link**: two consoles, one fighter each, always in the first-person view, on `gLinkStageSelectTable`
  ([input.md](input.md)).

## Character select

`InitCharacterSelect` opens the select screen in one of eight variants (`SelectGameMode`): story, story
challenger, VS, VS with Katze, first-person VS, practice, link and slash. Story and Slash give player one the
fighter ids 6 to 11, the other modes 0 to 5 (`gModeCharSelectLayout`). When it finishes,
`UpdateCharacterSelect` stores the choices in `gSelectedFighter` and `gSelectedWeapon`, picks the stage from the
mode's table and starts the mode. The screen itself is [character-select.md](character-select.md)'s.

## Options

`UpdateOptionsMenu` (`src/menu_state.c`) edits eight rows, each a pointer in `gOptionValuePtrs`; defaults are set
by `ResetGameOptions` (`src/main.c`):

| Row | Option | Default | Effect in the game |
| ---: | --- | --- | --- |
| 0 | `gOptionDifficulty` | 1 (hard) | the computer's skill ([ai.md](ai.md)); needed for Katze |
| 1 | `gOptionWallBump` | 0 | running into walls stumbles ([fighter.md](fighter.md)); must be off for Katze |
| 2 | `gOptionBgmEnabled` | 1 | bout music |
| 3 | `gOptionMonochrome` | 0 | grey colours |
| 4 | `gOptionContinueEnabled` | 1 | the Continue screen after a loss |
| 5 | `gOptionDaytime` | 1 (0 in Japan) | day or night rendering (`SetRendererDaytime`) and ambient sounds |
| 6 | memory card | | save or load now |
| 7 | `gOptionAutoSave` | 0 | save after the credits and on leaving the options |

The rumble option is not a row; see [input.md](input.md). Leaving the options saves when auto-save is on.

## Unlocks and the movie theatre

Two things are unlocked and saved: Katze for VS (`gKatzeUnlocked`, which switches VS's select screen to the
variant with Katze and changes the save's title), and the endings seen (`gUnlockedMoviesMask`). `TryUnlockKatze`
(`src/story_state.c`) unlocks Katze when Slash mode is cleared with no continue taken, the difficulty option on,
and the wall-bump option off. The movie theatre (49, 50) offers the opening and, for each character whose ending
has been seen, the normal ending on confirm and the true ending on a second button if that has been seen too.

## Saving

A save holds one 32-bit word: the eight options, the Katze flag and the endings seen
([files-and-memory.md](files-and-memory.md) gives its format). Nothing else is saved: no story progress, scores
or records.

The game saves itself after Katze's entrance always, and after the credits and on leaving the options when
auto-save is on (31, 32); an error is shown until cross is pressed. It loads at boot and from the options. The
card's own state machine is [memory-card.md](memory-card.md); the menus' screens are [menus.md](menus.md).
