# Villager jobs

Every adult has a job from the villager tables: housewife, farmer, forester, fisherman, shepherd, leader or trader. Jobs
gather food and wood for the town, and any adult can be sent to build, fetch or supply when the town wants it. A villager
put to work by the hand becomes a disciple (see disciples.md).

**Progress: 1/30 done, 2 partial — 7%**

## Common to all jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tribe has the seven jobs, each with its own model, tool and speeds | partial | `src/Enums.h`, models in `VillagerArchetype.cpp`; tools are not shown |
| A villager carries the tool or load of what it is doing | todo | See tools_and_carried_items.md |
| Villagers carry food and wood up to their job's capacity, slowed by the load | partial | the load slowdown is in `src/ECS/VillagerSpeed.cpp`; nothing is ever carried |
| Gatherers take what they gather to the storage pit and drop it off | todo | |
| A villager who is thrown or dies drops only wood (over 50, as a log); food and the rest are lost, never a pile | todo | See [tools_and_carried_items.md](tools_and_carried_items.md) |
| Without a storage pit, the town keeps its resources in temporary pots or in abodes | todo | See ../town/storehouse.md |
| An unemployed villager looks for work its town has room for | todo | |

## Food jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Farmers look for the town's best field, walk to it and plant the crop | todo | fields start sown and grow on their own (`src/ECS/Systems/Implementations/FieldSystem.cpp`) |
| Farmers harvest ripe crops at harvest time and dig up what is left | todo | |
| Fishermen look for water or the town's fish farm, fish there and bring fish home | todo | fish farms are not created (CREATE_TOWN_FISH_FARM is empty) |
| Shepherds find a flock, take control of it, lead it to food and water and bring it back | todo | |
| Shepherds fetch strays and wait for the flock | todo | |
| Shepherds pick an animal for slaughter and slaughter it for food | todo | |
| Housewives fetch food from the storage pit, gossip round it, and cook dinner at home | todo | |
| Housewives do housework when nothing else needs doing | todo | |
| The town decides when it is harvest time | todo | |

## Wood and building jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters go to the town's forest and chop trees for wood | todo | |
| Foresters chop wood for a building site and take it there | todo | |
| Villagers take wood from big forests, from rocks the game counts as wood, from pots, and from fallen trees | todo | |
| Builders fetch wood from the storage pit and walk to a building site | todo | See ../building/construction.md |
| Builders stand round the site and build it up, using wood per stroke | todo | |
| Builders never carry scaffolds: the game leaves that empty | n/a | See [tools_and_carried_items.md](tools_and_carried_items.md) |
| Only craftsman disciples bring wood from the storage pit to the workshop; ordinary villagers never supply it | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) and [disciples.md](disciples.md) |
| Builders repair damaged buildings when the town wants repairs | todo | |
| A villager waits for wood when the site has none and none can be found | todo | |

## Worship and trade

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worshippers walk to the worship site, dance and pray, rest at the altar and come home | todo | See ../worship/ |
| Villagers carry food from the storage pit to the worship site for the worshippers | todo | the town's desire for worship supplies is none in the game too (`src/ECS/TownDesire.cpp`) |
| Worshippers hide at the worship site when not dancing | todo | |
| Traders take a town's excess food or wood from its storage pit and trade it in abodes, food for wood or wood for food | todo | |
| Traders pick up and drop off excess from abodes | todo | |
| Leaders have no work of their own: they are never needed for anything and live as ordinary villagers | done | openblack treats them like any idle villager (`VillagerHome.cpp`) |
