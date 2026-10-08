# Villager death

Villagers die of old age, starvation, fire, drowning, falls, predators, miracles and the creature. They fall, lie dead
for a while (as a skeleton when burnt), their soul rises, those nearby mourn them and the town buries them in its
graveyard if it has one.

**Progress: 11/28 done, 5 partial — 48%**

## Life and harm

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers have life, lowered by harm, and die when it runs out | done | `src/ECS/WorldObjects.cpp` |
| Harm to a villager is an attack on its town by whoever did it | done | `WorldObjects.cpp`, `src/ECS/TownAggression.cpp`; see ../town/emergencies_and_aggression.md |
| Wounded villagers walk slower, badly wounded ones crawl | done | `src/ECS/VillagerSpeed.cpp` |
| Hurt villagers sleep longer to recover | done | `src/ECS/Systems/Implementations/VillagerHome.cpp` |
| Life slowly comes back | todo | |
| Chanting at the worship site costs worshippers life, killing some | todo | See ../worship/ |
| The heal miracle gives life back | done | See ../miracles/ |

## Ways to die

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Burnt to death by fire | done | `src/ECS/Systems/Implementations/VillagerFire.cpp` |
| Killed by a miracle or effect that destroys things | done | `WorldObjects.cpp` |
| Brought down and eaten by a predator | partial | the animal side drives it (`AnimalSystem`), `VillagerEaten.cpp` keeps the villager lying still |
| Eaten, crushed or thrown by the creature | partial | See ../creature/ |
| Drowned when dropped or thrown into the sea | done | `villager_physics::Sink`, `Drowning` (600 turns), credited to the dropper or the last interacting player (`villager_fire::DeathCause`) |
| Killed by a fall from a throw | partial | `PhysicsGameHooks::ReactToImpact`, `villager_physics::Land`; the death keeps no cause or killer |
| Dies of old age | todo | |
| Dies of hunger | todo | |
| Sacrificed at the altar | todo | See ../worship/ |

## Dying and after

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The villager falls dying, then lies dead for a fixed time and goes | done | `VillagerFire.cpp`, `src/ECS/Components/VillagerDeath.h` |
| A burnt villager leaves a skeleton once its flesh is gone | done | `VillagerFire.cpp` |
| The body stays shorter when the town has a graveyard | todo | |
| A dying villager drops its wood as a log (unless the creature eats it); its food is lost | todo | See [tools_and_carried_items.md](tools_and_carried_items.md) |
| The villager's soul rises to heaven or hell, fading out | todo | |
| A death message or help sprite shows for a death the player caused | todo | |
| The owner's alignment moves for a death they caused | partial | See ../worship/ |
| The town counts its deaths and its people | partial | the town's counts are rebuilt each turn (`TownDesireSystem.cpp`), deaths are not kept |
| Its abode and town forget the dead villager | done | `WorldObjects.cpp` |
| Nearby villagers point at, go to, look at and mourn the dead | todo | |
| A dead mother's children are orphaned and mourn | todo | |
| The dead are buried in the graveyard, which grows a grave for each | todo | See ../building/civic_buildings.md |
