# The rules of a match

A bout is game states 1 and 2: `InitCombatState` and `UpdateCombatState` (`src/combat_state.c`). It has no
clock, no rounds and no health: it ends when a fighter dies, and in Story mode also when a fighter falls out of
the stage.

## The start

`InitCombatState` loads the sound banks ([audio.md](audio.md)), both fighters' models ([models.md](models.md)),
the motion archive for their pair of weapons and the stage ([stages.md](stages.md)). `InitFighterForScene`
(`src/fighter.c`) places each fighter and resets it for the bout: no wounds, both arms and legs intact, its
sub-weapons, rounds for the gun, speeds and clash power from the tables ([weapons.md](weapons.md)), and the
computer's state when it is not a player. Where it stands:

| Mode | Spawn points |
| --- | --- |
| VS | one of the stage's four pairs at random (`gStageSpawnPositions`, `src/stage_tables.c`) |
| Story | pair 0; on stages 4 and 10 pair 0 or 2 by the height player one ended the last bout at |
| Slash | player one at the origin, each opponent 5000 away along Z, to either side at random after the first |
| Practice | pair 0 at first; on restart near player one ([game-flow.md](game-flow.md)) |
| Cutscenes | the approach and the Slash clear have their own positions |

The fighters face each other. Each enters its pair slot's start placement from the motion archive; a Story
opponent enters its class's intro placement instead (`storyIntroPlacements`), unless it is a challenger. The
computer's first decision is always to guard ([ai.md](ai.md)), and in Story mode the honour window opens
([story.md](story.md)). Blades, walls and props are not tested for clashes until the fourth frame.

## During the bout

`UpdateCombatState` runs, each frame: pause, camera, both fighters' input and frame, collisions, thrown items,
the honour window, stage events, the frame's drawing, sound and subtitles, then the mode's checks and a stage
change ([frame.md](frame.md)). Nothing counts down. A bout can be
paused from its fifth frame ([input.md](input.md)).

## The end of a bout

**A kill.** The first time either fighter is in a dead (19), fatal (5) or defeated (21) action, or in a
placement flagged fatal, `UpdateFighterFrame` records the other side as the winner in `gMatchWinnerSide`. Player
one's frame runs first, so in a mutual kill on the same frame the winner recorded is player two; the routing
below treats a mutual kill as a loss for both. With one human player, a fighter in the defeated action sets the
winner again on every frame, over any earlier record. The defeated action is never set by code: it comes from
the motion archive. A defeated fighter dies from any hit ([combat.md](combat.md)).

**The count.** From then on `UpdateCombatDistanceAndKoTimer` (`src/match_setup.c`) advances `gPostKoTimer` each
frame, every other frame when exactly one fighter is defeated. At 106 frames the bout ends:

| Mode | At 106 frames |
| --- | --- |
| VS, Link | the replay |
| Story | the replay; a win first advances `gStoryBoutIndex` |
| Slash | player one lost: the replay. Player one won: the next opponent at 61 frames (106 after a level's tenth), the replay after level 10 |
| Practice | nothing; a win is counted at 105, and a face button restarts both fighters |

**Falls.** Only Story mode ends bouts on a fall: on stages 4, 9 and 10 a fighter's fall starts
its `pitFallTimer`, and either a ring-out or a restart on stage 25 follows ([story.md](story.md)). In the other
modes a fall ends nothing by itself.

## After the bout

**The replay** (17, 18; `src/replay.c`) shows the last 195 frames (six and a half seconds; 7.8 in Europe) from a
ring of 256 frames of per-fighter snapshots, with the combat events re-fired from 64 recorded marks and thrown items
flown again from their history; its camera is described in [camera.md](camera.md). The winner's cancel button
(player one's with one human player) ends it early. Then `TransitionAfterMatchOrReplay` routes:

| Result | Next |
| --- | --- |
| both dead | Continue, or the main menu with continues off (Link: always Continue; a Story challenge: the victory scene) |
| VS, Link | the victory scene |
| Slash, player one won (level 10 cleared) | the victory scene |
| Slash, player one lost | Continue or the main menu |
| Story | the story's next step or ending ([story.md](story.md)) |

**The victory scene** (19, 20; `src/story_state.c`) plays the winner's victory placement (the legs-lost one if
its legs are crippled) for up to 150 frames; the winner's cancel ends it. Then VS goes to Continue (or the main
menu with continues off), Link to Continue, Slash to Katze's entrance or the main menu, and Story to its next
step ([story.md](story.md)).

**Continue** (27, 28) counts down 300 frames (ten seconds; twelve in Europe), with the music fading. The loser's pad
confirms (player one's with one human player, either pad after a mutual kill); 60 frames later the bout starts again
with the same fighters, models and stage (`gIsContinueRestart`), and `gContinueCount` goes up. Cancel takes a second
off the countdown. In VS and Link, continuing also adds the bout to the win counts (`IncrementVsWinCounts`: one to
the side opposite each dead fighter), which are shown from then on. When the countdown runs out the game returns to
the main menu, except after a Story challenge won by player one, when the story resumes. A continue taken in Story
mode rules out the true ending, and in Slash mode Katze.

## Moving between stages

Twelve stages are joined by 27 one-way portals (`gStageTransitions`, `src/stage_tables.c`;
`StageTransition`, `include/stage_tables.h`):

| Stage | Portals lead to |
| ---: | --- |
| 0 | 10 |
| 1 | 3, 10 |
| 2 | 3, 10 |
| 3 | 1, 4 (none back to 2) |
| 4 | 3, 5, 7 |
| 5 | 4, 6 |
| 6 | 5, 7, 8 |
| 7 | 4, 6 |
| 8 | 6, 9 |
| 9 | 8 |
| 10 | 0, 1, 2, and 22 twice |
| 22 | 10 twice |

A portal is a box on its source stage and a pair of origins, one in each stage's coordinates.
`CheckStageTransition` (`src/combat_state.c`) takes it only in Story and Slash modes (Slash's stage 20 has none),
outside a blade lock, when both fighters are standing, crouching or downed and both stand inside the same box
(`FindActiveStageTransition`, `src/fighter_hit.c`). The bout does not end: `gGameState` returns to
`InitCombatState` with the new stage, which loads it and its ambient sounds but does not reset the fighters.
`InitFighterForScene` moves each fighter, its anchor and its weapon's last points by the difference of the two
origins, keeping it at least 100 units inside the portal's box; the camera moves by the same amount. Wounds,
motions and the honour flag carry over, and the honour window is not reopened. Thrown items are not moved: each
keeps the stage it was thrown on and is drawn only there. VS,
Practice and Link never change stage, even on stages with portals.
