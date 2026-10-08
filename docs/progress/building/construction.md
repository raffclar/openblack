# Construction

Towns grow by building. A town keeps plans of buildings it means to put up; a building site opens on a plan (or where
the player drops a scaffold), builders bring wood and build it up stroke by stroke, the scaffold rising round the
growing building until it is finished and becomes functional.

**Progress: 0/19 done, 3 partial — 8%**

## Plans and sites

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place planned abodes and planned citadels a town will build | partial | planned citadels are made (`src/ECS/Archetypes/CitadelArchetype.cpp`); CREATE_PLANNED_ABODE is not implemented |
| A plan shows as a ghostly outline of the building on the ground | todo | |
| A town picks the best plan for what it wants and opens a building site on it | todo | |
| A building site clears its ground and checks nothing fixed is in the way | todo | |
| A site needs a number of builders, set by its size | todo | |
| A site needs a set amount of wood, a pile of wood beside it filling as builders bring it | todo | |
| Builders stand round the site at spaced places and build | todo | See ../villager/jobs.md |
| Each stroke uses wood and raises the building's percentage built | todo | |
| The building shows partly built: the model cut at the height reached, with a scaffold round it | todo | |
| A finished building is built, functional, and villagers react to it | todo | |
| A building that's had its site removed or its town lost stops being built; its builders leave | todo | |
| Building sites with no use are pruned each turn | todo | |
| A script forces a planned building to be built at a place | todo | |
| A town's citadel building site (when the citadel is planned) | partial | See ../temple/ |

## Wood for building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Builders decide whether to fetch wood from the storage pit, a forest or a pot | todo | |
| The town counts wood at its building sites in its stores | partial | the count exists in `src/ECS/Components/TownDesire.h`, always none |
| Wood dropped by hand on a site is added to its pile | todo | See ../resources/ |
| The player's scaffolds count as most of a building's wood | todo | See workshop_and_scaffolds.md |
| Wood the town uses for building is taken from its store | todo | See ../town/storehouse.md |
