# Forests

Groups of trees the land scripts set out as forests, and the big forest models. A forest grows new trees, gives its town
wood, has footpaths and is a place for creatures and animals.

**Progress: 4/16 done, 2 partial — 31%**

## Forests of trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make forests and put trees in them by number | done | CREATE_FOREST makes a land forest at the script's position and CREATE_TREE joins the first forest with its number (`src/ECS/LandForests.*`, `LandForest` component, test `test_land_forests`, `cb278194`) |
| A forest grows new young trees near its trees over time | todo | only the water miracle adds trees (`ForestSystem::AddTreeNear`) |
| A forest gains a tree no sooner than so many turns after the last any forest gained (41), at the first free spot round the tree | done | `ForestSystem::AddTreeNear` |
| A forest's trees die back now and then (unconfirmed when) | todo | |
| A forest belongs to the towns near it; a town's foresters work it, and it is removed from them when it's gone | partial | Towns list the forests within reach of their storage pit that hold wood (`land_forests`, `cb278194`); open: the temporary wood pile for a town without a pit, rebuilding the list when features or buildings change, foresters |
| A forest's wood is the sum of its trees' | todo | see `../resources/wood.md` |
| Villagers' footpaths lead into forests | todo | see `../terrain/` |
| Forests are places for the creature to go, play and vent its anger | todo | see `../creature/` |
| Wild animals choose lairs near forests | partial | Tigers and wolves pick their lairs from the land's forests as the game scores them (`cb278194`); other hunters' lairs, see ../animal/wild_animals.md |
| The hand over a forest shows help and makes its own sound | todo | see `../interface/` |
| Scripts read and change forests | todo | |

## Big forests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place big forest models, turned and scaled | done | `BigForestArchetype` |
| Big forests are lit as the game lights them | done | port notes (big forests unlit) |
| The creature and villagers walk round big forests | todo | see `../creature/` |
| The hand tugging a big forest pulls a tree out of it | todo | needs the hand to hold objects |
| A big forest gives wood (unconfirmed how much) | todo | |

The forest miracle's forests are in `../miracles/forest.md`.
