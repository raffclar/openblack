# Town desires

Each town works out, every game turn, how much it wants each of seventeen things: food, wood, playtime, protection,
mercy, abodes, civic buildings, worship supplies, children, building, rain, sun, repairs, workshop supplies, a wonder,
relaxation and sleep. Idle villagers serve the most wanted. The storage pit shows the town's wants as flags.

**Progress: 15/26 done, 6 partial — 69%**

## Working out the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town counts its adults, children, abodes, room, civic buildings, disciples, food and wood each turn | partial | `src/ECS/Systems/Implementations/TownDesireSystem.cpp`; food and wood carried and at building sites are always none, as nothing carries or builds |
| Food wanted: what the town's people need for dinner against what it has | done | `src/ECS/TownDesire.cpp`; test `test_town_desire.cpp` |
| Wood wanted grows with the town's buildings past a number | done | `TownDesire.cpp`; craftsmen raise it by their share of the town's adults (counted in the same formula); craftsmen: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Playtime wanted only after the game has run a while | done | `TownDesire.cpp` |
| Protection and mercy wanted from the town's memory of attacks | done | `TownDesire.cpp`, `src/ECS/TownAggression.cpp` |
| Abodes wanted for the people without room | done | `TownDesire.cpp` |
| Civic buildings wanted by population against the civic buildings it has | done | `TownDesire.cpp`; a football pitch is wanted only with football on and no pitch built or being built: [football.md](football.md) |
| Children wanted, less as food is wanted more | done | `TownDesire.cpp` |
| Building wanted from the town's building sites | partial | `TownDesire.cpp`; there are never any building sites |
| Repairs wanted for damaged abodes not empty | done | `TownDesire.cpp` |
| Worship supplies, workshop supplies, rain and sun are never wanted, as in the game | done | `TownDesire.cpp` |
| A wonder wanted by the town's belief, less while food, wood and abodes are wanted | done | `TownDesire.cpp` |
| Relaxation comes up in the evening; sleep comes up at nightfall | done | `TownDesire.cpp`, by the day clock |
| Each desire is scaled by the town's tribe | done | `TownDesire.cpp` |
| What villagers are already doing for a desire lowers it | partial | `TownDesire.cpp` takes it off, but only sleep is ever done |
| The desires are sorted most-wanted first, both as felt and after what is being done | done | `TownDesire.cpp` (the game's own unstable sort) |
| Every 50 turns the town works out how unhappy its people are | done | `TownDesire.cpp` |
| The guidance read the town's biggest need | partial | the query exists (`TownDesire.h`); see ../story/ for the advisors |

## Acting on the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An idle villager offers itself to the desires in order until one takes it | done | `src/ECS/Systems/Implementations/TownDesireSystem.cpp` |
| Each desire's job: fetch food, fetch wood, build abodes and civic buildings, build, repair, supply, play, relax, sleep | partial | only sleep (`src/ECS/Systems/Implementations/VillagerHome.cpp`); see ../villager/daily_routine.md |
| Scripts boost a desire, and can resort the order at once | partial | the boost is in `TownDesireSystem` but TOWN_DESIRE_BOOST in `src/LHScriptX/FeatureScriptCommands.cpp` is empty |
| A town's needs shown at a place a script picks | todo | TOWN_NEEDS_POS is not implemented |

## Desire flags

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit flies a flag for each thing the town wants, rising with how much it wants it | todo | |
| Each flag has a tooltip naming the desire | todo | See ../interface/ |
| The flags ripple in the wind | todo | |
| Hovering a flag gives help about what the town wants | todo | |
