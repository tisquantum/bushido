# Audio

Two separate machines make the game's sound. Effects and voices are samples in VAB banks, held in the sound
processor's memory and played through Sony's LIBSND on 24 voices that the game allocates itself
(`src/sound.c`). Music is XA audio streamed from the disc by the game's CD driver (`src/cd.c`). Positional
sound and the stage's ambience sit on top of the first (`src/combat_event.c`, `src/combat_state.c`). Movies
carry their own sound ([files-and-memory.md](files-and-memory.md)).

## Banks

`gSoundVabIds` holds the VAB of each of eleven bank numbers, loaded per need from the sound archives
(`SE0/` to `SE5/`, category 2 of the disc's file table):

| Bank | Holds | Archive (sound file index) | Load mode |
| ---: | --- | --- | --- |
| 0, 3, 4 | The bout's pair of weapons | the entry of `gWeaponPairSoundTable` whose mask has both weapons' bits (files 0 to 44); Practice uses the entry for weapon 9 alone | `SOUND_LOAD_WEAPON_PAIR` |
| 1, 2 | Player one's fighter: voice and effects | 45 + fighter id (fighters 18 and up use 45) | `SOUND_LOAD_FIRST_FIGHTER` |
| 5, 6 | Player two's fighter | 45 + fighter id | `SOUND_LOAD_SECOND_FIGHTER` |
| 7 | A story scene's dialogue, chosen by the pair of characters; closes banks 0 to 6 | from 68 | `SOUND_LOAD_DIALOGUE` |
| 8 | Menus | 122 | `SOUND_LOAD_MENU` |
| 9 | The stage's ambience | 123 + `gStageAmbientSoundFile[stage]`; some stages have none | `SOUND_LOAD_STAGE_AMBIENT` |
| 10 | The link lobby | 63 | `SOUND_LOAD_LINK_LOBBY` |
| 5 to 10 | Slash mode's enemies, replacing the second fighter's banks | 146 | `SOUND_LOAD_SLASH_MODE` |

A load is two calls with one `SoundLoadRequest` (`include/sound.h`): `RequestSoundBankLoad` starts
the read of the archive into a staging buffer, and once the drive is idle `CommitSoundBankLoad` copies the
archive's VAB headers into a resident header buffer (`gSoundVabHeaderWork`, 0x8000 bytes; Slash mode's at
0x801EF000) and hands each header and body to `TransferSoundVabSet`, which opens the header (`SsVabOpenHead`) and
sends the body to the sound processor (`SsVabTransBody`), waiting for each transfer. The staging buffer for a
bout is the dynamic load area at 0x8014C000, used before the bout's motion archive is read over it
(`LoadCombatSoundBanks`, `src/combat_state.c`). The archive's own layout is in [formats.md](../formats.md).

`InitSoundSystem` starts LIBSND afresh for a stage (`SsInit`, 60 ticks a second, a sequence table of one
entry, though the game's code opens no sequence), picks the sound map, and turns reverb on for two stages: hall
reverb on Underground Grotto (25), room reverb on Meikyokan (15), at depths 0x40 and 0x20 (`StartSoundSystem`).

## The voice queue

The game does not let LIBSND choose voices. `gSoundVoiceQueue` has one 6-byte entry per voice (`SoundVoiceState`,
`include/voice_state.h`: a sound id and two volumes), and `gSoundVoiceCooldowns` a timer per voice:

| Voices | Use | Queued by |
| --- | --- | --- |
| 0 | Player one's voice, story dialogue, the pause and interface cues | `QueuePriorityVoice0` |
| 1 | Player two's voice | `QueuePriorityVoice1` |
| 2 to 16 | Effects | `QueueSoundEffect` |
| 17 to 23 | The stage's ambience | `QueueSoundOnVoice` |

`QueueSoundEffect` takes the first effect voice whose cooldown has run out and sets the cooldown to 30 frames;
when all fifteen are cooling down the sound is dropped. The voices 0 and 1 have no cooldown: a new line simply
replaces the old. Volumes are scaled by 114/128 as they are queued. The main loop counts the cooldowns down each
frame (`TickSoundVoiceCooldowns`, [frame.md](frame.md)).

Nothing sounds until `FlushQueuedSounds` runs, which each state calls once near the end of its frame. For every
voice with a queued sound it keys the voice off, looks the sound up in the active map (`gNormalSoundMap`, 134
entries, or `gSlashSoundMap`; each a `SoundMapEntry`, `src/sound.c`: a bank in six bits with two bits choosing
the program, and a tone) and keys it on (`SsUtKeyOnV`) with that bank's VAB, program and tone at note 60 and the
queued volumes. So a sound's identity is its index in the map, and which sample plays depends on which archive
is in the bank at the time. Slash mode rewrites the bank numbers of its map for the current enemy
(`PatchSlashWeaponBankIds`, `PatchSlashEnemyBankIds`).

## Positional sound

`PlaySpatialSound(sound, position)` (`src/combat_event.c`) plays a sound from a point in the world. The listener
is the battle camera's eye, worked out from `gCameraState`'s target, yaw and distance; height is ignored. The
distance comes from half the X and Z differences through `gSqrtLutQ3`, and a source more than 50,000 units away
on either axis is not played. The volume is 127 within 5,000 units, falling linearly to 0 at 50,000; the ear
turned away gets the volume squared over 128. Which ear is which comes from the bearing of the source relative
to the camera's yaw.

The sound argument also says where the sound goes: below 0x100 an effect voice; 0x100 to 0xFFFF voice 0 when the
high byte is 1 and voice 1 otherwise; from 0x10000 a named voice (bits 8 to 15) is stopped and restarted, with
the volume also panned by the bearing; from 0x20000 only that voice's volume is changed. Footsteps, wounds,
clashes and the animation events that call it are [combat-events.md](combat-events.md)'s.

## Ambience

Each stage has a list of `StageAmbientSound` entries (`gStageAmbientSoundLists`, `src/combat_state.c`;
`include/stage_ambient_sound.h`), entry `i` playing sound 0x79 + `i` on voice 17 + `i` from bank 9. An entry's
flags choose a loop started with the stage, a random trigger, or a random trigger that also fires the next
entry, and whether it sounds by day, by night or both. A random entry waits at least its delay in seconds, then
each frame fires with a chance of 1 in 30 times its divisor (`UpdateAmbientSounds`), at its volume with the left
channel up to a quarter quieter at random. During a story cutscene the ambience plays at half volume. Stage 0's
first two entries are placed in the world through `PlaySpatialSound`; Yahiro Road (stage 24) runs its pair by a
small state machine of its own by day.

## Music

The four files of `XA/` each interleave four channels, so the game has 16 tracks; track `t` is channel `t & 3`
of file `t >> 2` (`XaTrack`, `include/cd.h`). The named ones: the prologues from 0, the prologue introduction at
6, the opening at 8, the select screen at 9, the credits at 10, the dishonourable ending at 11, two VS themes at
12 and 14, Practice at 13 and Slash mode at 15.

`StartXaTrack` (`src/cd.c`) sets the CD filter to the channel and puts the driver into its audio states: set the
drive's mode for real-time XA through the channel filter, seek, then read. `UpdateCdSubsystem`, called at the
top of every frame, steps the state machine one state at a time, so music never blocks the game. Once playing,
it sets the mix to the track's volume from `gXaTrackVolumes` (0x70 to 0x7F; not while the game is paused) and
from then on reads the drive's position every other frame. Each track has an end sector: the file's length less
150 sectors, or for tracks 12 to 15 a length in `gBgmTrackSectorCounts` (16,762, 18,938, 18,825 and 24,487
sectors). Within 75 sectors of the end a countdown takes over; at the end the mix is muted and the track either
stops or, when repeat is on (`SetCdAudioRepeat`), starts again from its first sector. The drive does one thing
at a time: a file read cannot start while music streams, so loading code stops the music first
(`StopAndWaitCdIdle`).

A bout's music is chosen by `StartCombatBgm` (`src/combat_state.c`) when the music option is on: the Slash
theme, one of the two VS themes by alternate rounds (none in the first-person view), or the Practice theme, all
repeating; Story and Link bouts have none. Other states start their own tracks (`src/menu_state.c`,
`src/opening_state.c`, `src/story_state.c`), `CueXaTrackPaused` readying one to start later. CD-DA playback (a
negative channel) and volume fades (`StartCdAudioMixFade`: the four mix values move linearly over a number of
frames) are also supported by the driver.
