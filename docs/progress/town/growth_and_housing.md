# Town growth and housing

A town is a collection of abodes and civic buildings around a town centre, with an area on the map. It houses its
people, plans new buildings as it grows, and grows outwards when the player gives it wood and scaffolds.

**Progress: 3/24 done, 3 partial — 19%**

## The town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Towns are made by the land script with an owner and a tribe | done | `src/ECS/Archetypes/TownArchetype.cpp`, CREATE_TOWN in `src/LHScriptX/FeatureScriptCommands.cpp` |
| A town has an area worked out from its buildings, and a centre | todo | |
| A town can be marked uninhabitable by script | todo | SET_TOWN_UNINHABITABLE is not implemented |
| A town has a congregation point, which scripts can move | todo | SET_TOWN_CONGREGATION_POS is not implemented |
| The town's own forests, fields, fish farms and flocks are assigned to it at load | todo | |
| A town knows its nearest towns | todo | |
| Each town has its own influence | done | See ../worship/ |
| A town turn runs its steps in the game's order: needs, abodes, repairs, artefacts, spell icons, emergencies, dying | partial | only desires and aggression each turn (`src/ECS/Systems/Implementations/TownDesireSystem.cpp`) |

## Housing its people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers are housed in abodes with room for adults and for children | partial | `src/ECS/Systems/Implementations/TownSystem.cpp` by count only, and the abode's list isn't filled for scripted villagers |
| The best abode for a new villager is scored by room and by who already lives there (wanting a man, a villager) | todo | |
| The town keeps a list of its homeless, newest first | partial | `src/ECS/Components/Town.h` keeps it; nothing adds to it outside the testbed |
| Villagers are shuffled between abodes to fit families | todo | |
| A town checks whether its villagers need a new abode | todo | |
| Overcrowded abodes push villagers out | todo | |
| The town's population counts for its desires and influence | done | `TownDesireSystem.cpp`, `src/ECS/Systems/Implementations/InfluenceSystem.cpp` |

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town keeps planned buildings: outlines where it means to build | todo | CREATE_PLANNED_ABODE is not implemented |
| When it wants abodes or civic buildings it picks the best plan and opens a building site | todo | See ../building/construction.md |
| It asks for a new abode when there is no plan | todo | |
| It looks for clear ground suited to a building in its area | todo | |
| A script forces a planned building to be built | todo | |
| A new building changes the town's area and stats and draws villagers' attention | todo | |
| Scaffolds dropped by the player in the town become its buildings | todo | See ../building/workshop_and_scaffolds.md |
| The town grows only as fast as its wood allows | todo | |
| Towns of the computer player grow and attack on their own | todo | See ../multiplayer/ (the rival gods' towns in the story are scripted; unconfirmed how much is AI) |
