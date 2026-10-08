# Species and choosing a creature

The player's creature is one of the game's animal species. At the start of the first land the player chooses between
three young creatures; later, silver scroll challenges offer other species ([../story/silver_scrolls/creature_swaps.md](../story/silver_scrolls/creature_swaps.md)), which the player can swap to, keeping
everything their creature has learnt. Each species looks, moves, sounds and grows its own way and has its own
appetite, strength and knack for learning miracles.

**Progress: 28/48 done, 6 partial — 65%**

## The species

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ape (the giant ape), with its own body, animations and voice | done | `CreatureType::GiantApe`; spawned with its rig (`Data/CTR`), meshes, morphs and voice bank by `CreatureArchetype` and the debug spawner |
| Cow | done | as above |
| Tiger | done | as above |
| Leopard | done | as above |
| Wolf | done | as above |
| Lion | done | as above |
| Horse | done | as above |
| Tortoise | done | as above |
| Zebra | done | as above; offered by [The Riddles](../story/silver_scrolls/the_riddles.md) |
| Brown bear | done | as above |
| Polar bear | done | as above |
| Sheep | done | as above |
| Chimp | done | as above |
| Ogre | done | as above; its bank is the "greek" one (`CreatureAudio.cpp`) |
| Mandrill | done | as above |
| Rhino | done | as above |
| Gorilla | done | as above |
| Chicken and crocodile, listed by the game but with no bodies in its data | n/a | never playable in the five lands |
| Species added by Creature Isle | n/a | Creature Isle |
| Each species' row in the game's creature tables, the ape's first | done | `creature::InfoRow`, `creature_mind_body::SpeciesFromRow`; test `SpeciesRowsStartWithTheGiantApe` |

## How species differ

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' size, fatness and strength | done | `CreatureArchetype::StartScale`, `StartBody` from the species tables |
| Each species walks, runs and goes slowly at its own speeds | done | species speeds in `GCreatureInfo`; see [locomotion.md](locomotion.md) |
| Each species gets hungry, thirsty, tired and grows at its own rates, and likes its own temperature | done | `GCreatureInfo` rates; see [physiology.md](physiology.md) |
| Each species has its own innate leanings: how nice, aggressive, lethargic, friendly or talkative it starts | partial | the starting desire tables are read per species ([desires.md](desires.md)) |
| Some species learn a miracle from fewer sightings than others | done | `CreatureWatching` per-species miracle rules; see [learning_by_observation.md](learning_by_observation.md) |
| Each species pays for miracles with its own energy rates | done | `chantsPerEnergy`, `spellEnergyFloor`; see [creature_casting.md](creature_casting.md) |
| Each species has its own special fight move | partial | see [fighting.md](fighting.md) |
| Each species leaves its own footprints | done | `CreatureFootprints` per-species prints; see [locomotion.md](locomotion.md) |
| Each species has its own places for tattoos | done | see [creature_tattoos.md](creature_tattoos.md) |
| Some species have hair that moves | done | see [face_eyes_hair.md](face_eyes_hair.md) |
| The ogre can never be held by the hand | done | `creature_hand::MayHold`; test `TheHandHoldsAnyPlayersCreature` |
| Other ogre limits (unconfirmed which) | todo | |
| The hand's tooltip and the creature's help texts name its species | todo | |

## Choosing the first creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the first land the player is shown three young creatures to choose from: an ape, a cow and a tiger | todo | the first land's challenge is in [../story](../story/); the shipped quest: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md) |
| Each of the three is shown off, and the good and evil advisors argue for and against it | todo | |
| Picking one up and putting it down, or clicking it, chooses it; the others leave | todo | in the shipped quest the choice is two clicks on the same creature, and the other two are deleted rather than leaving: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md#choosing) |
| The chosen creature takes the name the player gave in their profile | partial | the player page holds a creature name (`GameMenu.cpp`); nothing names the creature from it |
| A player who already has a creature from an earlier game can keep it and skip the creature training | todo | `IS_KEEPING_OLD_CREATURE`, `CAN_SKIP_CREATURE_TRAINING`, `CURRENT_PROFILE_HAS_CREATURE`, `LOAD_MY_CREATURE` are stubs in `CHLApi.cpp` |
| The new creature becomes the player's leashable creature | done | `LeashSystem::ClaimOnArrival` |
| The new creature starts young, at the first stage of growing up | partial | see [development_phases.md](development_phases.md) |

## Swapping species later

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll challenges in the lands offer other species (unconfirmed which in each land) | todo | see [../story](../story/) |
| The offered creature waits for the player to come and look at it | todo | |
| Choosing it swaps the player's creature for the new species, keeping its mind: desires, learning, opinions, known actions and miracles | todo | `SWAP_CREATURE` is a stub |
| The swap keeps the creature's name, alignment, strength and size (unconfirmed which carry over) | todo | |
| The old creature leaves | todo | |
| Scripts can create a creature next to another | todo | `CREATURE_CREATE_RELATIVE_TO_CREATURE` is a stub |
| Scripts can make a creature, give it to a player and name it | todo | `CREATURE_SET_PLAYER`, `SET_CREATURE_NAME` are stubs |

## Other gods' creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from their own mind files | partial | land scripts can create a creature from a mind file (`FeatureScriptCommands::CreateCreatureFromFile`); the story's creatures aren't created yet |
| Skirmish and computer players' creatures come from the game's template minds (destroy towns, impress towns, protect towns, destroy other creatures) | partial | the files are read (`components/creaturemind`); computer players aren't in openblack ([../multiplayer](../multiplayer/)) |
| Scripts can set what a computer player's creature likes | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is a stub |
