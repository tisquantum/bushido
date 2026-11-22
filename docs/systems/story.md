# Story mode and the code of honour

Story mode is one player against a fixed series of computer opponents, with cutscenes between bouts, three kinds
of ending, and a code of honour the game checks in a few precise places. It runs on the game states listed in
[game-flow.md](game-flow.md); the state routines are in `src/story_state.c`, `src/replay.c` and
`src/combat_state.c`.

## Progress

Choosing a character in Story mode calls `ResetStoryProgress` (`src/menu_state.c`): stage 0, bout 0, honour
intact (`gBushidoHonorIntact`), the no-damage flag set (`gStoryNoDamageFlag`), no opponents beaten, no wounds
shown. The character's prologue plays (`InitStoryPrologueState`, a picture slideshow with its own music), then
the first bout.

`gStoryBoutIndex` counts bouts won. `InitCombatState` (`src/combat_state.c`) picks the opponent from
`gStoryOpponentTable[class][bout]` (fighter id minus 6) and, for bouts 0 to 4, the weapon from
`gStoryOpponentWeapons`; later opponents bring their own weapons ([weapons.md](weapons.md)). Bouts 0 to 4 are the
five other playable characters, as fighter ids 6 to 11 (the set Story mode gives the player too):

| Player | Bouts 0 to 4 (weapon) | Bout 9 |
| --- | --- | --- |
| Kannuki | Red Shadow (nodachi), Utsusemi (sledgehammer), Black Lotus (broadsword), Tatsumi (katana), Mikado (naginata) | Black Lotus, long sword |
| Mikado | Kannuki (long sword), Utsusemi (broadsword), Black Lotus (rapier), Tatsumi (katana), Red Shadow (saber) | class 6 |
| Black Lotus | Mikado (katana), Red Shadow (saber), Utsusemi (broadsword), Kannuki (sledgehammer), Tatsumi (nodachi) | class 7 |
| Tatsumi | Black Lotus (saber), Mikado (rapier), Utsusemi (katana), Red Shadow (long sword), Kannuki (naginata) | class 7 |
| Red Shadow | Mikado (saber), Black Lotus (sledgehammer), Tatsumi (katana), Kannuki (katana), Utsusemi (long sword) | class 6 |
| Utsusemi | Tatsumi (sledgehammer), Mikado (long sword), Red Shadow (katana), Kannuki (nodachi), Black Lotus (rapier) | class 6 |

Bout 5 is Katze for every character, and bouts 6, 7 and 8 the bosses of classes 10, 11 and 9 (the source names
them `FIGHTER_CLASS_BOUT6_BOSS` and so on by the bout they appear in).

**Where.** Bouts 0 to 3 are fought on the stages joined by portals, starting on stage 0: each bout begins on
the stage where the last one ended, and the fighters may cross into neighbouring stages during it
([match-rules.md](match-rules.md)); a bout that starts on stage 4 or 10 picks its spawn points by the height
player one ended at. After bout 3 an approach cutscene plays (`UpdateApproachCutsceneState`): 151 frames on
stage 3 and 151 on stage 9, or only the second, by where bout 3 ended (`gApproachCutsceneStartPart`); bout 4 is
on Underground Grotto (25). Bouts 5 to 9 are on stage 11, Executioner's Cove (12), stage 13, Meikyokan (15)
and Yagura Point (17) (`gStoryBoutStageIds`). Each beaten opponent's class is recorded in `gStoryDefeatedMask`,
which decides whether an opponent appears with every wound texture (always for Kannuki's bout 9).

**Between bouts.** After a won bout and its replay, `TransitionAfterMatchOrReplay` (`src/replay.c`) runs the
story cutscene of the bout just won (`InitStoryCutsceneState`, [story-presentation.md](story-presentation.md));
when it ends or is skipped,
`UpdateStoryCutsceneState` starts the next bout, the approach cutscene, or an ending. A lost bout goes to the
Continue screen, or with continues switched off to the main menu.

## The code of honour

`gBushidoHonorIntact` is cleared in exactly three places, all in `UpdateFighterFrame` (`src/fighter.c`), and all
concern the computer opponent:

1. it receives any forced reaction (struck, parried, guard broken, recoiling from a clash, bumped) while the
   **honour window** is open;
2. it starts a fall while the window is open;
3. while it is **climbing**, it is staggered, wounded, killed or made to recoil, at any time in the bout.

The window opens at the start of every Story bout with `gStoryHonorWindowTimer` at 5 and is run by
`UpdateStoryHonorWindow` (`src/match_setup.c`):

- it does not count while the opponent is still in its intro clip (a neutral clip whose first active frame
  is 1), nor on frames where player one is attacking;
- it closes at once when the opponent attacks;
- it does not count down while player one stands behind the opponent, facing the way the opponent faces;
- when it reaches 0 the opponent's ready voice plays (`COMBAT_EVENT_STORY_READY_VOICE`).

So striking an opponent still in its intro clip or not yet readied costs honour, as does striking it from
behind while the window is held open, or at any time while it climbs a ledge. Throwing surface debris costs honour
only by the same rules: the burst staggers the opponent, which counts as a forced reaction if the window is open.
Nothing else is checked.

Losing honour has no effect until bout 4 or a later one is won: from then on every win checks it (after the
story cutscene, or directly after bouts 8 and 9), and a story without honour ends in the dishonourable ending
instead of continuing.

## Endings

After bout 8 is won (`gStoryBoutIndex` 9), `TransitionAfterMatchOrReplay` decides:

```
honour lost                                  -> dishonourable ending
honour kept, IsTrueEndingEligible()          -> cutscene, then bout 9 -> true ending
honour kept, not eligible                    -> normal ending
```

`IsTrueEndingEligible` asks for three things: player one never struck by a blade, a gunshot or a sub-weapon in
the whole story (`gStoryNoDamageFlag`, cleared in `ApplyFighterHitImpact` and `CheckSubWeaponFighterHits`), no
continue taken (`gContinueCount` 0), and stage 9 reached during bouts 0 to 3 (`gReachedStage9Flag`, set when
a Story bout starts on stage 9 or the fighters cross into it). Bout 9 itself is fought only on this path, and
losing honour in it also ends in the dishonourable ending.

- **Normal and true endings**: `PlayStoryEndingMovieState` plays movie `class + 1` (normal) or `class + 7`
  (true), marks it in `gUnlockedMoviesMask` (bits 0 to 5 normal, 6 to 11 true) for the movie theatre, and goes
  to the staff credits; after them the game saves if auto-save is on and returns to the opening.
- **Dishonourable ending**: `InitBadEndingSlideState` shows one of five pictures at random with its own music;
  cancel returns to the main menu. No movie is unlocked and no credits roll.

## Special results

- **Ring out.** On stages 4, 9 and 10 a fighter that drops below a set height (4000, 3000 and -2000) starts
  `pitFallTimer`. When one fighter's timer leads the other's by more than 150 frames (the opponent's) or 210
  (player one's), `UpdateCombatState` ends the bout as `STORY_EVENT_RING_OUT`, counts it as won by player one
  (whichever fighter fell), advances the bout and goes straight to the victory scene. It also increments
  `gStoryFallCount`, which nothing reads.
- **Stage 9.** When player one falls on stage 9, the bout is restarted on stage 25 (`STORY_EVENT_STAGE9_P1_FALL`,
  or `STORY_EVENT_STAGE9_BOTH_FALL` if both fell). If the opponent was already dying or legless, the story skips
  to bout 4; otherwise the next won bout skips there (`gStoryStage9FallSkip`).

## A challenger

Player two pressing start during a Story bout (while neither fighter is dying) or on the story's character select
halts it: a banner shows for 60 or 90 frames, then character select opens for two players
(`gStoryChallengerActive`). The fight is a versus bout on the current stage. If player one wins, the story
resumes where it was (through the Continue screen when continues are on); if player two wins, the story is over
unless a continue restarts the fight.
