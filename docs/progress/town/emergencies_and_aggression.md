# Town emergencies and aggression

A town remembers who has harmed it. Harm by its own god makes it want mercy, harm by others makes it want protection.
When something important in it is damaged the town falls into a state of emergency: its people run, hide in buildings
and afterwards gather in the town.

**Progress: 4/15 done, 3 partial — 37%**

## Aggression

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each harm to a town's villager or building adds aggression against whoever did it, the first harm more | done | `src/ECS/TownAggression.cpp`, `src/ECS/WorldObjects.cpp` |
| The more often a town is attacked, the less each attack adds | done | `TownAggression.cpp` |
| Aggression fades each turn into the wants of mercy (from its owner) and protection (from others) | done | `TownAggression.cpp`, `src/ECS/Systems/Implementations/TownDesireSystem.cpp` |
| The town remembers its last attacker and when | done | `TownAggression.cpp` |
| Villagers shelter under a magic shield while the town wants protection | partial | See ../villager/reactions.md |
| Damage done to a town's villagers lowers its belief in the attacker | todo | |
| The town's attitude to the creature changes when the creature harms it | todo | See ../creature/ |

## Emergencies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Damage to the town centre, the storage pit or an abode on fire puts the town in a state of emergency | todo | |
| In an emergency villagers run at fleeing speed | partial | the speed rule is in `src/ECS/VillagerSpeed.cpp`; no town is ever in an emergency |
| Villagers go and hide in a nearby building, then look to see if it is safe | todo | |
| Villagers rush to fight the town's most important fire | partial | firefighting is done (`VillagerFire.cpp`); choosing the most important abode on fire is not |
| After the emergency, villagers gather at the town's congregation point, then go back to their lives | todo | |
| Some villagers always react to their town's emergency, whatever they are doing | todo | |
| The town counts its dying and its recent deaths | todo | |
| A town with no buildings left is completely destroyed and empties | todo | |
