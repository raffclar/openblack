# Losing and game over

Every way a god can lose: in the story, the land is lost only when the player's temple is destroyed, which plays the
game-over sequence; in a skirmish or network game a god without a temple is out, and the last one standing wins.
Destroying a temple is slow on purpose: harm aimed at it goes to its god's towns first.

**Progress: 0/58 done, 4 partial — 3%**

## Can a story land be lost?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only way to lose a story land is the destruction of the player's temple. No land, challenge or global script checks for defeat, so losing every town, every believer or the creature does not end the game by itself | todo | the game-over script is started by the game itself, never by another script; the one script that runs it is a test that does not ship (see Unused) |
| At the end of every game turn the game checks: a one-player story game (not a skirmish or a network game), the player has had a temple on this land, and its heart's destruction has begun. When all hold, the game-over script starts | todo | nothing in openblack watches for it |
| It starts at once, with no grace period or timer, and only once: a flag records that the game is over | todo | |
| The flag is saved with the game and cleared when a new land is loaded | todo | see ../engine/saving_and_loading.md |
| Only the local player's temple counts; a computer god's temple being destroyed never ends the game | todo | |
| It cannot happen on the Gods' Playground: the player has no temple there | todo | see [tutorial_island.md](tutorial_island.md) |
| The same script and check serve every land, from the first to the fifth: no land has its own losing condition, timer or text | todo | the script is in the global challenge list, loaded for every land |
| The player's creature is never lost for good: at no life it faints and is carried home | partial | fainting works (see ../creature/physiology.md); the advisors' line about it does not |
| While Lethys kidnaps the creature at the end of Land 2, it cannot be hurt, so it cannot faint and go home halfway; once it reaches the vortex it can be hurt again | partial | the switch works (developer functions 8 and 9 in `src/CHLApi.cpp`, `canDie`); the kidnapping script does not run (see [land_2.md](land_2.md)); the kidnapping: [gold_scrolls/lethys_has_taken_our_creature.md](gold_scrolls/lethys_has_taken_our_creature.md) |
| Failing a gold or silver scroll never ends the game. Each failure has its own ending, and the story goes on | todo | the script commands the scrolls need are stubs in `src/CHLApi.cpp`; see [challenges_and_rewards.md](challenges_and_rewards.md) |
| The first land's gate stones cannot be lost: one destroyed or taken out of the player's influence is made again where it started, and one dropped somewhere else is put back after a minute | todo | see [land_1.md](land_1.md) |

## How a temple takes damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles, fire and other effects aimed at any part of a temple are passed on to its heart | todo | |
| The heart passes the harm on to a building in one of its god's own towns: the first one found with more than a quarter of its life left, or else the first one found still standing with any life | todo | |
| With no such building left, the harm goes to a homeless villager of one of those towns | todo | |
| Only when its god has neither does the heart take the harm itself | todo | |
| Each time harm is passed on, a spark beam runs from the temple to whatever takes it, with one of five temple spark sounds in turn | todo | the sounds are named in Diego's notes; nothing in openblack plays them |
| An object thrown at the heart is passed on the same way; a hit on the heart itself does hit damage from the object's speed and weight, at most 0.2 a hit | partial | the heart takes physical hits while more than a tenth of it is built (`src/ECS/PhysicsClasses.cpp`); nothing passes the hit on or damages it |
| A heart that takes harm itself, below full life, now and then gives off sparks, bigger the more it is hurt, and crackles once its life is at 60% or less | todo | |
| The heart's info table has a multiplier for passed-on damage | todo | undetermined: where the game reads it was not traced |

## How a temple is destroyed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the heart's life runs out, its destruction begins: every other part of the temple is removed | todo | |
| A script can destroy a temple the same way ("delete ... with temple explode"); deleting a heart already being destroyed sets off an explosion at it | todo | the delete command is a stub in `src/CHLApi.cpp` |
| In the story, scripts blow up rival gods' temples: Khazar's when Nemesis kills him in Land 2, Lethys's as the player leaves Land 2 and again in Land 3 when his last town is won after the creed scene, and Nemesis's twice in the final fight | todo | see ../rival_gods/, [ending.md](ending.md); Khazar's: [gold_scrolls/nemesis_no.md](gold_scrolls/nemesis_no.md#the-film-khazars-temple); Lethys's on the third land: [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| In the story a destroyed player temple loses the land; in a skirmish or network game it puts that god out (see below) | todo | |

## The game-over sequence

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any text drawn on the screen fades out over a second, and the game goes into cinema mode | todo | the draw text commands are stubs in `src/CHLApi.cpp` |
| The failure music starts | partial | the failure bank is named in `src/Audio/GameMusic.cpp` and the start music command plays it; nothing starts the game-over script |
| The time of day is moved to 23:00 over 10 seconds, so night falls | todo | the clock command works (`MoveGameTime`), but nothing runs the script |
| The camera shakes a little at the temple, then starts 90 m from its entrance and 40 m up and zooms in over 12 seconds to 50 m away and 10 m up, looking at the temple | todo | setting the camera works, moving it and shaking it are stubs in `src/CHLApi.cpp` |
| A bigger shake, and Nemesis laughs: "Bwa ha ha ha ha!" (heard only) | todo | |
| A cut closer to the temple; the evil advisor appears and says: "Ah. Okay, we're dead. We're all dead." | todo | advisors and spoken lines are stubs in `src/CHLApi.cpp`; see [advisors.md](advisors.md) |
| Another cut; the good advisor appears, the ground shakes, and the camera cuts again, still shaking | todo | |
| Once the line is read, the good advisor says: "I just hate goodbyes." | todo | |
| The camera cuts higher and higher above the temple every half second while the land shakes hard for 16 seconds | todo | |
| The good advisor leaves, the temple explosion sounds, the screen starts fading to black over 12 seconds, and the evil advisor leaves | todo | |
| "Game Over" fades in over 3 seconds in white, large, in the middle of the screen | todo | |
| The explosion sounds again and the camera rises 1000 m into the sky over 12 seconds | todo | |
| Game time stops; after a pause the text fades out over 5 seconds | todo | |
| The player is taken into the temple's Save Game room and the music stops | todo | that command is not implemented in openblack's `DevFunction`; see ../temple/save_game_room.md |

## After the game over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no restart or retry button: the player is left in the Save Game room to load a save | todo | see ../temple/save_game_room.md |
| Once the game is over, the automatic save, the quick-save on exit and the low-memory save all stop, so the saves the player had are kept | todo | see ../engine/saving_and_loading.md |
| A game saved after the game over keeps the flag: loading it never plays the sequence again, and those saves stay off | todo | |
| What the player can do on the land after leaving the Save Game room | todo | undetermined: game time has been stopped and nothing found starts it again |

## Skirmish and network games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's game over never runs | todo | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Each turn, a god among players one to four with no temple is out of the game. Its creature is taken off the land (the player's own is copied first) | todo | players five to eight (lookers-on in a network game) are never out |
| When another god is out and more than one is left, "<name> is out of the game." is shown | todo | |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" with only Leave Game | todo | in a skirmish or a local network game; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game. The network game keeps running | todo | the text with "Click YES to watch the other players, NO to quit" is never shown (see Unused) |
| When that box opens, the good advisor sometimes says "How shall I put this, Leader? You're, erm, not very good at this game. Sorry." (a 2% chance, at most once a game) | todo | |
| Watch Game closes the box and play goes on, so the player can watch the other gods | todo | |
| Leave Game closes the box and ends the game: a skirmish goes back to the skirmish box, a network game is left | todo | |
| When the player is out and only one god is left, the game ends, the network game stops, and the end box says "You have lost the game." with only Leave Game | todo | in a skirmish or a local network game; in an internet game the box is set up differently (points and credits, see ../multiplayer/online_services.md) and its buttons there were not traced |
| A looker-on (players five to eight) sees the winner's name with "Won" | todo | |
| In a network game (patch 1.2), the game also ends when one god completes all the winning conditions or the time limit is reached; gods are ranked by their share of the conditions, ties broken by a second measure, and the winner gets the winning box while everyone else gets "You have lost the game." | todo | see [../multiplayer/multiplayer_rules.md](../multiplayer/multiplayer_rules.md); the time limit is skipped in a skirmish |
| A game never ends in a draw | todo | ties are broken; the draw text is unused |
| There is no surrender. Leaving from the menu opens the same box asking "Are you sure you want to quit this game?" (or, in some internet games, "If you leave the game now, you'll forfeit your credits!") with Back to Game and Leave Game | todo | see [../multiplayer/network_play.md](../multiplayer/network_play.md) |
| The end box has a "Game Over" tab and a "Statistics" tab | todo | see [../interface/statistics.md](../interface/statistics.md) |

## Computer gods losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story's rival gods are beaten by their land's scripts, which check their towns and blow up their temples; they do not lose by the temple rule | todo | see ../rival_gods/ and [ending.md](ending.md) |
| A story rival's temple can still be worn down once it has no towns, but no story script reacts to that | todo | undetermined in play |
| In a skirmish, computer gods are out by the same rule as the player, stop thinking and lose their creature | todo | see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |

## Unused

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A test script that just runs the game-over script | n/a | in the script sources but not in the shipped challenge file |
| The game-over script's own temple explosion, two more of Nemesis's laughs and a closing window are commented out | n/a | the temple is already being destroyed when the script starts |
| "You have lost the game. Click YES to watch the other players, NO to quit the Multiplayer Game." (twice) and the same for "the Skirmish Game" | n/a | nothing in the 1.2 program shows them; the end box uses "You have lost the game." with Watch Game and Leave Game buttons |
| "You have lost the game. The winner was %s.", "Game winner: %s" and "All the players lost. It's a draw." | n/a | nothing in the 1.2 program shows them |
| There is no game-over video | n/a | the game ships only the intro, logo, tips and falling spell videos |
