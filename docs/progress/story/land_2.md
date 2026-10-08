# Land 2

Khazar's land: the player arrives through the vortex to find the friendly god Khazar and the hostile Lethys. Khazar
teaches worship and miracles, the player wins over the land's towns, and Lethys ends the land by stealing the creature.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/27 done, 1 partial — 2%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | partial | `Game.cpp` loads `challenge.chl` and starts the story's top script, which runs the land's control script; it stops at the first unwritten native (about 62 of 464 do something, `src/CHLApi.cpp`); the land script's contents: see ../scripts/land2_script.md |
| The player's people come out of the vortex with the creature and found a new town | todo | see ../miracles/teleport.md for vortex-like effects; the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Khazar introduces himself, gives the first scaffold and the first one-shot miracles | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Lethys shows himself with a fake fireball attack and his computer player starts | todo | the computer player commands are stubs |
| The land's eleven towns are watched and the next challenges open as the player wins them over | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Khazar is the player's ally and Lethys an enemy god with his own temple and creature | todo | see ../multiplayer/player_diplomacy.md |

## Khazar's lessons (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "Worship Site" — Khazar shows the player the worship site, raising and lowering the village totem, and how worshippers make prayer power | todo | every step in [gold_scrolls/worship_site.md](gold_scrolls/worship_site.md) (a builder disciple raises the worship site first, then the totem and miracle lessons); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll: "Impress Village" — Khazar teaches casting a miracle by gesture without going back to the temple, and impressing a village with it | todo | every step in [gold_scrolls/impress_village.md](gold_scrolls/impress_village.md) (starts 20 minutes after the worship lesson: gestures, then filling a Norse village's store); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll: "Khazar's Fireball Challenge" — three huts as targets and fireball seeds to throw at them | todo | every step in [gold_scrolls/khazars_fireball_challenge.md](gold_scrolls/khazars_fireball_challenge.md), also the "Miracle Challenge" scroll and Khazar's tour that start both challenges; never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll: "Khazar's Shield Challenge" — Khazar shows a Physical Shield at a lone hut on an island, hands over three shield seeds, and drops three boulders of his own on the hut the player has shielded | todo | every step in [gold_scrolls/khazars_shield_challenge.md](gold_scrolls/khazars_shield_challenge.md); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Khazar's lesson on influence: grow towards Lethys | todo | not a scroll: covered in [gold_scrolls/impress_village.md](gold_scrolls/impress_village.md#khazars-lesson-on-influence); it starts the land's last scroll; never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll: "The Workshop" — a man sent by Khazar shows the workshop and how scaffolds plan new buildings | todo | every step in [gold_scrolls/the_workshop.md](gold_scrolls/the_workshop.md); the reward is the Forest miracle; never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Sea" — a woman's children have swum out too far; save them (or don't) | todo | every step in [silver_scrolls/the_sea.md](silver_scrolls/the_sea.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Beach Temple Puzzle" — move a flooding temple's rings up the beach column by column, a ring never on a smaller one | todo | every step in [silver_scrolls/the_beach_temple_puzzle.md](silver_scrolls/the_beach_temple_puzzle.md), the ring rules in [minigames.md](minigames.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Greedy Farmer" — a farmer's cows are stolen by children; what the player does to the thieves and the farmer sets the alignment | todo | every step in [silver_scrolls/the_greedy_farmer.md](silver_scrolls/the_greedy_farmer.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Idol" — a man has built an idol to worship; burn it, or the people, or leave it | todo | every step in [silver_scrolls/the_idol.md](silver_scrolls/the_idol.md); the idol is made and held by the script, not an artefact (see ../town/artefacts.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Riddles" — a woman's riddles ask for things put in a stone ring ("something which howls at night", "something hot" …) | todo | every step in [silver_scrolls/the_riddles.md](silver_scrolls/the_riddles.md); "something hot" is any burning object within 10 m of the altar (see ../nature/rocks_splitting_and_heat.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Plague" — Lethys poisons a village; heal it and stop the plague spreading | todo | every step in [silver_scrolls/the_plague.md](silver_scrolls/the_plague.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Sacrifice" — a tribe's altar turns anything laid on it into prayer power; what is sacrificed sets the alignment | todo | every step in [silver_scrolls/the_sacrifice.md](silver_scrolls/the_sacrifice.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll (no title in the game's text): a spiritual healer in a village and what the player lets him do | todo | not actually a scroll (no highlight, no log entry, no title): every step in [silver_scrolls/the_spiritual_healer.md](silver_scrolls/the_spiritual_healer.md); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Slavers" — strangers who want wild animals for a circus are holding kidnapped villagers; a villager first warns of them | todo | every step in [silver_scrolls/the_slavers.md](silver_scrolls/the_slavers.md); the separate warning scroll is compiled but never started; never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Singing Stones" — stones that each have a voice; playing their melodies wakes the spirits of the ancients | todo | every step in [silver_scrolls/the_singing_stones_land_2.md](silver_scrolls/the_singing_stones_land_2.md) (a different quest from land 1's of the same title); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| A bronze puzzle of trees with a miracle dispenser as its prize | todo | a puzzle with a rules signpost, not a silver scroll: its rules and prize are in [minigames.md](minigames.md) (Tree puzzles); never reached: the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |

## The end of the land (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar is killed by Nemesis | todo | a gold story-log entry ("Nemesis. No!"): every step in [gold_scrolls/nemesis_no.md](gold_scrolls/nemesis_no.md); set off by the first town Lethys loses, the player's sixth town or Khazar losing his towns; never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll, the land's last: win or wipe out Lethys's three towns | todo | every step in [gold_scrolls/destroy_it.md](gold_scrolls/destroy_it.md) (logged as "Destroy it!"); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Lethys takes the creature away through a vortex and the advisors despair | todo | a gold story-log entry with a silver "follow now" scroll: every step in [gold_scrolls/lethys_has_taken_our_creature.md](gold_scrolls/lethys_has_taken_our_creature.md); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
| Gold scroll: leave through the vortex — the player follows Lethys into the third land | todo | every step in [gold_scrolls/leave_through_the_vortex_land_2.md](gold_scrolls/leave_through_the_vortex_land_2.md), the vortex in [portals.md](portals.md); never reached: Land 2's control script never runs in openblack and the scroll, dialogue, log and rival-god commands are stubs in `src/CHLApi.cpp` |
