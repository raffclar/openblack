# Town storehouse

The storage pit is the town's store of food and wood: piles of wood and a pile of food beside it, which grow and shrink
as villagers bring and take, and which the player can add to by hand or miracle. Without one the town makes do with
temporary pots and its abodes' own stores.

**Progress: 5/21 done, 3 partial — 31%**

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit holds five wood piles and a food pile placed by its model | done | `src/ECS/Archetypes/AbodeArchetype.cpp`, `src/ECS/Components/StoragePit.h` |
| Scripts start the pit with food and wood | done | CREATE_ABODE's amounts, `AbodeArchetype.cpp` |
| Piles rise out of the ground and sink as they fill and empty | done | `src/ECS/Components/ResourcePile.h`, `src/Magic/ResourcePiles.h`; test `test_resource_piles.cpp` |
| Wood is added and taken pile by pile in the game's order | partial | `src/ECS/Systems/Implementations/MagicResources.cpp` adds; nothing takes yet |
| The town's total food and wood follows the piles | done | `MagicResources.cpp` |
| A pit can hold only so much; the excess is left over | todo | |
| Villagers drop off food and wood they gather at the pit | todo | See ../villager/jobs.md |
| Housewives and builders take food and wood from the pit | todo | |
| Food and wood miracles cast on the pit fill it | done | `MagicResources.cpp`; see ../miracles/ |
| The pit is the town's resource drop point; the nearest edge is used | todo | |
| Without a pit, temporary pots near the town hold its resources | todo | CREATE_TOWN_TEMPORARY_POTS is not implemented |
| Abodes keep some food of their own for dinner | partial | `src/ECS/Components/Abode.h` keeps food and wood; nothing uses them |

## The player and the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can take food or wood from the pit | todo | See ../resources/ |
| Dropping food or wood into the pit by hand adds it, and villagers react | todo | |
| Taking from a town's pit lowers its belief in the taker for a while | todo | |
| A pit belonging to another player gives its belief to that player when touched | todo | |
| Food in the pit can be poisoned, harming those who eat it | todo | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Magic food in the pit speeds up those who take from it, with a sparkle | partial | the sparkle is shown (`ResourcePile.h`); see ../villager/daily_routine.md |
| The creature can eat from the pit and copy what the player does with it | todo | See ../creature/ |
| Damaging the pit puts the town in a state of emergency | todo | See emergencies_and_aggression.md |
| The pit casts a shadow at night | todo | |
