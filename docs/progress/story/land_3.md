# Land 3

Lethys's prison land: the player arrives without the creature, which Lethys holds bound, and with only a few believers.
Lethys sends possessed wolves and fanatics; a monk helps; freeing the creature opens the way on.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/11 done, 2 partial — 9%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | partial | `Game.cpp` loads `challenge.chl` and starts the story's top script, which runs the land's control script; it stops at the first unwritten native (about 62 of 464 do something, `src/CHLApi.cpp`); the land script's contents: see ../scripts/land3_script.md |
| The player comes through the vortex alone and Lethys taunts them | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The land's own weather is switched off for the story | partial | pausing the climate system works (`src/CHLApi.cpp`); the land's script never gets there |

## Freeing the creature (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: free the creature — the creature is held by three prison statues; each one stopped frees it a third | todo | every step (the arrival, the prison, each pillar falling and rising, Lethys giving the creed, sparing or finishing him) in [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md); never reached: Land 3 never loads and the dialogue, scroll, log, villager and creature commands are stubs in `src/CHLApi.cpp` |
| Lethys's possessed wolves attack the player's people; the monk explains and helps | todo | a gold story-log entry of its own: every step in [gold_scrolls/the_wolves_are_possessed.md](gold_scrolls/the_wolves_are_possessed.md); never reached: Land 3 never loads and flocks, villagers and dialogue are stubs in `src/CHLApi.cpp` |
| Lethys sets sixteen of the Japanese village's fishermen alight at their beach campfire; they run burning for the village's buildings and must be put out before they get there | todo | a gold story-log entry of its own: every step in [gold_scrolls/fire_fire_im_on_fire.md](gold_scrolls/fire_fire_im_on_fire.md); never reached: Land 3 never loads; fire works but villagers, dialogue and the rival hand are stubs in `src/CHLApi.cpp` |
| The water one-shot miracles the player is given to fight the fires | todo | see ../miracles/water.md |
| Gold scroll: leave through the vortex — with the creature freed the player leaves for the fourth land | todo | every step in [gold_scrolls/leave_through_the_vortex_land_3.md](gold_scrolls/leave_through_the_vortex_land_3.md), the vortex in [portals.md](portals.md); never reached: Land 3 never loads and highlights and vortices are stubs in `src/CHLApi.cpp` |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Shaolin" — a monk goes off to meditate in a secret place and asks not to be followed | todo | see [silver_scrolls/the_shaolin.md](silver_scrolls/the_shaolin.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Rejuvenator" — an old woman makes the old young; after three, her magic goes wrong on a child, who turns into an ape (or a chimp if the creature already is one) to swap for | todo | see [silver_scrolls/the_rejuvenator.md](silver_scrolls/the_rejuvenator.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| A bronze puzzle of trees with a reward | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
