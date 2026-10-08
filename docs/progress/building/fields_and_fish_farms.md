# Fields and fish farms

Fields and fish farms are the town's food-producing places. A field is a patch of crop the town's farmers sow and
harvest; a fish farm is a stretch of water by the shore where fishermen fish. This file covers them as places; the crop
and the food they give are in ../resources/.

**Progress: 5/15 done, 2 partial — 40%**

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place a town's fields of each type, turned as given | done | CREATE_NEW_TOWN_FIELD and friends (`src/LHScriptX/FeatureScriptCommands.cpp`), `src/ECS/Archetypes/FieldArchetype.cpp` |
| A field without a town joins the nearest | partial | `FieldArchetype.cpp` (unconfirmed it matches the game's choice) |
| The crop grows on its own turn in every ten, by alignment, sun and rain | done | `src/ECS/Systems/Implementations/FieldSystem.cpp`; test `test_field_crop.cpp`; see ../resources/ |
| The crop's look changes with ripeness, sways once ripe and eases to its height | done | `FieldSystem.cpp`, `src/3D/FieldCrop.h` |
| Fields start sown; the game has farmers sow them | partial | See ../villager/jobs.md |
| A field knows its farmers and how much it wants farming | todo | |
| A field burns its food rather than its life | done | `src/ECS/WorldObjects.cpp` |
| A field destroyed by an effect loses its crop and its fire | done | `WorldObjects.cpp` |
| Water sprinkled on a field by the water miracle helps it grow | todo | See ../miracles/ |
| The creature can eat or examine a field, and stamp on it | todo | See ../creature/ |
| Crop sheaves can be picked up from a field by hand | todo | See ../hand/ |

## Fish farms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place fish farms for towns | todo | CREATE_TOWN_FISH_FARM is empty (used 13 times in the first land alone) |
| A fish farm shows a shoal of fish when there is sea round it | todo | |
| Fishermen fish at the farm's spots, taking its fish | todo | See ../villager/jobs.md |
| The farm's fish come back over time | todo | See ../resources/ |
