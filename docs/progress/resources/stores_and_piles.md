# Stores and piles

Where food and wood are kept: each town's store (its storage pit) with its piles, loose piles and pots on the land, and
the piles the miracles make. The game has only these two resources.

**Progress: 13/24 done, 1 partial — 56%**

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A store has one food pile and five wood piles, at the points its model marks, turned with it | done | `AddStoragePitComponents` (`src/ECS/Archetypes/AbodeArchetype.cpp`) |
| Wood fills the piles in turn; every pile but the last holds no more than it is drawn full at | done | `MagicResources.cpp`, `src/Magic/ResourcePiles.cpp`, test `test_resource_piles` |
| The town keeps count of the food and wood its store holds | done | `GameMagicWorld::AddToStore` |
| A store can hold more than its maximum for a while (unconfirmed what happens to the excess) | todo | |
| A damaged store causes an emergency in its town | todo | see `../town/` |
| A damaged store stops working until repaired | todo | |
| A store's contents can be poisoned, tinting its piles | todo | see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| The store's desire flags stand by it | todo | see `../town/` |
| Villagers' taking food and wood from the store shrinks its piles | todo | nobody takes yet |
| Only food and wood exist; there is no ore | n/a | the game has no other resource |

## Piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pile rises out of the ground as it fills and sinks as it empties, easing over a second | done | `src/Magic/ResourcePiles.cpp`, `MiracleFxSystem`, test `test_resource_piles` |
| Food rises fast at first then slows; wood rises evenly; neither beyond full | done | test `test_resource_piles` |
| A pile is drawn only while some of it is above ground | done | `RenderingSystem.cpp` |
| The grain on a food pile flows down it as it rises | done | test `test_resource_piles` |
| Food piles follow the land's shape | done | `MorphWithTerrain` on food piles (`PotArchetype.cpp`) |
| A pile thuds as it is made or added to, louder for 200 or more; piles in the hand don't | done | test `test_resource_piles` |
| Food and wood poured at a point go to the stores and piles in its cell, then the cells around, as far as each reaches | done | `GameMagicWorld::AddResource`, test `test_resource_piles` |
| What nothing takes makes a new pile, unless it lands in water | done | same |
| A new pile makes the nearby people come and look | done | same |
| Pots and piles placed by the land scripts | done | `CreatePot` (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A town's temporary pots placed by the land scripts | todo | stub |
| A pile or pot with something in it can catch fire | partial | `FireSystem.cpp` burns them; unconfirmed whether the food or wood is lost |
| Pots are physical: they can be thrown, roll and land | todo | see `../physics/` |
| An emptied pile goes (unconfirmed) | todo | |
| Hovering over a pile shows how much it holds | todo | see `../interface/` |
