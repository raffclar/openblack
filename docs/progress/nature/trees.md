# Trees

The single trees and bushes on every land: their kinds, how they grow, sway and burn, and what the hand, the creature and
the villagers do with them.

**Progress: 14/30 done, 6 partial — 57%**

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place trees of each kind (beech, birch, cedar, conifer, oak, olive, palm, pine, cypress, bushes, copses, hedges, burnt) and their variants, turned and sized | done | `TreeArchetype`, `src/LHScriptX/FeatureScriptCommands.cpp` |
| A tree has a size now and a largest size it grows to | done | `src/ECS/Components/Tree.h` |
| Scripts mark some trees as scenery, which villagers leave alone (unconfirmed what else it changes) | todo | the flag is read and dropped (`TreeArchetype::Create`) |
| Trees stand in the way of walkers, each as a circle on the ground | done | `Fixed` obstacle (`TreeArchetype.cpp`) |
| Trees are lit by the land's light and drawn unlit where the game does | done | port notes: tree brightness and unlit (`PROGRESS-bw-clean.md`), test `test_tree_brightness` |
| Trees cast shadows, including at night (unconfirmed) | todo | see `../rendering/` |

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree short of its full size grows by its kind's amount every so many turns, the first wait a random part of it | partial | `VegetationSystem::ProcessTurn`, `src/3D/TreeGrowth.h`; no test or audit checks the rates |
| Trees grow faster in the rain and on good land | partial | same |
| The water miracle makes trees grow faster | done | `MagicSystem.cpp` (water drops on trees), miracle audit |
| The water miracle plants a young tree of the same kind by a full-grown one of a forest | done | `ForestSystem::AddTreeNear`; see [forests](forests.md) |
| Each kind can seed only so many new trees (unconfirmed) | todo | the table's limit is unused |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees sway in the wind, in one of sixteen sways chosen by the way they face | done | `VegetationSystem`, `Swayable` |
| Trees bend away from the hand passing among them, and creatures bend small trees as they walk through | done | `VegetationSystem::GetBend` |
| Bent trees crash about, louder the further they bend; tall trees near the camera rustle and creak now and then | done | `src/ECS/Systems/TreeRustle.h` |
| Snow settles on trees | done | port notes (snow cover); see `../weather/` |

## Fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees catch fire and burn, their flames kept to the trunk's middle | done | `FireSystem.cpp` (tree flame shape) |
| A burning tree is drawn burning | done | tree instance data's burning flag (`RenderingSystem.cpp`) |
| Fire spreads from tree to tree, each checking its neighbours now and then, with a random chance | partial | fire spreads by heat (`FireSystem.cpp`); the tree table's own spread checks are unused; see `../physics/` |
| A tree burnt down leaves a burnt tree (unconfirmed) or goes | partial | it goes |
| A magic tree on fire stops being something people come to look at | done | `FireSystem.cpp` |

## Hand, creature and villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand pulls up a tree; held, it shows as branches or logs by its kind | partial | The pull and uprooting are done (`HandGrabSystem`, `hand_grab::PullAt`, see `../hand/tug.md`); the held tree is drawn as itself, not as branches or logs; a firefly hiding in the tree leaves a one-shot seed in its place ([fireflies.md](fireflies.md)) |
| A tree dropped on the land is planted where it falls; planting and pulling up move the alignment | todo | the alignment amount is in the table |
| A thrown tree flies and lands as a dead tree lying as it came down | done | `object_physics::EndTree` → dead tree lying as it came down; see `../physics/throwing_and_landing.md` |
| Uprooted trees lie as dead trees, with their roots showing, and can be picked up and burnt | partial | dead trees are placed by the land scripts (`DeadTreeArchetype`), without their tilt |
| The creature pulls up, eats, throws and plays with trees, and must be big enough for a tree | todo | see `../creature/` |
| Foresters fell trees for wood; trees are worth wood by kind and size | todo | see `../resources/wood.md` |
| Villagers sleep under trees and shelter by them (unconfirmed) | todo | |
| Footprints are worn round trees on busy routes (unconfirmed) | todo | |
| Trees on a building site block the town's clearing | todo | see `../building/` |
| Tooltips over trees say what the hand can do | todo | see `../interface/` |
