# Wood

What the towns build and repair with. Foresters fell trees and carry the logs to the store; the player can pull up trees
or pour the wood miracle; builders take wood from the store to building sites and the workshop.

**Progress: 4/17 done, 2 partial — 29%**

## Where wood comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters fell trees of the town's forests and carry the logs to the store | todo | see `../villager/` |
| A tree gives wood by its kind and size | todo | the tree table is loaded, nothing reads its wood |
| A tree a forest miracle grew gives more wood, by its caster's tribal power | partial | the multiplier is kept on the tree (`MagicTree::woodMultiplier`); nobody fells it |
| Big forests give wood without being used up (unconfirmed) | todo | |
| Uprooted (dead) trees and felled trunks count as wood | todo | |
| A tree pulled up by the hand is held as branches or logs by its kind (evergreen, fruit, hardwood) | todo | the hand models are listed with the pots (`src/Enums.h`); see [handling](resource_handling.md) |

## Storing and using wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting wood | done | `AbodeArchetype` |
| The store's wood shows as five piles that fill in turn | done | see [stores and piles](stores_and_piles.md) |
| Builders carry wood from the store to building sites and scaffolds | todo | see `../building/` |
| The workshop takes wood to make scaffolds | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Damaged buildings are repaired with wood | todo | |
| Wood poured near a workshop, building site or scaffold goes to it | todo | only stores and piles take poured wood (`MagicResources.cpp`) |
| A town short of wood raises its wood desire flag | partial | the town's desires read the store; see `../town/` |
| Giving wood to a town that wants it impresses the town | done | `ReactionMultiplier` |
| Wood poured on a store or by a building site teaches the creature to do the same | done | `src/Magic/MiracleDeeds.cpp` |
| The creature carries wood and builds with it | todo | see `../creature/` |
| Scripts read, add and take a town's wood | todo | stubs in `src/CHLApi.cpp` |

The food and wood miracles are in `../miracles/`.
