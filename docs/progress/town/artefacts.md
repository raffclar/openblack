# Town artefacts and totems

An artefact is a rock or other loose fixed object the player has put down by a town or worship site. The town's
villagers gather and dance round it; their dances make it worth more to its owner, and in a town that isn't the owner's
they win the owner belief. A worthy artefact glows with a magic effect and then shows its owner's symbol. Each town also
has its town centre totem showing its worship share and, once it has seen the creature, a statue of the creature.

Holding an object in the hand doesn't change it: nothing in the game paints held objects or counts how long they are
held. An artefact's only marks are the magic effect and the owner's symbol.

openblack has none of it: the table values are read (`InfoConstants.h`), the artefact effect's particle type exists
(`ParticleTypes.cpp`), the two villager dance states are placeholders (`LivingActionSystem.cpp`) and the script command
is a stub (`FeatureScriptCommands::MakeLastObjectArtifact`).

**Progress: 1/34 done, 0 partial — 3%**

## Becoming an artefact

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose fixed object (a rock, a dead tree, a toy, a lantern, the idol and the like) put down gently from the hand by a player who isn't neutral becomes an artefact of the town of the nearest building within 50 m, or of the nearest worship site there | todo | see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Only kinds whose table gives an artefact multiplier above 0 qualify (all rocks 0.0002, volcanic 0.00025, singing stones and the idol 0.0003, gate totems 0.0006, the bowling ball 0.0002, other toys, lanterns, fences and dead trees 0.0001), and nothing a script is holding | todo | toys: [../nature/toys.md](../nature/toys.md) |
| Something thrown (faster than 2 m/s across the land as it leaves the hand), or put down by the creature, never becomes an artefact | todo | |
| A new artefact starts worth the kind's villager-impressing value from the tables (0.01 for rocks) | todo | |
| An artefact belongs to the player who put it down; it joins its town's (or site's) artefacts once, and the game's list of all artefacts | todo | |
| An artefact sent through a vortex comes out in the next land still an artefact of the same god with the same worth; only its town is forgotten | todo | see [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |
| An artefact worth more than 1 put down by a town (or site) other than its own impresses it: the villagers come to look at it | todo | |
| Making an artefact can teach the player's creature to copy it | todo | see [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |
| Taking an artefact in the hand takes it out of its town or site and stops its dance; whoever took it becomes its owner, and it keeps its worth | todo | So another player can steal one; `HandGrabSystem.cpp` has the TODO |
| An artefact whose object is gone (or broken in two) goes, with its dance and its effect | todo | |
| A land's feature script can make the last object it made an artefact of a numbered town (or else of the nearest town if that is the named player's, or else of no town), for a named player, at a set worth; no shipped land uses it | todo | `FeatureScriptCommands::MakeLastObjectArtifact` is a stub |

## The villagers' dance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town looks after its artefacts every time it is processed; artefacts at worship sites get no dance of their own | todo | |
| A villager wants to join an artefact's dance less the farther it is (out to 250 m) and the fuller the dance already is; not at all while it is under way | todo | |
| The dance is chosen by the object's width: under 7 m the small object dance, under 10 m the medium one, otherwise the dance for objects wider than 20 m (the dance for objects under 20 m is never chosen) | todo | see [../scripts/dance_scripts.md](../scripts/dance_scripts.md) |
| If the artefact has moved since its dance was set out, the dance moves to it and footpaths are laid to it from the nearest town's storage pit | todo | see [../terrain/footpaths.md](../terrain/footpaths.md) |
| A villager who joins goes to wait by the artefact: from farther than 12 m it walks to a clear spot up to 12 m away on its own side; there it finds itself room, and after waiting its time (200 turns for villagers) it starts the dance | todo | `LivingActionSystem.cpp` lists the wait and dance states as to do |
| When the dance starts, every dancer switches to the artefact dance, which lasts the dance's length | todo | |
| When the dance is over, if the town most wants relaxation the dancers go back to waiting for another dance, otherwise they go back to their lives | todo | |
| A dance left with no dancers ends | todo | |
| The creature can go to an artefact (as near as three of its heights, at most 50 m) and dance round it | todo | see [../creature/town_actions.md](../creature/town_actions.md) |

## Worth and belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each dance finished in its owner's own town adds to its worth: the town's belief in its owner (counted fully up to 2, a third above, at most 3; 1 for a neutral town), times the kind's artefact multiplier, the share of the dance's dancers it had, the share of the dance's length danced and the land's artefact speed (1 unless a script sets it) | todo | |
| What is added is multiplied by 300 and, once the artefact is worth more than 1, divided by its worth, so it grows ever more slowly | todo | |
| While its dance is on in a town that isn't its owner's, every 100 turns (plus a random part up to a quarter) the town gives the owner belief: its worth (only once over 1) × the object's impressiveness × dancers per head of the town's people × 5 | todo | see [belief_and_conversion.md](belief_and_conversion.md) |
| Every 1000 turns each artefact at a worship site gains a little worth from the site's worship dance (0.00001 × the land's artefact speed × the site's dancers over a count of the player's) | todo | `WorshipBattery.cpp` notes it |
| An artefact worth more than 1 makes the object 1 + its worth times as impressive to the villagers that see it | todo | see [../worship/belief.md](../worship/belief.md) |
| Worth more than 1, a magic effect plays over it, as strong as it is worth | todo | The effect's particle type is in `ParticleTypes.cpp` |
| Worth more than 2, its owner's symbol floats over it at half size | todo | |
| The rules are the same for every player, the computer's and other people's in multiplayer | todo | |

## Totems and statues

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town centre's totem pole rises and falls with the town's worship share | todo | See ../worship/ |
| The hand can pull the totem up or down to set how many worship | todo | See ../worship/ |
| A statue of the player's creature is put up in the town centre once the town knows it | todo | |
| The creature can be impressed by, play with or steal the statue | todo | See ../creature/ |
| A totem is not destroyed by miracles | done | `src/ECS/WorldObjects.cpp` |
| The town centre's spell icons show the miracles the town gives its worship site | todo | See ../miracles/ |
