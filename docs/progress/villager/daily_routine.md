# Villager daily routine

What a villager does when nobody is directing it: it offers itself to what its town wants most, sees to its own hunger
and tiredness, goes home at night and sleeps, and fills idle time by sitting out or wandering.

**Progress: 11/38 done, 7 partial — 38%**

## Deciding what to do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every villager runs a state machine, one state a turn, with a state it is heading for | partial | `src/ECS/Systems/Implementations/LivingActionSystem.cpp`; 28 of the 255 states have functions, the rest log "unimplemented" |
| Entering and leaving states runs their entry and exit rules and transition animations | partial | entry/exit is wired for fire, home and reactions; transition animations are not played |
| An idle villager decides what to do: what its town wants most that it can serve, past its own trigger | done | `src/ECS/Systems/Implementations/VillagerHome.cpp`, `src/ECS/Systems/Implementations/TownDesireSystem.cpp`; test `test_town_desire.cpp` |
| Children can serve only some desires | done | `src/ECS/TownDesire.cpp` (child flag per desire) |
| A villager's own hunger and life press it before the town's wants | todo | `VillagerHome.cpp` is a stub: only children get a fixed trigger |
| The town's desires a villager can take up: food, wood, abodes, civic buildings, build, repair, workshop, worship supplies, playtime, relaxation, sleep | partial | only sleep is acted on (`VillagerHome.cpp`); the others return "not taken" |
| Villagers pause between decisions | partial | a fixed cool-down stands in for the idle animations the game plays (`VillagerHome.cpp`, TODO #863) |
| The moving speed of each state, slowed by loads and wounds, quickened by needs, belief, magic food and emergencies | done | `src/ECS/VillagerSpeed.cpp`, applied in `VillagerReactions.cpp`; test `test_villager_speed.cpp` |
| Villagers walk round buildings and obstacles to their goal | partial | `src/ECS/Systems/Implementations/PathfindingSystem.cpp` (wall hugging); footpaths are not followed |
| Villagers keep to footpaths between buildings | todo | See ../terrain/ |

## Home and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the town wants sleep, and villagers go home | done | `src/ECS/TownDesire.cpp` (sleep ramp by time of day), `VillagerHome.cpp` |
| Villagers walk to their abode's door and go in, and are not drawn while inside | done | `VillagerHome.cpp`, `src/ECS/Components/AtHome.h` |
| Inside, villagers go to bed and sleep while the town wants sleep, longer when hurt | done | `VillagerHome.cpp` |
| Villagers wake in the morning and come out | done | `VillagerHome.cpp` (waking uses the going-home path) |
| Windows light up and chimneys smoke while someone is in | done | See ../building/abodes.md |
| Now and then an idle villager just goes home | done | `VillagerHome.cpp` |
| Home checks: hunger at home, illness at home, needs at home | todo | |
| A villager whose home is destroyed becomes homeless | partial | destroyed abodes take their villagers out (`src/ECS/WorldObjects.cpp`), but the villager is not put on the town's homeless list |
| Homeless villagers move into an abode with room when one is built | todo | the homeless list exists (`src/ECS/Components/Town.h`) but nothing reads it |
| Homeless villagers eat their dinner outside and sleep in a tent by a tree | todo | |
| Vagrants with no town wander until they find one to join | todo | |
| Villagers move house when their abode is too crowded or another suits better | todo | |
| A villager tapped on its abode goes home or comes out | todo | |

## Food and dinner

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry over time | todo | `Villager::hunger` is set at creation and never changes |
| Hungry villagers go and eat: at home, from the storage pit, or outside | todo | |
| Housewives fetch food from the storage pit, make dinner, serve it and clear it away | todo | See jobs.md |
| Villagers sit down to dinner at home, waiting for it when the housewife is late | todo | |
| A meal takes the villager's meal amount of food from the abode | todo | |
| Hungry adults walk slower | done | `src/ECS/VillagerSpeed.cpp` (reads hunger, which never changes yet) |
| Starving villagers lose life and lie weak on the ground | todo | |
| Food poisoned by the player harms those who eat it | todo | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md); Lethys poisons a village's food in [The Plague](../story/silver_scrolls/the_plague.md) |
| Magic food speeds up those who eat it for a while | partial | the speed rule is in `VillagerSpeed.cpp`; eating never sets it |
| Villagers pick up food they find and are interested in | todo | |

## Idle time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing to do, villagers wander near their town | done | `VillagerHome.cpp` |
| They sit and chill out outside their home or about the town in the evening | todo | the game does this instead of wandering; TODO in `VillagerHome.cpp` |
| Villagers notice interesting things nearby (animals, trees, rocks, pots, the ball, fields, abodes, other villagers) and interact | todo | |
| Villagers pause for a second now and then | todo | |
| Villagers yawn, stretch and play idle animations | todo | villagers play no animations yet |
