# Food

What the villagers live on. Food comes from fields, fish farms, livestock and the food miracle, is kept in each town's
store, and is eaten by villagers, worshippers and the creature. The player can give food by hand.

**Progress: 5/20 done, 4 partial — 35%**

## Where food comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fields' crops grow over the turns, faster on good land and by the weather, until ripe | done | `FieldSystem`, `src/3D/FieldCrop.h`, test `test_field_crop` |
| Farmers sow fields, several times before the crop grows | todo | fields start sown; see `../villager/` |
| Farmers harvest a ripe field and carry the crop to the store | todo | |
| A field's food runs out as it is harvested, and it is sown again | todo | |
| The water miracle and rain speed a field's growth | partial | rain does (`FieldSystem`); the water miracle's effect on fields is blocked on the crop's watering |
| Fishermen fish at the town's fish farms and bring fish to the store | todo | see [fish](../animal/fish.md) |
| Shepherds take livestock to be slaughtered for food | todo | see [livestock](../animal/livestock.md) |

## Eating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry and fetch food from the store to eat | todo | a hunger field exists on villagers, nothing uses it; see `../villager/` |
| Villagers with no food starve and die | todo | |
| Worshippers eat at the worship site | todo | see `../worship/worship_sites.md` |
| The creature eats from the store, the fields and the fish farms, taking the town's food | todo | see `../creature/` |
| Food from a powered-up food miracle makes those who eat it faster for a while | partial | the pile sparkles (`MagicResources.cpp`); nobody eats it yet |
| Poisoned food poisons those who eat it; the heal miracle cures them | partial | the poisoned state and its cure exist (`src/ECS/Components/Poisoned.h`); food is never poisoned; see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned village store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| How much food each thing is worth (fish, grain, a handful from a field, animals) | todo | |

## A town's food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting food | done | `AbodeArchetype` |
| The store's food shows as one pile that rises with what it holds | done | see [stores and piles](stores_and_piles.md) |
| A town short of food raises its food desire flag | partial | the town's desires read the store (`TownDesireSystem.cpp`); see `../town/` |
| Giving food to a town that wants it impresses the town | done | `ReactionMultiplier` (`src/Magic/Impressiveness.cpp`) |
| Food poured on a store teaches the creature to feed the town | done | `src/Magic/MiracleDeeds.cpp` |
| Scripts read, add and take a town's food | todo | stubs in `src/CHLApi.cpp` |

The food miracle is in `../miracles/food.md`.
