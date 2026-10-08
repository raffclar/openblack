# Damage, burning, repair and ruins

Buildings take damage from thrown rocks, the creature, fire and miracles. A damaged building loses chunks, stops
working when badly hurt and empties of people; it can burn down, and towns repair what is damaged. A building
destroyed outright goes, and the town keeps a plan to rebuild it.

**Progress: 13/24 done, 3 partial — 60%**

## Damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A building has life from 1 down to 0, lowered by harm | done | `src/ECS/WorldObjects.cpp` |
| A building being hurt sends its people out | done | `WorldObjects.cpp` |
| A building left with no life stays standing, useless, when a miracle destroys it | done | `WorldObjects.cpp` |
| Harm to a building is an attack on its town | done | See ../town/emergencies_and_aggression.md |
| A hit from a thrown object damages it by the impact | todo | See ../physics/ |
| Rocks knock real holes in the building: the mesh is cut, pieces fly off and fall, larger ones landing as rubble | done | `BuildingDamageSystem`, `src/Physics/DamageMesh.*` (test `test_damage_mesh`); see [../physics/impact_damage.md](../physics/impact_damage.md) |
| A badly damaged building stops working until repaired | todo | |
| Damage to the town centre or storage pit raises an emergency | todo | |
| The creature kicking or stomping on a building damages it | todo | See ../creature/ |
| Miracles that destroy things damage or flatten buildings | partial | buildings take the hit (`WorldObjects.cpp`); see ../miracles/ |
| Heal restores a building's life | done | `WorldObjects.cpp`; see ../miracles/ |
| A totem, the temple and teleport stones can't be destroyed by miracles | done | `WorldObjects.cpp` |

## Burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Buildings catch fire, burn by their material and spread fire to what is near | done | `src/ECS/Systems/Implementations/FireSystem.cpp`; test `test_fire.cpp` |
| A large burning building lights the land round it | done | `FireSystem.cpp` |
| Villagers come to beat the fire out | done | See ../villager/reactions.md |
| A building burnt down flickers out as a ghost of itself and goes | done | `src/ECS/WorldObjects.cpp`, `src/ECS/Components/DestructionGhost.h`, `src/Graphics/Renderer.cpp` |
| Villagers inside a burning building come out | partial | the harm empties it (`WorldObjects.cpp`); villagers at home can't catch fire |
| Rain cools burning buildings | done | `FireSystem.cpp` (rain cooling); see ../weather/ |

## Repair and ruins

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town wants repairs for its damaged, lived-in abodes | done | `src/ECS/TownDesire.cpp`; see ../town/town_desires.md |
| At most one repair site opens per town turn | todo | |
| Builders repair the building with wood, back to whole | todo | |
| A repaired building works again and its damage is removed | todo | |
| A destroyed building leaves a plan to rebuild it, so the town can rebuild | todo | |
| Its people become homeless and its stores lose their piles | partial | the building's villagers are let go (`WorldObjects.cpp`); homeless list and piles not handled |
