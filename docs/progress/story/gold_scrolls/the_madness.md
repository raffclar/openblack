# The Madness

A cut gold-scroll story chapter in which Nemesis takes over the player's creature: it is made strong, turned to the
opposite alignment and sent to throw fireballs at a village's store and houses while the advisors look on, then
confined for a minute and a half until the madness wears off. It was never compiled into the game; the shipped fifth
land does the same story beat as the curse ([i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md)).

**Land:** 5 (never started) · **Giver:** none (a cut scene that starts by itself) · **Script:** CreatureRunsAmok · **Reward:** none · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, its single-quest test launcher, the game's text table (`InfoScript2.txt`) and the
fifth land's map script (`Land5.txt`). Every row is n/a: nothing in the shipped game can start it, so there is nothing
for openblack to do.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is not in the list of files compiled into the shipped `challenge.chl`, so the game does not contain it | n/a | never started by the game: only its own test launcher (also uncompiled) runs it |
| Its challenge name still has an entry in the compiler's list of challenges, so it was a planned story chapter | n/a | never started by the game |
| Its title "The Madness" and the reminder "Your Creature is still out of control!" are in the shipped text table, and so are all eleven lines of its dialogue (Nemesis's four and the advisors' seven) | n/a | never started by the game: the text survived the cut; no shipped script uses the title |
| Its markers fit the fifth land: the village it attacks is the Japanese village there (its centre is about 3 units from the land's town 7, a neutral town at the start), and its camera points are around that village | n/a | never started by the game; no other land has a town within 230 units |
| The shipped fifth land tells the same story differently: Nemesis curses the creature's strength, size and alignment at the start and the player lifts it in three steps | n/a | see [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |

## How it would begin

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no scroll to click: the chapter would start as soon as whatever started it ran the script | n/a | never started by the game |
| The creature's leash is switched off and the creature is walked to a spot beside the village; the script waits until it is within 150 of it | n/a | never started by the game |
| The scroll is logged in the story log as a gold quest titled "The Madness", at nothing done, with the reminder "Your Creature is still out of control!" (good advisor) | n/a | never started by the game |
| The creature is given a Strength miracle that never runs out, and is taught everything | n/a | never started by the game |

## The cut scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The time of day is set to 18:30 and game time is stopped | n/a | never started by the game |
| The camera locks onto the creature and follows it, the lens widening to 35 over 8 seconds | n/a | never started by the game |
| Nemesis: "You are close to your Creature." then "After all this time, you think you know him." | n/a | never started by the game |
| The camera swings round to a point east of the village over 5 seconds, aimed at the creature's chest (80% of its height), then rises 20 over 9 seconds | n/a | never started by the game |
| Nemesis: "But I am stronger than you will ever be."; when the creature reaches its spot (within 5): "And now he obeys only me." | n/a | never started by the game |
| The creature looks into the camera; two seconds later the camera cuts in close to it by the store and rises slowly | n/a | never started by the game |
| A target marker effect is put on the creature for 30 seconds; it is made to want only to be angry, plays its angry animation twice, and the camera shakes (amplitude 0.5 for 2 seconds within 200) | n/a | never started by the game |
| The creature's alignment is flipped to its mirror (a good creature becomes as evil as it was good, and the other way round), in steps of 0.025 | n/a | never started by the game |
| The creature is allowed to attack its own side's town, looks at the store and throws a fireball at it at full energy, then plays angry again | n/a | never started by the game; the store is looked up by its position, about 50 from the village centre |
| Both advisors step out. Good: "He's out of control! Oh this is terrible!" Evil: "We've got to prevent him from doing too much damage." | n/a | never started by the game |
| The creature is moved to the village centre; the camera pulls back above the village. Good: "We've got to get him back, too. He's ours!" | n/a | never started by the game |
| It throws a fireball at a house; the cut scene ends but the advisors keep talking. Evil: "Restrain him first!" It throws a fireball at a second house. Evil: "That's it. We must keep him harmless until this wears off." Both advisors go home | n/a | never started by the game |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is let go, still wanting only anger, and kept within 90 of the first house for 90 seconds; the player can only limit the damage it does | n/a | never started by the game; there is no success or failure test: the chapter simply waits |
| After 90 seconds the leash is switched back on, the Strength miracle is removed, the confinement and the right to attack its own town are taken away | n/a | never started by the game |

## The end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good: "The Creature did some damage when he was out of our control." Evil: "I know. Let's get it cleaned up. As if we haven't got better things to do." Good: "Cleanliness is next to godliness." Evil: "Doh!" | n/a | never started by the game |
| The gold scroll is marked complete | n/a | never started by the game |
| A target effect is put on the creature again and its alignment is turned back to what it was before, in 0.025 steps; game time runs again | n/a | never started by the game |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The stored "mirror" alignment is negated a second time to restore it, so the creature ends where it began; the madness leaves nothing behind except the damage | n/a | never started by the game |
| Commented-out lines show it once looked for the nearest town and its store wherever the creature was, and once made the creature examine the store before attacking; a "TODO cut to creature being horrible to town" note shows it was unfinished | n/a | never started by the game |
| If the targeted village is not the player's, "attack own town" means nothing to it: the fireballs are forced anyway | n/a | never started by the game; on the shipped fifth land this village starts neutral |
