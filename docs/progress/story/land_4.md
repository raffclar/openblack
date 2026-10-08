# Land 4

Khazar's land again, now ruined by Nemesis: meteors fall, a village has been turned undead and ogres guard the way. The
player rebuilds, helps the survivors and the creature is given the first of the creeds that will protect it.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/15 done, 1 partial — 3%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | partial | `Game.cpp` loads `challenge.chl` and starts the story's top script, which runs the land's control script; it stops at the first unwritten native (about 62 of 464 do something, `src/CHLApi.cpp`); the land script's contents: see ../scripts/land4_script.md |
| The player comes through the vortex and finds the land ruined; the creature is kept away from the vortex | todo | covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp`; the advisors' arrival lines depend on whether Lethys was spared on the third land: [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md#sparing-or-finishing-lethys) |
| A man explains what Nemesis has done | todo | covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md) (his story logs that scroll); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Meteors fall on the land: fire, lightning and darkness meteors, aimed near the player's towns | todo | covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); see ../miracles/ for the effects; the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The player's home town is built up and its people are kept from exploring until the story allows | todo | covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The creature is given the creed | todo | covered in [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md#the-creed); (unconfirmed what the creed does in play) |

## Gold scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "The Heartbroken Man" — a nomad's story; how the player answers sets the alignment | todo | full breakdown: [gold_scrolls/the_heartbroken_man.md](gold_scrolls/the_heartbroken_man.md) (its ending breaks the darkness Guardian Stone); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: "Undead Village" — a village of skeletons; set right by raising its two sunken totems | todo | full breakdown: [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md) (raising both sunken totems lifts the curse; the reward is the second Creed); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: "The Totem Puzzle" — a bell memory game at five bell towers by the Japanese village: the towers ring rounds of 3, 5, 7 and 9 notes and the player clicks them back in the same order; the totems to raise belong to Undead Village | todo | full breakdown: [gold_scrolls/the_totem_puzzle.md](gold_scrolls/the_totem_puzzle.md) (it breaks the fire Guardian Stone); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: "The Defending Ogres" — the ogre Sleg and his family guard the way with a guardian stone; gremlins attack the creature | todo | full breakdown: [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md) (logged during the farmer's story; Sleg holds the lightning Guardian Stone); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Gold scroll: leave through the vortex — the way to Nemesis's land | todo | full breakdown: [gold_scrolls/leave_through_the_vortex_land_4.md](gold_scrolls/leave_through_the_vortex_land_4.md); vortex mechanics: [portals.md](portals.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Treacherous Path" — a blind woman walks to her brother with healing potions; the player keeps her safe | todo | see [silver_scrolls/the_treacherous_path.md](silver_scrolls/the_treacherous_path.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Fish Puzzle" — a boy wants to be a fisherman; tapping the water herds the fish away from the hand | todo | see [silver_scrolls/the_fish_puzzle.md](silver_scrolls/the_fish_puzzle.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll (no title in the game's text): the creature breeder returns with creatures to swap | todo | see challenges_and_rewards.md; [silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |
| Bronze puzzles: the Theseus maze, whose prize falls from the sky, and the Japanese village's totem puzzle (raise all the totems), which gives a shield miracle dispenser | todo | see [minigames.md](minigames.md) and, for the totem puzzle, [gold_scrolls/the_totem_puzzle.md](gold_scrolls/the_totem_puzzle.md#quirks-unused-and-cut-parts); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
