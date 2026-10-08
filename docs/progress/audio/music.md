# Music

The game's music: each tribe's music in good, neutral and evil versions that follow the player's alignment over the
land, the worship chants, the temple's music, fight music, and the pieces the story's scripts start.

**Progress: 19/27 done, 0 partial — 70%**

## Choosing what plays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a game turn the music is picked: the temple's music inside the temple, else a script's music, else the land's | done | `src/Audio/GameMusic.cpp`; test `test_music` |
| Over land, the music of the nearest town's tribe within 300 of the camera, or the last heard while still within 400 | done | `src/Audio/GameMusic.cpp`; test `test_music` |
| Away from towns, the generic music of the player's alignment | done | `src/Audio/GameMusic.cpp` |
| Too high above the land no town is heard | done | `src/Audio/GameMusic.cpp` |
| The alignment is taken in seven steps and grouped into evil, neutral and good | done | `src/Audio/GameMusic.cpp` |
| Changing between versions of the same music carries on in time | done | music groups (`src/Audio/MusicPlayer.cpp`); test `test_music` |
| A piece that played to its end isn't picked again for a while | done | `src/Audio/GameMusic.cpp` |
| Each music group remembers where it got to and picks up from there | done | `src/Audio/GameMusic.cpp` |
| Scripts turn the alignment music off and on | done | `src/CHLApi.cpp` |
| The music forgets what played when a new land loads | done | `src/Audio/GameMusic.cpp` |
| A music volume setting | done | `src/Gui/GameMenu.cpp`, `AudioManager::SetMusicVolume` |
| The music fades out when stopped | done | `AudioManager::MusicStop` |

## Tribes' music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Celtic, Aztec, Japanese, Indian, Egyptian, Greek and Tibetan town music, each good, neutral and evil | done | `src/Audio/GameMusic.cpp` |
| Generic good, neutral and evil music | done | `src/Audio/GameMusic.cpp` |
| Norse towns have no music of their own and play the Celtic music | done | `src/Audio/GameMusic.cpp` |

## Situations

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's music, of the player's alignment, inside the temple | done | `src/Audio/GameMusic.cpp`; `Game.cpp` passes the temple's turn as inside |
| Worship chants: each tribe's chant and its sung version while its people worship | todo | the banks are listed in `src/Audio/GameMusic.cpp` but nothing plays them |
| Creature fight music, and the big fight's own music | todo | listed, not played; see ../creature/fighting.md |
| Music while the creature dances | todo |  |
| The creature's mood shapes the music | todo | (unconfirmed how far the game uses it) |
| Music attached to an object, heard by distance (the pied piper's tune) | todo | the attach/move/detach music commands are stubs |
| The intro and outro music | todo | see ../story/ending.md; the outro plays over the credits: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |
| Music from the CD's audio tracks | n/a | the game's own banks are used |

## Script music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts start and stop a piece of music | done | `src/CHLApi.cpp`, `GameMusic::StartScriptMusic` |
| A script piece plays from its start until stopped | done | `src/Audio/GameMusic.cpp` |
| The story's pieces: the piper's tunes, the hermit, the missionaries, the intro, the singing stones, the welcome dance, creature chosen, funeral, creature guide, Khazar, Nemesis, twinkle, the whistles, Sleg the ogre, guardian stone, failure, gregorian, Christmas, circus, the epics and the creature's end sequence | done | all named in `src/Audio/GameMusic.cpp` |
| Scripts ask whether a piece has played, how far from it they are, and set where it plays from | todo | stubs in `src/CHLApi.cpp` |
| The missionaries' sing-along with a bouncing ball over the words | todo | see voices_and_speech.md; the quest: [the_explorers.md](../story/silver_scrolls/the_explorers.md) |
