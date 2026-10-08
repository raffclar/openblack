# Land 5

Nemesis's land: Nemesis curses the creature, his towns defend themselves with shields, and the player takes his land town
by town until the last fight between the creatures and the end of the game.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/15 done, 1 partial — 3%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | partial | `Game.cpp` loads `challenge.chl` and starts the story's top script, which runs the land's control script; it stops at the first unwritten native (about 62 of 464 do something, `src/CHLApi.cpp`); the land script's contents: see ../scripts/land5_script.md |
| Nemesis greets the player with "a surprise": the creature is cursed | todo | full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md) (the curse is logged as a gold quest titled "I have a surprise for you."); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The curse warps the creature's strength, size and alignment | todo | full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md) (one Wonder each for strength, size and alignment, striking once a night); see ../creature/ |
| Lifting the curse in three steps puts the creature back as it was | todo | full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md) (a third per Wonder village won, then a healing scene); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| The volcano rumbles | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Nemesis defends his last town himself | todo | full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md) (meteors, shields and blast barrages); the computer player commands are stubs |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Magic Dragon" — crusaders want healing before they fight the dragon in its cave | todo | full breakdown: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Heavenly Fire" — meteors rain on a village; catch them before they hit | todo | full breakdown: [silver_scrolls/the_heavenly_fire.md](silver_scrolls/the_heavenly_fire.md) (no scroll is shown: it starts by itself after the Greek village is won, and success is judged on the damage the village takes; catching is optional); Burning rocks: see ../nature/rocks_splitting_and_heat.md; the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "Stanley The Wolf" — a blind wolf walks towards ringing bells; ring them to lead it to the sheep | todo | full breakdown: [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll: "The Explorers Again" — the missionaries of the first land arrive in their boat | todo | full breakdown: [silver_scrolls/the_explorers_again.md](silver_scrolls/the_explorers_again.md); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp`; it only starts if the ark sailed in [The Explorers](silver_scrolls/the_explorers.md) on Land 1 |
| Silver scroll: "Swap To Brown Bear" — a stench in the forest; clearing what causes it offers a brown bear to swap for | todo | full breakdown: [silver_scrolls/swap_to_brown_bear.md](silver_scrolls/swap_to_brown_bear.md) (a trail of eight dung piles leads to the bear; a heal miracle chest goes to the home village; it is only offered while the creature is still cursed); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Silver scroll (no title in the game's text): someone hiding in the forest is plotting against a village | todo | full breakdown: [silver_scrolls/the_japanese_traitor.md](silver_scrolls/the_japanese_traitor.md) (a stranger praying at night who tells, wrongly in two cases, which of Nemesis's Wonders does what to the creature); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Cut challenge: "Chimp Posse" — three chimp creatures on a hilltop have lost their ball to Nemesis's creature | n/a | never compiled into the game, no scroll and no reward: [silver_scrolls/chimp_posse.md](silver_scrolls/chimp_posse.md) |
| A village behind Nemesis's spiritual shield must be broken through | todo | full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md) (no scroll and no log entry: a story obstacle, not a logged quest); the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |

## The end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All of Nemesis's towns are won and he says he is not finished | todo | gold scroll "So this is a fight to the death": [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md); see ending.md |
| The big fight between the player's creature and Nemesis's | todo | gold scroll "So this is a fight to the death": [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md); see ending.md and ../creature/fighting.md |
