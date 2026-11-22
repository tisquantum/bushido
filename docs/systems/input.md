# Input

Buttons never name moves in the code. A controller frame reaches a fighter as four bit masks, and the motion
archive's triggers decide what each button does in each placement ([fighter.md](fighter.md)). Three things
stand in for a controller: the computer opponent, the link cable and scripted cutscenes.

## The controller frame

`PollControllers` (`src/pad.c`) runs first in every pass of the main loop. It reads the two ports' receive
buffers (`gPadReceiveBuffers`, handed to `InitPAD` in `src/main.c`) and builds a `PadInputFrame`
(`include/pad.h`):

| Field | Holds |
| --- | --- |
| `type[2]` | the controller kind from the pad's id byte (`PadType`); a missing pad reads as digital |
| `connected[2]` | outside Japan only: whether each port answered |
| `held` | buttons down now: port 1 in bits 0 to 15, port 2 in bits 16 to 31, `libetc.h`'s `PAD*` bits |
| `pressed` | down now and not last frame |
| `released` | down last frame and not now |
| `notHeld` | not down now |
| `analog[2][4]` | each port's right stick X and Y, then left stick X and Y, 0x80 centred |

A mouse, a Namco gun or a multitap gives no buttons. The result is kept in `gPadState`; menus read it directly.

The builds differ. In Japan, analog pads keep their type, their sticks are scaled to calibrated centres and
ranges (`CalibratePadSticks`; the analog controller menu captures the centres with `CapturePadStickCentres`), and
during a bout the left stick also presses the d-pad and the right stick the four face buttons, past 96 from
centre (`StickToDpadButtons`, `StickToFaceButtons`). Elsewhere both analog types are reported as digital, so the
analog steering in `UpdateFighterRunSteering` (`src/fighter.c`) is reached only in the Japanese build.

## From pad to fighter

Each bout frame, `BuildFighterInput(player, out)` (`src/combat_state.c`) makes one fighter's frame:

1. In Story mode, while the challenger banner is up, the input is empty.
2. A computer fighter's frame comes from `UpdateFighterAiInput` ([ai.md](ai.md)).
3. Otherwise the player's 16 bits are taken from `gPadState`. For player two, outside Link play, the d-pad is
   mirrored (left with right, up with down) and the left stick inverted (`REVERSE_DPAD`).
4. The held buttons are pushed into `gFighterInputHistory`, an eight-frame history per player that
   nothing in the source reads.

`UpdateFighterFrame` then copies the frame into `Fighter.currentInput`, a `PadInputFrame` like the pad state. On
a fighter's first frame of a bout the buttons are cleared. In the first-person view the d-pad of a human fighter
is turned a quarter (`ROTATE_DPAD_POV`): up acts as right, the direction toward the opponent in the third-person
view, and so on round.

## Buttons into moves

Every placement lists triggers; each names a button mask, an edge (pressed, released, held or not held) and the
placement it leads to. While the current frame is below the placement's `triggerLimit`, `UpdateFighterFrame`
tests them in order and takes the first that matches. Where the code itself reads the d-pad, as in steering a
run (`gDpadAngleTable`), right is toward the opponent, left away, up and down to either side. Outside Japan, a
trigger fired by select alone also shows the character's taunt subtitle.

Three things carry input across frames:

- **Queueing.** A placement with `canQueue` set stores the first trigger that fires instead of obeying it; the
  stored placement starts when the frame reaches `triggerLimit`, or at the end of the span. Further presses are
  not read while one is queued.
- **Buffering.** During the last 11 frames of a recoil, stagger, heavy recoil, special or type-8 action, every
  press is collected in `Fighter.bufferedButtons`; on the frame the next placement starts, the collection is
  added to that frame's presses, so a button pressed while recovering acts as soon as recovery ends
  (`BUFFERED_INPUT_ACTIONS`, `include/fighter.h`).
- **Held state.** Triggers on held and not-held buttons fire on every frame the condition holds.

Some input is read by code rather than by triggers: R1 with a direction against a wall climbs; triangle, circle
and cross fill the blade-lock meter and left, up and down act in a blade lock ([combat.md](combat.md)). In the
first-person view, left and right on the pad add 32 a frame to a standing fighter's `turnRemaining`, and, when
the opponent stands still 8000 or more away, set a walking fighter's to 256 (one walking while crouched, to 128), turning
it in place. Each frame the heading takes `turnRemaining` times the animation speed over 0x6000, a sixth of it
at normal speed.

## Outside a bout's moves

- **Pause**: start (either pad's when two play), from the fifth frame of a bout. While paused, holding start and
  select (outside Japan also L1, L2, R1 and R2) on one pad quits to the main menu (`UpdateCombatState`).
- **Challenger**: player two's start during a Story bout halts it and opens character select for two players
  ([game-flow.md](game-flow.md)).
- **Skipping**: the region's cancel button (`MENU_CANCEL`, `include/pad.h`) ends the replay, the
  victory scene and the story cutscenes early.

## Rumble

`TriggerFighterRumble` (`src/fighter.c`) sets a duration in frames and a duty from `gPrimaryRumbleTable` (12
presets: fatal hit, wound, clash, parry, guard break, landing, body bump, debris hit, gunshot, prop cut, wall
hit); the first five also shake the opponent's pad from `gOpponentRumbleTable`, unless the opponent holds the gun.
Each frame `UpdateFighterFrame` copies a human fighter's rumble into `gRumblePulses`, and
`UpdateControllerRumble` (`src/pad.c`), called every vertical blank, turns the duty into on and off pulses of the
motor, stopping a pulse train after 60 blanks. The option `gOptionRumbleEnabled` gates the motor. It defaults on in
Japan and off elsewhere (`ResetGameOptions`, `src/main.c`), and only the Japanese analog controller menu changes
it, so outside Japan the motor never runs unless a save says otherwise.

## The computer as a controller

`UpdateFighterAiInput` (`src/fighter_ai.c`) returns the same frame a pad would: it computes held buttons from a
script or the behaviour states and derives pressed and released from its previous frame. Its fighter therefore
moves through exactly the same triggers, queueing and buffering as a human.

## The link cable

Two consoles joined by a link cable each run the whole game; they stay identical by exchanging their controllers
every frame inside `PollControllers` (`ExchangeLinkInputPacket`, `src/link.c`). The lobby (`UpdateLinkLobbyState`)
opens libcomb's serial device at 2,073,600 baud and settles the roles by the handshake lines: the console that
sees clear-to-send already raised is the client. A session forces Link mode, two players and the first-person
view.

Each frame each console sends an 8-byte `LinkPacket` (`include/link.h`): its role, a disconnect request, its sync
phase, its daytime option, its pad type, its 16 buttons, its left stick, and a 16-bit XOR checksum of the upper
halves. The host waits for the client's packet, then sends its own; the client sends after reading. The host
builds `held` as the client's buttons in the high half and its own current buttons in the low half; the client
puts its own previous packet in the high half and the host's in the low half. Both consoles therefore act on the
same pair every frame: the host's buttons of this frame and the client's of the frame before. Only the left
stick and pad type of each side cross the cable; pressed and released are derived afterwards as for a local pad.
The client takes the host's daytime option.

Around every load, `BeginLinkSyncPause` and `WaitLinkSyncBarrier` exchange idle packets until both sides reach
the barrier. A bad checksum, a write that times out, or a read that fails five times sets `gLinkErrorFlag`, and
the main loop sends the game to the link abort state. Two counters of the button bits sent and received
(`gLinkTxButtonBitCount`, `gLinkRxButtonBitCount`) are kept and never read.

## Scripted input

Cutscenes drive fighters by building frames in code: the approach cutscene walks player one forward with
d-pad, L1, R2 and R1 on fixed frames (`UpdateApproachCutsceneState`, `src/story_state.c`), the Slash clear has
Katze press left twice and then triangle (`UpdateSlashClearCutsceneState`), and the victory and Continue screens
run both fighters on empty frames.

## The replay

The instant replay does not replay input. `RecordFighterReplayFrame` (`src/replay.c`) stores each fighter's
placement, frame, position, heading, look angles and face for each of the last 256 frames, and playback
decodes poses from those snapshots ([match-rules.md](match-rules.md)).
