# Challenge scripts

The story, the tutorial, the advisors' help and the rewards are written as scripts in the game's challenge language and shipped compiled in `Scripts/Quests/challenge.chl`. This file covers those scripts as code: how many there are, where they come from, how they start land by land, and how many of them openblack can run without reaching a function it doesn't have yet. What each challenge is about is the story domain's ([../story/](../story/)); how the virtual machine runs scripts is [../engine/script_vm.md](../engine/script_vm.md); each function the scripts call is in the `challenge_natives_*.md` files of this folder.

In all, 514 scripts: 27 call only functions openblack has, and 17 of those also start only scripts that do; the rest stop at a function that logs "not implemented" (most often the dialogue box, its text, the advisors, cut-scene control and object properties). "Runs" below means the script and every script it starts call nothing missing; whether what they do then matches the game is the natives' files' business.

The function counts below were taken before a fix to how the commands each script needs are counted, so some are a little
low (for example, The Missionaries and the helper scripts it runs need 81 commands, of which 23 work); the
per-quest files under [../story/silver_scrolls/](../story/silver_scrolls/) have the corrected counts. Every silver
scroll is listed in [../story/silver_scrolls.md](../story/silver_scrolls.md).

These challenge scripts are in the sources but not compiled into the game, so they have no row here: The Big Whale,
Landslide, The Sculptor's early silver version, Food for Thought, The Attackers, The Miracle Stones, See The Citadel,
Chimp Posse, the Creature Ladder and the swaps to horse, leopard, lion, tortoise and wolf (see the "Cut or never
started" table of [../story/silver_scrolls.md](../story/silver_scrolls.md#cut-or-never-started)).

**Progress: 7/144 done, 22 partial — 12%**

## The program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `challenge.chl` holds 514 scripts compiled from 98 source files, with 398 global variables and 157,160 instructions | done | loaded whole by `LHVM::LoadBinary` (see ../engine/script_vm.md) |
| Scripts are of several kinds: 463 ordinary, 13 help, 15 challenge help, 17 temple help, 5 multiplayer help and 1 temple special; the game runs only some kinds in some situations (which, when, unconfirmed) | partial | openblack runs every kind always (see ../engine/script_vm.md) |
| One script starts by itself when the program loads: the creature's development (its training lessons) | done | the auto-start list in `LHVM::LoadBinary`; it then waits on functions openblack lacks |
| The program's source compiles back to exactly the shipped file | done | `components/lhvmcompiler`; test `ChlRoundTrip.OriginalSourcesCompileToTheGamesProgram` |
| The scripts' names for objects, sounds, music, help texts and constants come from the game's headers and info tables | done | `components/lhvmlang` (`ChlConstants`); test `ChlLanguage.ConstantsFromHeaders` |

## Starting the story land by land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game starts the story's control script, which runs Land 1's control script and waits for it to finish | partial | `Game::Run` starts it by name after the land loads; it starts even when another land or a playground was loaded |
| When a land's control script ends, every script except the story's control script is stopped | partial | `StopAllScriptsInFilesExcluding` works by source file; reached only if Land 1 ends, which it can't yet |
| The story's control script then loads the next land's script and runs that land's control script (Land 2, 3, 4 and 5 in turn) | todo | loading a land from a script does nothing (`LoadMap` in `CHLApi.cpp`) |
| Arriving on Land 3 fades the screen in over three seconds | done | `SetFadeIn` (`ScriptFade` tests); reached only once lands can load |
| Each land's control script runs its set-up script (computer players, creatures, influence, starting rewards), then starts the land's challenges and their reminders | todo | every land's set-up script stops at a missing function; Land 3 and 4's control scripts themselves call nothing but stop at the scripts they start |
| The tutorial land starts the tutorial's own control script, not the story's | todo | openblack always starts the story's control script |
| The rival gods' creatures are loaded by a shared set-up script (each from its mind file, see creature_mind_scripts.md) | todo | loading a creature from a script logs "not implemented" |
| A hidden script lets the player ring the phone boxes on Lands 1 to 4 for recorded messages (its land-skipping cheat is commented out in the shipped source and only says the cheat was removed) | todo | it stops at the sound functions |

## Shared by every land (help, rewards, the creature's training, the story's control)

15 source files, 97 scripts; 2 call only functions openblack has and 1 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Standard Reminder: 1 script, 0 run | todo | 6 of the 6 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / shows a line of text / whether the text on screen has been read / takes the dialogue box for the script |
| Vortex Entry: 1 script, 0 run | todo | 4 of the 5 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / adds an animal or villager to a flock, optionally as its leader / throws an object off along a heading at a speed |
| Swap Creatures: 2 scripts, 0 run | todo | 35 of the 43 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen / shows a line of text / whether the text on screen has been read; quest: [Creature swaps](../story/silver_scrolls/creature_swaps.md) |
| Challenge Notify: 6 scripts, 0 run | todo | 13 of the 19 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / makes an advisor point at a place, on screen or in the world / shows a line of text / whether the text on screen has been read |
| Did You Know: 2 scripts, 0 run | todo | 10 of the 11 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / shows a line of text / whether the text on screen has been read / takes the dialogue box for the script |
| Help System: 9 scripts, 0 run | todo | 11 of the 11 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box |
| Reward: 6 scripts, 0 run | todo | 32 of the 38 functions it calls are missing; first to do: takes the dialogue box for the script / lets go of the dialogue box / shows a line of text / whether the text on screen has been read |
| Citadel Help: 17 scripts, 0 run | todo | 10 of the 11 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen / shows a line of text / whether the text on screen has been read |
| Creature Help: 7 scripts, 0 run | todo | 12 of the 13 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box |
| Creature Development: 15 scripts, 1 runs | partial | 64 of the 88 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box; quest: [The Creature's Learning](../story/gold_scrolls/the_creatures_learning.md) |
| Hand Demos: 18 scripts, 0 run | todo | 31 of the 38 functions it calls are missing; first to do: whether the text on screen has been read / shows a line of text / whether the camera has finished a glide / glides the camera's eye to a position over a number of seconds |
| Setup Computer Creatures: 4 scripts, 0 run | todo | 11 of the 13 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / teaches a creature / hands an object back to the game after a script has controlled it |
| Creature Breeder: 3 scripts, 0 run | todo | 27 of the 33 functions it calls are missing; first to do: gives a player's creature / glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen; quest: [The Creature Breeder](../story/silver_scrolls/the_creature_breeder.md) |
| Game Over: 1 script, 0 run | todo | 23 of the 34 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / shows a line of text |
| Land Control All: 5 scripts, 0 run | todo | 41 of the 52 functions it calls are missing; first to do: plays a sound effect from one of the sound banks, at a position or on an object / whether a sound effect is playing / reads one of an object's properties / changes one of an object's properties |

## The tutorial

1 source files, 19 scripts; 2 call only functions openblack has and 1 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land Control T: 19 scripts, 1 runs | partial | 43 of the 59 functions it calls are missing; first to do: gives the camera back to the player at the end of a cut scene / lets go of the dialogue box / puts the game back to its normal speed after a cut scene / creates a challenge marker |

## Land 1

20 source files, 144 scripts; 8 call only functions openblack has and 5 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setup Land 1: 3 scripts, 0 run | partial | 12 of the 14 functions it calls are missing; first to do: finds an object of a kind at a position / finds an object of a kind within a radius of a position / puts a ring of influence around an object for a player / puts a ring of influence at a position for a player |
| Creature Guide: 15 scripts, 1 runs | partial | 81 of the 98 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number |
| Citadel Guide: 2 scripts, 0 run | partial | 29 of the 39 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Temple Is Finished](../story/gold_scrolls/the_temple_is_finished.md) |
| Protect Gate Keys: 5 scripts, 0 run | todo | 12 of the 17 functions it calls are missing; first to do: reads one of an object's properties / deletes an object, plainly, fading it away, exploding it or with the temple's explosion / starts a special effect on an object for a time / puts a ring of influence at a position for a player; quest: [Choose Your Creature](../story/gold_scrolls/choose_your_creature.md#the-stones-are-guarded) (the gate-stone guards) |
| Lost Brother: 14 scripts, 0 run | todo | 60 of the 79 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box; quest: [The Lost Brother](../story/gold_scrolls/the_lost_brother.md) |
| The Sculptor: 9 scripts, 1 runs | partial | 43 of the 59 functions it calls are missing; first to do: reads one of an object's properties / creates a timer running for a number of seconds / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number; quest: [The Sculptor](../story/gold_scrolls/the_sculptor.md) |
| Choose Your Creature: 5 scripts, 0 run | todo | 55 of the 74 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / reads one of an object's properties / takes the dialogue box for the script; quest: [Choose Your Creature](../story/gold_scrolls/choose_your_creature.md) |
| Creature Guardian: 9 scripts, 1 runs | partial | 50 of the 61 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / makes a creature do an action / takes the dialogue box for the script; quest: [The Ogre](../story/silver_scrolls/the_ogre.md) |
| Creature Saving People: 4 scripts, 1 runs | partial | 42 of the 51 functions it calls are missing; first to do: puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number / turns an object to face a position / brings the good or evil advisor out onto the screen; quest: [The Saviour](../story/silver_scrolls/the_saviour.md) |
| Follow Us: 7 scripts, 0 run | todo | 74 of the 94 functions it calls are missing; first to do: lets go of the dialogue box / gives the camera back to the player at the end of a cut scene / puts the game back to its normal speed after a cut scene / brings the good or evil advisor out onto the screen |
| Leave Through Vortex L1: 1 script, 0 run | todo | 32 of the 39 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [Leave Through the Vortex (Land 1)](../story/gold_scrolls/leave_through_the_vortex_land_1.md) |
| Pied Piper: 18 scripts, 0 run | todo | 63 of the 81 functions it calls are missing; first to do: lets go of the dialogue box / gives the camera back to the player at the end of a cut scene / puts the game back to its normal speed after a cut scene / reads one of an object's properties; quest: [The Pied Piper](../story/silver_scrolls/the_pied_piper.md) |
| Singing Stone Circle: 15 scripts, 1 runs | partial | 53 of the 67 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box; quest: [The Singing Stones](../story/silver_scrolls/the_singing_stones.md) |
| Take Over Villages L1: 1 script, 0 run | todo | 11 of the 15 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen / shows a line of text / whether the text on screen has been read |
| The Hermit: 6 scripts, 0 run | todo | 52 of the 63 functions it calls are missing; first to do: reads one of an object's properties / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number / makes a living thing walk to a position, ending within a radius of it; quest: [The Hermit](../story/silver_scrolls/the_hermit.md) |
| The Lost Flock: 6 scripts, 0 run | todo | 50 of the 63 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box; quest: [The Lost Flock](../story/silver_scrolls/the_lost_flock.md) |
| The Missionaries: 15 scripts, 0 run | todo | 57 of the 75 functions it calls are missing; first to do: lets go of the dialogue box / shows a line of text / takes the dialogue box for the script / whether the text on screen has been read; quest: [The Explorers](../story/silver_scrolls/the_explorers.md) |
| Throwing Stones: 7 scripts, 0 run | todo | 46 of the 57 functions it calls are missing; first to do: lets go of the dialogue box / reads one of an object's properties / gives the camera back to the player at the end of a cut scene / puts the game back to its normal speed after a cut scene; quest: [Throwing Stones](../story/silver_scrolls/throwing_stones.md) |
| Magic Mushroom: 1 script, 0 run | todo | 35 of the 48 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / makes an advisor point at a place, on screen or in the world; quest: [The Immersion Mushrooms](../story/silver_scrolls/the_immersion_mushrooms.md) |
| Land Control 1: 1 script, 0 run | todo | 14 of the 18 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / finds an object of a kind at a position / deletes an object, plainly, fading it away, exploding it or with the temple's explosion |

## Land 2

26 source files, 128 scripts; 7 call only functions openblack has and 7 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setup Land 2: 2 scripts, 0 run | todo | 13 of the 18 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / makes an advisor point at a place, on screen or in the world / whether an object is in the camera's view / shows a line of text |
| Slavers Warning: 1 script, 0 run | todo | 15 of the 21 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; compiled but never started by the game (no script runs it): see the end of [The Slavers](../story/silver_scrolls/the_slavers.md) |
| Sacrifice: 2 scripts, 0 run | todo | 34 of the 49 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; quest: [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) |
| Baywatch: 3 scripts, 0 run | todo | 32 of the 49 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / shows a line of text / whether the text on screen has been read / puts a villager, animal or other living thing into one of its script states; quest: [The Sea](../story/silver_scrolls/the_sea.md) |
| Land 2Shield Challenge: 1 script, 0 run | todo | 34 of the 47 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md) |
| Land 2Fireball Challenge: 2 scripts, 0 run | todo | 34 of the 46 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; quest: [Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md) (with the "Miracle Challenge" scroll) |
| Land 2Computer AI: 7 scripts, 0 run | todo | 24 of the 30 functions it calls are missing; first to do: creates a timer running for a number of seconds / shows a line of text / whether the text on screen has been read / gives the camera back to the player at the end of a cut scene |
| Begin Land 2: 14 scripts, 0 run | todo | 54 of the 69 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / moves a rival god's hand to a position at a speed / glides the camera's eye to a position over a number of seconds |
| Learn Worshipping: 4 scripts, 0 run | todo | 54 of the 64 functions it calls are missing; first to do: reads one of an object's properties / shows a line of text / whether the text on screen has been read / gives the camera back to the player at the end of a cut scene; quest: [Worship Site](../story/gold_scrolls/worship_site.md) |
| Greedy Farmer: 10 scripts, 0 run | todo | 44 of the 57 functions it calls are missing; first to do: lets go of the dialogue box / gives the camera back to the player at the end of a cut scene / puts the game back to its normal speed after a cut scene / shows a line of text; quest: [The Greedy Farmer](../story/silver_scrolls/the_greedy_farmer.md) |
| Idol Pyre: 1 script, 0 run | todo | 43 of the 59 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Idol](../story/silver_scrolls/the_idol.md) |
| Kill Khazar: 2 scripts, 0 run | todo | 41 of the 61 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds; quest: [Nemesis. No!](../story/gold_scrolls/nemesis_no.md) |
| Leave Through Vortex L2: 1 script, 0 run | todo | 21 of the 27 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [Leave through the vortex (Land 2)](../story/gold_scrolls/leave_through_the_vortex_land_2.md) |
| Lethys Vortex: 3 scripts, 0 run | todo | 40 of the 58 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / reads one of an object's properties / changes one of an object's properties; quest: [Lethys has taken our Creature!](../story/gold_scrolls/lethys_has_taken_our_creature.md) |
| Lost Treasure: 4 scripts, 0 run | todo | 30 of the 45 functions it calls are missing; first to do: deletes an object, plainly, fading it away, exploding it or with the temple's explosion / gives the camera back to the player at the end of a cut scene / lets go of the dialogue box / puts the game back to its normal speed after a cut scene; quest: [The Riddles](../story/silver_scrolls/the_riddles.md) |
| Singing Stones Songs: 16 scripts, 5 run | partial | 56 of the 72 functions it calls are missing; first to do: lets go of the dialogue box / takes the dialogue box for the script / gives the camera back to the player at the end of a cut scene / puts the game back to its normal speed after a cut scene; quest: [The Singing Stones (land 2)](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Plague: 1 script, 0 run | todo | 49 of the 58 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Plague](../story/silver_scrolls/the_plague.md) |
| Spiritual Healer: 7 scripts, 0 run | todo | 34 of the 48 functions it calls are missing; first to do: lets go of the dialogue box / reads one of an object's properties / plays a spoken sound effect / takes the dialogue box for the script; quest: [The Spiritual Healer](../story/silver_scrolls/the_spiritual_healer.md) |
| The Slavers: 23 scripts, 1 runs | partial | 58 of the 76 functions it calls are missing; first to do: puts a villager, animal or other living thing into one of its script states / reads one of an object's properties / lets go of the dialogue box / puts a living thing into a script state that takes a whole number; quest: [The Slavers](../story/silver_scrolls/the_slavers.md) |
| The Workshop: 2 scripts, 0 run | todo | 42 of the 52 functions it calls are missing; first to do: reads one of an object's properties / finds an object of a kind within a radius of a position / glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds; quest: [The Workshop](../story/gold_scrolls/the_workshop.md) |
| Hanoi Flood: 3 scripts, 1 runs | partial | 26 of the 36 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; quest: [The Beach Temple Puzzle](../story/silver_scrolls/the_beach_temple_puzzle.md) |
| Tree Puzzle One: 1 script, 0 run | todo | 3 of the 6 functions it calls are missing; first to do: deletes an object, plainly, fading it away, exploding it or with the temple's explosion / finds an object of a kind within a radius of a position / whether a living thing has finished the animation or state the script gave it |
| Learn Gestures: 2 scripts, 0 run | todo | 41 of the 53 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; quest: [Impress Village](../story/gold_scrolls/impress_village.md) |
| Learn Influence: 1 script, 0 run | todo | 22 of the 31 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / shows a line of text / whether the text on screen has been read; quest: [Impress Village](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence) (Khazar's lesson on influence) |
| Final Gold Scroll: 4 scripts, 0 run | todo | 25 of the 37 functions it calls are missing; first to do: gives the camera back to the player at the end of a cut scene / lets go of the dialogue box / puts the game back to its normal speed after a cut scene / brings the good or evil advisor out onto the screen; quest: [Destroy it!](../story/gold_scrolls/destroy_it.md) |
| Land Control 2: 11 scripts, 0 run | todo | 7 of the 10 functions it calls are missing; first to do: reads one of an object's properties / finds an object of a kind within a radius of a position / sets a timer's time / creates a timer running for a number of seconds |

## Land 3

9 source files, 28 scripts; 2 call only functions openblack has and 1 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setup Land 3: 2 scripts, 1 runs | partial | 7 of the 11 functions it calls are missing; first to do: finds an object of a kind at a position / gives a player's creature / loads the player's own creature into the land at a position / gives a player influence everywhere |
| Shaolin: 5 scripts, 0 run | todo | 35 of the 50 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number; quest: [The Shaolin](../story/silver_scrolls/the_shaolin.md) |
| Leave Through Vortex L3: 1 script, 0 run | todo | 12 of the 17 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / reads one of an object's properties / changes one of an object's properties; quest: [Leave Through the Vortex (Land 3)](../story/gold_scrolls/leave_through_the_vortex_land_3.md) |
| Free The Creature: 8 scripts, 0 run | todo | 59 of the 83 functions it calls are missing; first to do: reads one of an object's properties / hands an object back to the game after a script has controlled it / turns an object to face a position / makes a living thing walk to a position, ending within a radius of it; quest: [So You Couldn't Bear to Be Without Your Creature?](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md), [The Wolves Are Possessed](../story/gold_scrolls/the_wolves_are_possessed.md), [Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md) |
| Get Through Vortex L2: 4 scripts, 0 run | todo | 32 of the 43 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / gives a player's creature / loads the player's own creature into the land at a position; quest: [So You Couldn't Bear to Be Without Your Creature?](../story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md) |
| Swap To Ape: 2 scripts, 0 run | todo | 42 of the 53 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a position; quest: [The Rejuvenator](../story/silver_scrolls/the_rejuvenator.md) |
| Tree Puzzle Two: 1 script, 0 run | todo | 14 of the 19 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / reads one of an object's properties / takes the camera from the player at the start of a cut scene |
| Throw Bloke: 4 scripts, 0 run | todo | 32 of the 41 functions it calls are missing; first to do: puts a villager, animal or other living thing into one of its script states / reads one of an object's properties / whether a creature is holding an object in its hand / makes an object impossible to destroy, or not |
| Land Control 3: 1 script, 0 run | partial | calls no functions itself |

## Land 4

11 source files, 47 scripts; 2 call only functions openblack has and 1 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setup Land 4: 1 script, 0 run | todo | 4 of the 5 functions it calls are missing; first to do: finds an object of a kind at a position / finds an object of a kind within a radius of a position / opens or closes an object / boosts or lowers one of a town's desires |
| Land 4Meteorites: 25 scripts, 1 runs | partial | 70 of the 93 functions it calls are missing; first to do: reads one of an object's properties / shows a line of text / whether the text on screen has been read / takes the dialogue box for the script; quest: [The Defending Ogres](../story/gold_scrolls/the_defending_ogres.md), [The Totem Puzzle](../story/gold_scrolls/the_totem_puzzle.md), [The Heartbroken Man](../story/gold_scrolls/the_heartbroken_man.md), [Undead Village](../story/gold_scrolls/undead_village.md) |
| Land 4Ogre: 7 scripts, 0 run | todo | 52 of the 72 functions it calls are missing; first to do: lets go of the dialogue box / makes a living thing walk to a position, ending within a radius of it / takes the dialogue box for the script / hands an object back to the game after a script has controlled it; quest: [The Defending Ogres](../story/gold_scrolls/the_defending_ogres.md) |
| Begin Land 4: 2 scripts, 0 run | todo | 25 of the 34 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / makes an advisor point at a place, on screen or in the world |
| Leave Through Vortex L4: 1 script, 0 run | todo | 22 of the 32 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [Leave Through the Vortex (Land 4)](../story/gold_scrolls/leave_through_the_vortex_land_4.md) |
| Fish Puzzle: 1 script, 0 run | todo | 31 of the 41 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Fish Puzzle](../story/silver_scrolls/the_fish_puzzle.md) |
| Blind Woman: 5 scripts, 0 run | todo | 51 of the 69 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / deletes an object, plainly, fading it away, exploding it or with the temple's explosion / shows a line of text; quest: [The Treacherous Path](../story/silver_scrolls/the_treacherous_path.md) |
| Swap To Cow: 2 scripts, 0 run | todo | 33 of the 44 functions it calls are missing; first to do: puts a villager, animal or other living thing into one of its script states / reads one of an object's properties / changes one of an object's properties / makes a living thing walk to a position, ending within a radius of it; compiled but never started by the game: [Swap To Cow](../story/silver_scrolls/swap_to_cow.md) |
| Take Over Villages L4: 1 script, 0 run | todo | 2 of the 2 functions it calls are missing; first to do: reads one of an object's properties / finds an object of a kind within a radius of a position |
| Thesius Puzzle Land 4: 1 script, 0 run | todo | 4 of the 7 functions it calls are missing; first to do: deletes an object, plainly, fading it away, exploding it or with the temple's explosion / finds an object of a kind within a radius of a position / puts a ring of influence at a position for a player / whether a living thing has finished the animation or state the script gave it |
| Land Control 4: 1 script, 0 run | partial | calls no functions itself |

## Land 5

16 source files, 51 scripts; 4 call only functions openblack has and 1 run with everything they start.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Begin Land 5: 1 script, 0 run | todo | 35 of the 48 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [I have a surprise for you.](../story/gold_scrolls/i_have_a_surprise_for_you.md) |
| Creature Mirror Fight: 1 script, 0 run | todo | 30 of the 35 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / shows a line of text |
| Fire On High: 3 scripts, 0 run | todo | 34 of the 46 functions it calls are missing; first to do: plays a sound effect from one of the sound banks, at a position or on an object / reads one of an object's properties / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Heavenly Fire](../story/silver_scrolls/the_heavenly_fire.md) |
| Lion Puzzle: 2 scripts, 0 run | todo | 34 of the 41 functions it calls are missing; first to do: puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number / whether a living thing has finished the animation or state the script gave it / glides the camera's eye to a position over a number of seconds; quest: [Stanley The Wolf](../story/silver_scrolls/stanley_the_wolf.md) |
| Missionaries Returned: 1 script, 0 run | todo | 30 of the 39 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / brings the good or evil advisor out onto the screen / sends the good or evil advisor back off the screen; quest: [The Explorers Again](../story/silver_scrolls/the_explorers_again.md) |
| Swap To Brown Bear: 3 scripts, 0 run | todo | 33 of the 41 functions it calls are missing; first to do: brings the good or evil advisor out onto the screen / shows a line of text / whether the text on screen has been read / takes the dialogue box for the script; quest: [Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md) |
| The Big Fight: 8 scripts, 0 run | partial | 43 of the 59 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / starts a special effect / makes a creature do an action; quest: [So this is a fight to the death](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md) |
| Throw Through Shield: 3 scripts, 0 run | todo | 33 of the 45 functions it calls are missing; first to do: reads one of an object's properties / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number / deletes an object, plainly, fading it away, exploding it or with the temple's explosion; quest: [Nemesis's Shielded Village](../story/gold_scrolls/nemesis_shielded_village.md) |
| Volcano: 1 script, 1 runs | done | 0 of the 2 functions it calls are missing |
| Final Creature Sequence: 1 script, 0 run | todo | 22 of the 32 functions it calls are missing; first to do: glides the camera's eye to a position over a number of seconds / glides the camera's point of view to a position over a number of seconds / puts a villager, animal or other living thing into one of its script states / puts a living thing into a script state that takes a whole number; quest: [So this is a fight to the death](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| Japanese Traitor: 2 scripts, 0 run | todo | 29 of the 42 functions it calls are missing; first to do: shows a line of text / whether the text on screen has been read / takes the dialogue box for the script / lets go of the dialogue box; quest: [The Japanese Traitor](../story/silver_scrolls/the_japanese_traitor.md) |
| Setup Land 5: 1 script, 0 run | todo | 6 of the 8 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / gives a player influence everywhere / turns an object |
| Creature Curse: 6 scripts, 0 run | todo | 28 of the 39 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / brings the good or evil advisor out onto the screen / makes an advisor point at a place, on screen or in the world; quest: [I have a surprise for you.](../story/gold_scrolls/i_have_a_surprise_for_you.md) |
| Control Nemesis Battle Strategy: 2 scripts, 0 run | todo | 12 of the 16 functions it calls are missing; first to do: reads one of an object's properties / sets a rival god's personality / gives the camera back to the player at the end of a cut scene / takes the dialogue box for the script |
| Crusaders: 6 scripts, 0 run | todo | 41 of the 55 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / makes a living thing walk to a position, ending within a radius of it / turns an object to face a position; quest: [The Magic Dragon](../story/silver_scrolls/the_magic_dragon.md) |
| Land Control 5: 10 scripts, 0 run | partial | 59 of the 84 functions it calls are missing; first to do: reads one of an object's properties / changes one of an object's properties / starts a special effect / gives a player's creature; quest: [I have a surprise for you.](../story/gold_scrolls/i_have_a_surprise_for_you.md), [Nemesis's Shielded Village](../story/gold_scrolls/nemesis_shielded_village.md), [So this is a fight to the death](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md) |

## The missing functions that block the most scripts

Each row is one function the challenge scripts call that openblack doesn't have, with how many scripts call it directly. Doing the first ten would unblock the dialogue, cut scenes and advisors that nearly every challenge uses.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lets go of the dialogue box (end of the dialogue block) — called directly by 296 scripts | todo | `EndDialogue` in `CHLApi.cpp` |
| Whether the text on screen has been read (its speech finished or clicked past) — called directly by 267 scripts | todo | `TextRead` in `CHLApi.cpp` |
| Takes the dialogue box for the script (start of the language's dialogue block); waits while another script has it — called directly by 265 scripts | todo | `StartDialogue` in `CHLApi.cpp` |
| Shows a line of text (and plays its speech) in the dialogue box, optionally waiting for the player to click on — called directly by 264 scripts | todo | `RunText` in `CHLApi.cpp` |
| Reads one of an object's properties (health, age, food, wood, altitude, belief, scale, speed and dozens more) — called directly by 257 scripts | todo | `GetProperty` in `CHLApi.cpp` |
| Gives the camera back to the player at the end of a cut scene — called directly by 238 scripts | todo | `EndCameraControl` in `CHLApi.cpp` |
| Puts the game back to its normal speed after a cut scene — called directly by 238 scripts | todo | `EndGameSpeed` in `CHLApi.cpp` |
| Takes the camera from the player at the start of a cut scene (part of the language's cinema and camera blocks) — called directly by 181 scripts | todo | `StartCameraControl` in `CHLApi.cpp` |
| Puts the game at cinema speed for a cut scene (part of the cinema block) — called directly by 181 scripts | todo | `StartGameSpeed` in `CHLApi.cpp` |
| Changes one of an object's properties — called directly by 175 scripts | todo | `SetProperty` in `CHLApi.cpp` |
| Brings the good or evil advisor out onto the screen — called directly by 167 scripts | todo | `SpiritEject` in `CHLApi.cpp` |
| Glides the camera's eye to a position over a number of seconds — called directly by 162 scripts | todo | `MoveCameraPosition` in `CHLApi.cpp` |
| Glides the camera's point of view to a position over a number of seconds — called directly by 161 scripts | todo | `MoveCameraFocus` in `CHLApi.cpp` |
| Makes a living thing walk to a position, ending within a radius of it — called directly by 156 scripts | todo | `MoveGameThing` in `CHLApi.cpp` |
| Whether the camera has finished a glide — called directly by 153 scripts | todo | `HasCameraArrived` in `CHLApi.cpp` |
| Puts a villager, animal or other living thing into one of its script states (walking to a place, dancing, sitting and so on) — called directly by 150 scripts | todo | `SetScriptState` in `CHLApi.cpp` |
| Puts a living thing into a script state that takes a whole number — called directly by 140 scripts | todo | `SetScriptUlong` in `CHLApi.cpp` |
| Turns an object to face a position — called directly by 138 scripts | todo | `SetFocus` in `CHLApi.cpp` |
| Deletes an object, plainly, fading it away, exploding it or with the temple's explosion — called directly by 121 scripts | todo | `ObjectDelete` in `CHLApi.cpp` |
| Sends the good or evil advisor back off the screen — called directly by 101 scripts | todo | `SpiritHome` in `CHLApi.cpp` |
| Whether a living thing has finished the animation or state the script gave it — called directly by 97 scripts | todo | `Played` in `CHLApi.cpp` |
| Hands an object back to the game after a script has controlled it (it goes back to its own life) — called directly by 96 scripts | todo | `ReleaseFromScript` in `CHLApi.cpp` |
| Creates a timer running for a number of seconds — called directly by 84 scripts | todo | `CreateTimer` in `CHLApi.cpp` |
| Gives the time a timer has left — called directly by 79 scripts | todo | `GetTimerTimeRemaining` in `CHLApi.cpp` |
| Creates a challenge marker (the bronze, silver or gold scroll and its signpost) at a position — called directly by 78 scripts | todo | `CreateHighlight` in `CHLApi.cpp` |
| Takes a picture of the camera's view for the challenge log in the temple — called directly by 76 scripts | todo | `Snapshot` in `CHLApi.cpp` |
| Finds an object of a kind within a radius of a position — called directly by 70 scripts | todo | `CallNear` in `CHLApi.cpp` |
| Closes the dialogue box — called directly by 70 scripts | todo | `GameCloseDialogue` in `CHLApi.cpp` |
| Sets a timer's time — called directly by 68 scripts | todo | `SetTimerTime` in `CHLApi.cpp` |
| Draws an object in full detail however far away it is — called directly by 66 scripts | todo | `SetHighGraphicsDetail` in `CHLApi.cpp` |

## Globals and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The 398 global variables (challenge progress flags such as whether a wonder was built or the ark sailed, chosen creature, counters) are shared by every script | done | held by the VM (see ../engine/script_vm.md) |
| The scripts' state, globals and running tasks are written into a saved game and read back, so a challenge carries on where it was | partial | the VM can write and read its state; openblack has no saved games (see ../engine/saving_and_loading.md) |
| The temple's challenge log keeps a picture of each challenge taken by the scripts | todo | the picture functions log "not implemented" |
