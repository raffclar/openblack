# Abodes

Abodes are the homes of a town: each tribe has its own set, from tents and huts to shacks and large houses, and the
land scripts place them with food and wood in them. Villagers live in them with their families, and they light up and
smoke at night while people are in.

**Progress: 11/26 done, 3 partial — 48%**

## Placing and looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tribe has its own abodes (several sizes of home) and civic buildings, found by name | done | `src/Enums.h`, `src/InfoConstants.h` |
| The land script places an abode in a town with its angle, size, food and wood | done | CREATE_ABODE in `src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AbodeArchetype.cpp` |
| An abode given an unknown town joins the nearest town | done | `AbodeArchetype.cpp` |
| Abodes sit into the slope of the land; large civic buildings bend with it | partial | `AbodeArchetype.cpp` marks which bend; the game sinks the foundations to the lowest corner, which is not done |
| Abodes block the way of walkers with a round footprint | done | `AbodeArchetype.cpp` (Fixed bounding circle) |
| Windows light up at night while someone is home | done | `src/ECS/Systems/Implementations/RenderingSystem.cpp`; see ../sky/ for night |
| Chimneys smoke while someone is home, blown by the wind and the hand | done | `src/ECS/Systems/Implementations/ChimneySmokeSystem.cpp`; test `test_chimney_smoke.cpp` |
| Abodes and lanterns light the ground round the town at night | done | `src/ECS/Systems/Implementations/VillageLightSystem.cpp`; test `test_village_lights.cpp` |
| Street lanterns of the town and the country, flickering and crackling | done | `src/ECS/Archetypes/StreetLanternArchetype.cpp`, `VillageLightArchetype.cpp` |
| Snow settles on roofs in cold lands | done | See ../weather/ |
| Abodes cast a shadow at night by lantern light | todo | |
| A full abode shows how full it is (food and people) when the hand is over it | todo | See ../interface/ |
| Abodes are highlighted under the hand | todo | See ../hand/ |
| Footpaths go round abodes | todo | See ../terrain/ |
| Low-detail versions of buildings far away | todo | See ../rendering/ |

## Living in them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An abode holds a set number of adults and children | partial | `src/ECS/Systems/Implementations/TownSystem.cpp` counts adults only, and scripted villagers are not added |
| Villagers go in by the door and are counted as present | done | `src/ECS/Systems/Implementations/VillagerHome.cpp` |
| Abodes keep food for dinner and wood | partial | kept on `src/ECS/Components/Abode.h`, unused |
| The abode works out the food its family needs for dinner | todo | |
| An abode knows its nearest drinking water | todo | |
| Tapping an abode's roof calls its people out or sends them in | todo | |
| A disciple dropped by an abode moves into it | todo | See ../villager/disciples.md |
| Villagers can hide in buildings in an emergency | todo | See ../town/emergencies_and_aggression.md |
| An abode is part of its player's influence through its town | done | See ../worship/ |
| Food and wood can be given to an abode by hand | todo | See ../resources/ |
| The creature can look at, kick or stomp on abodes, and learns from it | todo | See ../creature/ |
