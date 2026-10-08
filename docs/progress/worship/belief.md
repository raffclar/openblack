# Belief and impressiveness

How much a town believes in each player. Villagers who see something impressive, kind or terrible, believe more in the
god who did it; enough belief wins the town over (see `../town/` for conversion). Artefacts left in other players' towns win belief too: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 11/24 done, 1 partial — 48%**

## Being impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager who sees something impressive gives its town belief in whoever did it, by how impressive it was, the land's balance and the villager's share of the town | done | `ReactionSystem::Impress` (`src/ECS/Systems/Implementations/ReactionSystem.cpp`), `src/Magic/Impressiveness.cpp` |
| Closer watchers are more impressed, along an S curve of distance | done | `DistanceChangeToBelief` |
| Food and wood impress by how much the town wants them | done | `ReactionMultiplier` |
| Things that belong to no player impress no one | done | `ReactionSystem::Impress` |
| A town grows bored of seeing the same kind of thing; boredom wears off each town turn, times the land's lost-town scale | done | `BoredomAfterImpression`, `BoredomAtTownTurn` |
| Creatures are impressed by their own god's and other creatures' deeds | done | `CreatureImpression`; see `../creature/` |
| Villagers hiding indoors are still impressed by what they see | done | `ReactionSystem.cpp` |
| Artefacts given to a town keep impressing it | todo | |
| Throwing things into a town impresses it (and frightens it) | todo | needs the hand to hold objects, see `../hand/` |
| The creature's impressive actions win belief for its god | todo | see `../creature/` |

## A town's belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belief gained waits until the town's turn, then is believed at once, times the town's belief scale | done | `src/Magic/TownBelief.cpp`, test `test_town_belief` |
| Belief in a player is capped (10 unless a script sets another cap) | done | same |
| A symbol of the belief gained rises from the town centre in the player's colour | done | `villager_reactions::ShowTownBelief` |
| Each town keeps a fading tally of the belief each player has lately won | done | same |
| Once claimed, a town's belief is reset by the claimed-town multiplier | todo | |
| Losing a town lowers belief in that player elsewhere | todo | |
| Hurting a town lowers its belief in the player who did it (unconfirmed exact rule) | todo | |
| The belief a player needs to win a town grows with the belief other players have there | todo | see `../town/` |
| A town nearly lost makes the help sprites warn the player | todo | |
| A town's belief shows on its scroll and the town's status | todo | see `../interface/` |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts set a town's starting belief in a player and its cap | partial | stored on the town (`Town::beliefs`) by `src/LHScriptX/FeatureScriptCommands.cpp`, but the belief rules don't read it |
| Land scripts set a town's belief scale | todo | stub |
| Challenge scripts read a player's belief in an object and set it | todo | stubs in `src/CHLApi.cpp` |
| Challenge scripts scale how impressive an object is | todo | stub |
