# The computer opponent

The computer plays through a controller. `BuildFighterInput` (`src/combat_state.c`) asks
`UpdateFighterAiInput` (`src/fighter_ai.c`) for a computer fighter's frame, and the frame it returns goes through
the same triggers, queueing and buffering as a human's ([input.md](input.md)). What the computer can do is
therefore what the motion archive allows; the code decides only which buttons to press and when. Constants and
records are in `include/fighter_ai.h`; the state lives in the first fields of the `Fighter` record.

## Each frame

`UpdateFighterAiInput` runs:

1. On first use, `LoadAiAttackTables` takes the fighter's three attack lists, one per stance, from the motion
   archive.
2. `UpdateAiOscillator` advances a private angle and sets `aiRangeScale`, a factor near 1.0 by which every
   distance threshold below is multiplied: it wanders by up to 16 (1/256) either way, pulled toward 1.0 by the
   skill rating (at full skill it is exactly 1.0). It also notes whether the fighter stands more than 1000 below
   its opponent (`aiBelowOpponent`).
3. `UpdateAiPriorityOverrides` can interrupt whatever state is running (below).
4. If an input script is running, `StepAiInputScript` continues it; otherwise `DispatchAiState` runs the
   handler of `aiState` from `gAiStateHandlers` (30 states, `AiState`), after waiting out `aiDelayFrames`.
5. Pressed and released are derived from the previous frame's held buttons.

A delay or a script's hold does not count down on a frame where the fighter's animation did not advance a whole
frame, so a slowed fighter also thinks slower.

## What it reads

The computer reads the horizontal distance between the fighters (`gFighterDistanceXZ`) and its change since last
frame, the opponent's action, whether the opponent's attack has reached the end of its active frames, both
stances, whether the opponent is downed or has lost its legs, the two headings, the blade-lock meters and the
opponent's presses in a lock, and the floor 500, 1000 or 3000 units ahead, behind or to the side
(`ProbeAiTerrain`: level, a slope down, a drop of more than one 2000 band, or blocked).

## Difficulty and character

`InitFighterAi` sets `aiSkillRating` to 4096 when the difficulty option is on (hard) and 1024 when it is off.
Skill sets the delays before defending and attacking (`0x2000 / skill + 2`: 4 frames at hard, 10 at easy), scales
the attack, defence and recovery chances, narrows the random part of attack choice, and steadies `aiRangeScale`.

Each class has its own tendencies, Q12 chances in `gAiCharParamTable`; a roll succeeds when
`chance * percent / 100` exceeds a random number below 4096:

| Class | Approach | Leap | Attack | Defence | Evade | Recovery |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 Kannuki | 1024 | 1024 | 2304 | 2048 | 0 | 256 |
| 1 Mikado | 3072 | 2304 | 1792 | 2048 | 4096 | 1536 |
| 2 Black Lotus | 1792 | 1024 | 1536 | 2304 | 4096 | 1280 |
| 3 Tatsumi | 3072 | 2048 | 4096 | 1536 | 512 | 2048 |
| 4 Red Shadow | 4096 | 4096 | 1280 | 1792 | 1024 | 3072 |
| 5 Utsusemi | 1536 | 256 | 1024 | 2304 | 4096 | 256 |
| 6 | 2048 | 1024 | 256 | 4096 | 0 | 4096 |
| 7 | 1024 | 1536 | 3072 | 4096 | 512 | 4096 |
| 8 Katze | 2048 | 0 | 4096 | 1792 | 0 | 0 |
| 9 | 1536 | 1024 | 80 | 4096 | 4096 | 2048 |
| 10 | 2048 | 2048 | 1024 | 2048 | 0 | 1536 |
| 11 | 4096 | 4096 | 1280 | 4096 | 0 | 4096 |

The record's fifth field, `unusedChance` (between Defence and Evade), is left out: only `RollAiUnusedChance`,
which nothing calls, reads it.

Two modes replace these. In Slash mode the defence and recovery chances are `1024 + 102 * level` times skill,
the computer defends only on the tenth bout of a level (and then at a tenth of its chance), and
`InitFighterForScene` sets the opponent's animation speed to `0xA00 + 128 * level`, plus 0x100 on a level's tenth
bout, capped at 1.0. In the first-person VS mode they are `1024 + 3072 * wins / 10` (player one's wins, at most
10), the defence roll is made at `10 + 9 * wins` percent, and the opponent's speed is scaled by
`0xC00 + 1024 * wins / 10`, capped at 1.0. Both modes also write a scaled `clashPower`, but `UpdateFighterFrame`
resets `clashPower` from `baseClashPower` every frame, so that scaling does not last.

## Interruptions

`UpdateAiPriorityOverrides` checks, every frame:

- the opponent dead: wait (a state that presses nothing);
- the fighter staggered, heavily recoiling or dying: back to idle;
- the opponent attacking, its active frames not over, within 3000, and the defence roll passed: defend, after the
  defence delay;
- the fighter downed: the grounded state; a blade lock: the lock state, after 0 to 15 frames;
- on stages 4, 9 and 10, a drop of more than a band ahead with the opponent below: leap forward.

## Deciding

`RunAiIdleDecision` is the hub. It does nothing while the fighter's neutral clip has its first active frame at 1
(as the story opponent's intro clip does), and the first decision of a bout is always to defend (`aiForceDefend`).
Otherwise, standing:

- with a drop behind and none ahead, leap forward;
- against a downed opponent, run in from far away, else attack (or a back-and-forth attack), or dash;
- against an opponent without legs, run in and attack;
- once in 512 frames, try to retreat or leap (evade, 25 percent), to leap (33), or to flank (approach, 50);
- in Slash bouts before the tenth, return to the middle stance whenever out of it; otherwise, once in 256
  frames, change stance: R2 from high, R1 from low, either from middle;
- 10000 or more away, run in if the approach roll passes; 5000 or more, at a tenth of it; standing below the
  opponent, flank at 5 percent;
- facing within a quarter turn of the opponent's own heading, if the evade roll passes, leap or sidestep;
- else, if the attack roll passes, attack after the attack delay.

Crouching hands over to the crouched states; staggering, a passed recovery roll presses R2.

**Choosing an attack** (`SelectAiAttackScript`): for playable characters whose evade roll fails, one time in
eight a carried sub-weapon is thrown, or one in sixteen a handful of the ground. Otherwise every entry of the
current stance's list (`AiAttackChoice`) is scored: the gap between its preferred distance (in units of 20) and
the actual distance, plus its penalty times 32, plus a random amount up to 8190 scaled by `1 - skill / 2`; an
entry flagged for a downed or a crippled opponent scores half when that is so. The lowest score is played; an
empty list sends the fighter running in. An entry may be limited to some classes by a mask at its script's
start. In Slash bouts before the tenth, levels 0 to 8 each use one fixed script instead (`gAiSlashAttackScripts`;
level 9 scores the list, so the table's tenth script is never used).

**Defending** (`RunAiDefendOrEvade`): the fighter presses square (`PADRleft`) until the opponent's active frames
pass or it is no longer guarding. The routine would choose a sidestep or backstep (`gAiEvadeScripts`) when the
opponent's swing is vertical, but `IsWeaponSwingPlaneVertical` never returns true
([oddities.md](oddities.md)); so the computer always guards, and those evasions are reached only from the other
states.

**Moving**: running in holds right and L1 (and R1 once running), flanks when blocked or stuck, presses triangle
within 1200 and attacks a standing opponent when the distance is changing fast; flanking holds L1 with left
and a side for about ten frames, then right and the same side for up to 90; on the ground the computer rolls to
one side or the other by where its opponent is, strikes, or rises.

**Blade lock** (`RunAiBladeLock`): pressing circle at half the attack chance each frame; after the opponent
slips while the computer leads, it waits 8 frames; once the opponent has the advantage, it tries to slip away
with left at 3 percent of its defence chance a frame.

## Scripts

Input scripts are byte streams read by `StepAiInputScript`: a button number 0 to 15 presses it, the same with
0x10 added releases it, 0x80 plus a count holds the buttons for that many frames, 0x80 alone ends the script,
and 0x40 followed by a 16-bit class mask is skipped. The source has 35 fixed scripts (sidesteps as double taps
of up or down, a backstep and a dash as double taps of left and of right, a backflip as a triple tap of left, a
leap as right, R2, R1, throws as right, R2 and circle, Katze's shots, the Slash levels' attacks); the attack
lists in the motion archive supply the rest. Nothing reads two of them, `gAiScriptLeapForwardQuick` and
`gAiScriptSingleTriangle`.

## Katze

Katze ignores the attack lists. Its decisions (`RunAiIdleDecision`): out of rounds, within 3000 it retreats half
the time (running, or one time in 16 by a backflip), and otherwise presses select half the time. With rounds
left, when their number is at most a random 1 to 4, one time in 16 it backflips if within 3000 and else presses
select. Otherwise it attacks if the attack roll passes at 60 percent; else, each on a coin toss after the last, it
backflips, sidesteps up, sidesteps up (the same state again; [oddities.md](oddities.md)) or runs back, or does
nothing. Its attacks (`SelectAiAttackScript`) are R1 with circle
against a downed opponent, R1 with triangle or a double circle within 3000, and further off one of the eight
entries of `gAiKatzeAttackScripts`: seven scripts, the double circle listed twice.

## Unreached modes

`gAiMoveCycleMode` makes the computer mirror its opponent's stance and play the stance's attack list in order. It
is set only when main-menu entry 11 is chosen, and the menu's cursor covers entries 0 to 7 (8 in Japan with an
analog pad), so no retail game turns it on ([game-flow.md](game-flow.md)). `UpdateFighterAiInputNoOp` is empty.
