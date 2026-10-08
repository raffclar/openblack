# Civic buildings

Besides homes, a town has civic buildings: the town centre with its totem and spell icons, the storage pit, the
crèche, the workshop, the graveyard, the football pitch, the wonder and the spell dispensers. Each becomes functional
once built and does its own job for the town.

**Progress: 5/28 done, 6 partial — 29%**

## All civic buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Civic buildings are placed by script like abodes, in each tribe's style | done | `src/ECS/Archetypes/AbodeArchetype.cpp` |
| A civic building becomes functional once built, setting up its job | todo | |
| A damaged civic building stops working, and starts again when repaired | todo | See damage_and_repair.md |
| The town counts its civic buildings and wants more as it grows | done | `src/ECS/TownDesire.cpp`; see ../town/town_desires.md |

## Town centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town centre is placed by script with the town's worship share | partial | CREATE_TOWN_CENTRE places it (`FeatureScriptCommands.cpp`); the worship share is dropped |
| It has a door villagers go in by | todo | |
| It shows the town's spell icons, with power-up levels | todo | See ../miracles/ |
| A totem pole for the worship share and a creature statue stand by it | todo | See ../town/artefacts.md |
| A faint column of light in its owner's colour stands over it | todo | see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Belief symbols rise from its foot | done | See ../town/belief_and_conversion.md |
| Damaging it puts the town in a state of emergency | todo | |
| Planned town centres can be built by a town | todo | |

## Storage pit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pit with its wood and food piles | done | See ../town/storehouse.md |
| The desire flags fly over it | todo | See ../town/town_desires.md |

## Crèche

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Children go to the crèche and stay there during the day | todo | |
| Children play at the crèche | todo | |
| Losing the crèche sends its children home | todo | |

## Graveyard

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dead of the town are buried in the graveyard | todo | |
| Each burial adds a grave, which goes through stages | todo | |
| Bodies are cleared away sooner when the town has a graveyard | todo | |

## Football pitch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placed as a building, with its ball | partial | the pitch is placed (`AbodeArchetype.cpp`); its ball is not made |
| Villagers play matches on it at playtime | todo | See ../villager/play_and_gossip.md and [../town/football.md](../town/football.md) |

## Others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spell dispensers give out miracles | done | See ../miracles/ |
| The workshop | todo | See workshop_and_scaffolds.md |
| The wonder | partial | See ../town/wonder.md |
| The citadel and the temple | partial | See ../temple/ |
| Worship sites | partial | See ../worship/ |
| Fields and fish farms | partial | See fields_and_fish_farms.md |
| Walls (planned wall sections) | n/a | no land or playground script uses the command |
| Creature pens | n/a | no land or playground script uses the command |
