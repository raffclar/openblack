# Land 1

The first land: the player's own island, where the people build the temple, the advisors teach the hand and the camera,
the player chooses and trains a creature, and the land ends with the player leaving through the vortex for Khazar's land.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/33 done, 1 partial — 2%**

## Opening and setup

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's map loads and its story begins with the land's control script | partial | `Game.cpp` loads `challenge.chl` and starts the story's top script, which runs the land's control script; it stops at the first unwritten native (about 62 of 464 do something, `src/CHLApi.cpp`); the land script's contents: see ../scripts/land1_script.md |
| Setup places the first did-you-know scrolls, sets the starting belief of the villages and the weather | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The family leads the player to their village ("follow us"); the mother waits if the hand falls behind and says so if it runs ahead | todo | see tutorial.md |
| The villagers finish building the temple and the advisors show the player its entrance and how to go in | todo | see [gold_scrolls/the_temple_is_finished.md](gold_scrolls/the_temple_is_finished.md) (the scene that shows the first gold scroll); see ../temple/temple_exterior.md; a cut early draft of this scene: [silver_scrolls/see_the_citadel.md](silver_scrolls/see_the_citadel.md) |
| Camera zones keep the camera inside the parts of the land opened so far, widened as gates open | todo | the camera zone command is a stub; see ../camera/camera_limits.md |
| The hidden phone box of the first land, with its jokey recorded messages | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| A new game can skip the opening (straight to choosing a creature), skip all of the first land's story, or keep the old creature (patch 1.1) | todo | the skip and keep-creature commands are stubs; openblack keeps no profiles (../interface/profiles.md) |

## Choosing the creature (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "Choose Your Creature" — three gate stones, placed in turn (the tiger stone, the ape stone, then the cow stone carved from a blank rock), open the creatures' gates; the three creatures show off in their glade and the player picks one | todo | see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: "The Lost Brother" — a woman's brother has wandered off sick; bringing him home earns the ape gate stone (worse alignment for killing or dropping him) | todo | see [gold_scrolls/the_lost_brother.md](gold_scrolls/the_lost_brother.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: "The Sculptor" — the player brings a rock from the quarry, the sculptor carves the third gate stone from it | todo | see [gold_scrolls/the_sculptor.md](gold_scrolls/the_sculptor.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gate stones taken away or thrown into the sea come back to where they belong | todo | see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md) (the stone guards); the influence, timer and effect commands are stubs in `src/CHLApi.cpp` |
| The advisors point out the gate stones and the quarry rock while the player hasn't got them | todo | see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md) and [gold_scrolls/the_sculptor.md](gold_scrolls/the_sculptor.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The creatures of the glade the player didn't choose wander off | todo | see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md): in the game the two not chosen are deleted during the fade to black, they don't wander off; see ../creature/ |

## The creature's learning (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "The Creature's Learning" — Sable the trainer teaches the creature in five lessons: seeing its home pen, learning to eat, being slapped and stroked, the Leash of Learning, and tying the leash to a tree, where she also shows the leashes of aggression and compassion; the good-and-evil leash lesson itself is cut and never played | todo | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Each stage has its own scroll, reminder and advisor lines, and the next waits for the last | todo | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| After the last lesson Sable joins the player's Norse village as an ordinary villager; after the earlier lessons she goes back into the temple, or vanishes | todo | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md#the-trainers-exit-and-aftermath): she goes back to the temple after the first three lessons and vanishes after the leash lesson; the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| How the creature learns from these lessons | todo | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md); see ../creature/lessons_and_help.md; the learn-to-eat lesson's lines are in ../creature/feeding_and_thrown_things.md |

## The big creature and the storm (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide (a big creature) wanders the land and asks to meet the player's creature | todo | see [creature_guide.md](./creature_guide.md) |
| The guide teaches the creature to impress a village and then to do it on its own | todo | see [creature_guide.md](./creature_guide.md) |
| The guide teaches the creature to fight, healing both after the bout | todo | see [creature_guide.md](./creature_guide.md) |
| The storm ends the land's lessons: Nemesis's storm kills the guide and the scripts of the land wind down | todo | see [creature_guide.md](./creature_guide.md) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Ogre" — an ogre's guardian stone blocks the way; the player's creature fights the ogre (or puts him to sleep) for the reward | todo | see [silver_scrolls/the_ogre.md](silver_scrolls/the_ogre.md); waits on the guide's fight lesson; the dialogue, camera, creature-making and reward commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Saviour" — a freak wave leaves five men drowning; only the creature is tall enough to wade out and save them | todo | see [silver_scrolls/the_saviour.md](silver_scrolls/the_saviour.md); the villager-making, timer, influence and dialogue commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "Throwing Stones" — a target game of throwing rocks at targets (and not at the houses) | todo | see [silver_scrolls/throwing_stones.md](silver_scrolls/throwing_stones.md); never started; the hand demo, highlight and reward commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Lost Flock" — a shepherd's sheep have strayed; bring them back to his pen (better alignment the more come back) | todo | see [silver_scrolls/the_lost_flock.md](silver_scrolls/the_lost_flock.md); the dialogue, flock, camera, highlight, snapshot and reward commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Pied Piper" — a piper lures the village's children into his cave; the creature leashes him, drags him out and carries him back to free them (eating or drowning him is the evil ending) | todo | see [silver_scrolls/the_pied_piper.md](silver_scrolls/the_pied_piper.md); never started; the leash, flock, dialogue and reward commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Hermit" — a hermit won't worship until he sees a huge creature; impressing him, damaging his hut or killing him changes the alignment | todo | see [silver_scrolls/the_hermit.md](silver_scrolls/the_hermit.md); the dialogue, property, villager-state and reward commands are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Explorers" — missionaries want a boat to sail away in and sing three verses while it is made; the player helps them leave | todo | see [silver_scrolls/the_explorers.md](silver_scrolls/the_explorers.md); only the music, campfire and interaction-level commands work, the rest are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Singing Stones" — a circle of stones sings when they are put back in the right order | todo | see [silver_scrolls/the_singing_stones.md](silver_scrolls/the_singing_stones.md); never reached (the land's set-up script stops first); most commands it needs are stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Immersion Mushrooms" — a man wants the most powerful mushroom, the one that shakes most, for an experiment | todo | see [silver_scrolls/the_immersion_mushrooms.md](silver_scrolls/the_immersion_mushrooms.md); only runs with a force-feedback mouse, which openblack never reports; the puzzle and dialogue commands are stubs in `src/CHLApi.cpp` |
| Silver scroll (no title in the game's text): a creature breeder offers other creatures to swap for | todo | see challenges_and_rewards.md; [silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |

## Leaving the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: leave through the vortex — when the guide's lessons are done the vortex opens; the player sends people through and follows to the second land | todo | see [gold_scrolls/leave_through_the_vortex_land_1.md](gold_scrolls/leave_through_the_vortex_land_1.md); vortex mechanics: [portals.md](portals.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Loading the second land from the story | todo | `LoadMap` in `src/CHLApi.cpp` has its body commented out, so it does nothing; the land's scripts never reach it anyway |
| An unused script for taking over the land's villages with the guide's help | n/a | in the scripts but never started by the game; see [creature_guide.md](./creature_guide.md) |
