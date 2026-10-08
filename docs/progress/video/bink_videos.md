# Bink videos

The game ships five Bink videos: two start-up pictures, a pre-intro, the story's intro film, a strip of tip pictures for
the loading screen and the film behind the creature's fall at the end of the game. This file is what each is for and
when the game shows it; how they are decoded and drawn is in [bink_playback.md](bink_playback.md). None of them plays in
openblack yet.

**Progress: 0/29 done, 1 partial — 2%**

## The files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Data/logo.bik`: the publisher's and studio's pictures, two still frames | todo | looked for on the CD's root first, then in the install's data folder; see [bink_playback.md](bink_playback.md) |
| `Data/pre_intro.bik`: the pre-intro film, about 100 seconds | todo | looked for on the CD's root first; see [bink_playback.md](bink_playback.md) |
| `Data/INTRO.bik`: the story's intro film, about 67 seconds | todo | see [bink_playback.md](bink_playback.md) |
| `Data/tips.bik`: 35 still pictures for the loading screen's tips | todo | see [bink_playback.md](bink_playback.md) |
| `Data/Spells/fall/fall.bik`: the film behind the creature's fall, 50 seconds | todo | see [bink_playback.md](bink_playback.md) |
| No other videos ship with the game; Creature Isle is not supported | n/a | searched `C:\projects\black`; no expansion videos |
| The videos have no sound of their own: every sound with them comes from the game's music and sound banks | todo | see [bink_playback.md](bink_playback.md) |

## Start-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The logo pictures show at every start of the game, before the front end | todo | openblack goes straight to the land; see [bink_playback.md](bink_playback.md) |
| The pre-intro plays on a first run, before any player profile exists, with the trailer music | todo | openblack keeps no profiles (../interface/profiles.md) |
| The pre-intro ends at its last frame or on a key press, and the trailer music stops with it | todo | see [bink_playback.md](bink_playback.md) |
| The cursor is hidden while the pre-intro plays | todo | see [bink_playback.md](bink_playback.md) |

## The loading screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a land loads, one of the tip pictures shows with a tip of the day under it | todo | openblack has no loading screen |
| The tip is picked at random by the time; with player profiles already made, two first-run tips are skipped | todo | (unconfirmed which two tips those are) |
| The tip's text comes from the game's help texts, with its control codes blanked out | todo | the texts load (`src/Gui/TextDatabase.h`) |
| The picture fades in, with the game's version shown in a corner and a progress bar | todo |  |
| The tip picture is cleared when a full-screen film starts | todo | see [bink_playback.md](bink_playback.md) |

## The story's intro film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the start of a new game, in the first land's opening scene: after the boy is saved from the sharks the screen fades to black and the film plays | todo | the opening scene's script asks for the intro film; the script command that plays a film is an empty stub in `src/CHLApi.cpp`; there is no video player in openblack |
| When it ends the opening carries on: a fireball streaks across the sky, the camera follows it and the family is left on the beach | todo | see ../story/land_1.md and ../story/cutscenes.md |
| It fades out two seconds before its end and the screen fades back from the script's fade | todo | see [bink_playback.md](bink_playback.md) |
| Skipping the intro film on a new player's first game is not allowed | todo | see [bink_playback.md](bink_playback.md) |
| The game is paused and the cinema bars are on while it plays | todo | the bars exist (../camera/cinematics.md); see [bink_playback.md](bink_playback.md) |

## The creature's fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the end of the game, when the creature has climbed the volcano and leapt in | todo | the ending's script asks for it with sound effects turned off around it; the script command that plays a film is an empty stub in `src/CHLApi.cpp`; there is no video player in openblack; see ../story/ending.md; the scene round it: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The player's creature is drawn falling in front of the film, with glows on its hands, sparks and a burst of light | todo | the sparks and light burst are ported with the miracles work; the falling creature is not |
| The camera follows the fall along its recorded path | todo | the path file (`Data/Spells/fall`) is read by the camera path reader (`components/`), not used for this |
| Sounds and a white fade are timed to the film | todo | see [bink_playback.md](bink_playback.md) |
| It only plays when the player has a creature | todo | see [bink_playback.md](bink_playback.md) |
| Afterwards the creature is put back on the land and the game speed and music are restored | todo | see ../story/ending.md |

## Options and keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape skips a film (fading it out) | todo | see [bink_playback.md](bink_playback.md) |
| The video option in the game's options | todo | (unconfirmed what it changes; see ../interface/options.md) |
| Films run while the world is paused; the land's ambience is hushed | partial | the ambience takes a 'video playing' flag (`src/Audio/AtmosAudio.cpp`); nothing sets it |
