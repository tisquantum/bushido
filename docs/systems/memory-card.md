# The memory card

The game keeps one save file on the card in the first port (port 0, slot 0). What the file holds, its header and
its encoded save word are in [files-and-memory.md](files-and-memory.md); when the game saves and loads is in
[game-flow.md](game-flow.md). Between the two lie the kernel's card events and file calls
(`src/memcard_io.c`) and the probe, command and transfer machine that drives them a step per frame
(`src/memcard.c`). Their constants and enumerations are in `include/memcard_io.h` and `include/memcard.h`.

## The kernel's side

`InitMemoryCard`, called once from `InitSystem` (`src/main.c`), starts the card driver (`InitCARD(1)`, `StartCARD`,
`_bu_init`, `ChangeClearPAD(0)`) and opens eight events: the four outcomes of a card operation (done, error,
timeout, new card) once for the software driver (`ev0` to `ev3`, `SwCARD`) and once for the hardware (`ev10` to
`ev13`, `HwCARD`). Three ways of reading them:

- `PollSwCardEvent` tests the four software events once and returns the first that fired, or none.
- `PollSwCardEventWithTimeout` tests all four (a later one wins) and counts its calls in `event_time_out`; once the
  count reaches 120 the answer is an error whatever fired.
- `WaitHwCardEvent` loops until a hardware event fires. It has no limit.

The file calls go through the kernel's file system on device `bu00:`. `ReadMemoryCardDirectory` lists the card into
`slot_dir` (15 `DIRENTRY`s), `GetMemoryCardFreeBlocks` subtracts the listed sizes in 8 KB blocks from 15, and
`MemoryCardHasFile` compares each entry's name with the save's name (`gMemoryCardFilename`) up to the latter's end,
so a longer name that starts with it also counts. `StartMemoryCardRead` and `StartMemoryCardWrite` open the file
without waiting (`O_NOWAIT`), creating it first with its block count in the open mode's high bits when asked, clear
both event sets, issue the `read` or `write` of 0x2A0 bytes from `gSharedWorkBuffer`, and wait for the hardware
event; anything but done fails the call. Completion is left to the caller to poll. `FormatMemoryCard` calls
`format("bu00:")`.

## The probe

`StepMemoryCardProbe` finds out, one step per call, whether a usable card is present. It keeps its step in
`gMemoryCardProbeStep` and its last answer in `card_sts`, published only at the end of a round:

```
INFO       _card_info(channel); answer BUSY until published
WAIT_INFO  wait for a software event:
             done       OK; if the card's directory was read before, publish, else LOAD
             timeout    NO_CARD; publish
             new card   NEW_CARD; _card_clear(channel), wait for the hardware event; LOAD
             error      ERROR; publish
LOAD       _card_load(channel)
WAIT_LOAD  wait for a software event:
             done       the card is now loaded (gMemoryCardLoaded); the answer stays OK or NEW_CARD
             timeout    NO_CARD
             new card   UNFORMATTED
             error      ERROR
PUBLISH    card_sts = the answer; back to INFO
```

So a round takes at least three calls for a card already known and five for one just inserted, and any failure
clears `gMemoryCardLoaded`, making the next round read the directory again. The probe runs whenever
`UpdateMemoryCard` is called with no transfer under way, idle or not.

## Commands

A command starts with one of four calls, each of which sets the status and puts the machine into its probe phase
(`gMemoryCardPhase`, `MemoryCardPhase`):

| Call | Command | Used by |
| --- | --- | --- |
| `BeginMemoryCardLoad` | load | the boot auto-load, the options menu |
| `BeginMemoryCardSave` | save, refusing to replace a save | the options menu |
| `BeginMemoryCardOverwriteSave` | save over any save | the auto-save, the options menu after a yes |
| `BeginMemoryCardFormat` | format | the options menu after a yes |

`UpdateMemoryCard`, called once a frame by the state that waits, steps the probe, then the command
(`StepMemoryCardCommand`), then the transfer (`PollMemoryCardTransfer`), and returns `gMemoryCardStatus`
(`MemoryCardStatus`, sixteen values).

In the probe phase the command waits for a published answer. OK or a new card goes on to load, save or format. An
unformatted card ends the command with `MEMORY_CARD_STATUS_UNFORMATTED` unless it is a format. No card ends it with
`MEMORY_CARD_STATUS_NO_CARD` on the fourth such answer (`MEMORY_CARD_NO_CARD_RETRY_LIMIT`). An error, or the busy
answer, leaves the command waiting, and the probe tries again; nothing limits these retries.

- **Save.** List the card. If the save exists, a plain save stops with `MEMORY_CARD_STATUS_EXISTS` and an overwrite
  writes over it; if not, the file is created, unless no block is free (`MEMORY_CARD_STATUS_NO_FREE_BLOCKS`). The
  buffer is cleared and filled (`BuildMemoryCardHeader`, `EncodeSavePayload`), the write started, and the phase
  moves to waiting for it.
- **Load.** List the card; no save gives `MEMORY_CARD_STATUS_NO_DATA`; otherwise start the read and wait.
- **Format.** `FormatMemoryCard` runs to completion and the status becomes `MEMORY_CARD_STATUS_FORMAT_OK` whatever
  it returned; nothing sets `MEMORY_CARD_STATUS_FORMAT_ERROR`, though the menus handle it.

While a transfer runs, `PollMemoryCardTransfer` tests the software events each frame. When one fires it closes the
file; done gives `MEMORY_CARD_STATUS_SAVE_OK`, or for a read `MEMORY_CARD_STATUS_LOAD_OK` once `DecodeSavePayload`
has accepted the word and applied it to the options (`MEMORY_CARD_STATUS_CORRUPT_DATA` if no 17 copies agree);
anything else is a save or load error. A file call that fails to start also closes the file and ends in that error.
Each ending returns the machine to idle.

## Who drives it

- **Boot** (states 33, 34, `src/memcard.c`): `InitBootAutoLoadState` starts a load; `UpdateBootAutoLoadState` calls
  `UpdateMemoryCard` until the status is no longer saving, loading or formatting, then goes to the opening whatever
  the outcome. A save that decodes has by then restored the options and unlocks.
- **Auto-save** (states 31, 32): `InitAutoSaveState` starts an overwrite save. On a save error, no free block, no
  card or an unformatted card (and the load and format errors, which a save cannot give), `UpdateAutoSaveState`
  stops the CD, loads the options menu's art (`LoadOptionsMenuAssets`) and draws the error
  (`DrawAutoSaveErrorOverlay`) until cross is pressed on either pad, the cross button on every build rather than
  the region's confirm button. Then, or at once on success, it goes to `gAutoSaveReturnState`, which the caller
  set: the main menu after the options and after Katze's entrance (`UpdateSlashClearCutsceneState`, with the menu's
  cursor on VS), the opening after the credits (`UpdateCreditsState`, `src/story_state.c`).
- **The options menu** (`UpdateOptionsMenu`, `src/menu_state.c`): confirm on the memory card row starts a load or a
  plain save, as the row's value says, and opens the status screen, which shows the message
  `gMemoryCardStatusToMsg` gives for the status and plays a success or error sound once. A finished outcome waits
  for confirm. An unformatted card or an existing save opens a yes-or-no screen instead (the cursor starting on
  yes): yes starts a format or an overwrite save, no returns to the rows. A formatted card is not loaded or saved
  afterwards: the player starts again.
