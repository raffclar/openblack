# The wonder and tribal powers

Each tribe can build a wonder, a huge building raised from scaffolds and wood like any other. A finished, working wonder adds
its tribe's power (from the tribe table) to its owner's power for that tribe. Each miracle's table says which tribes'
powers make it stronger or cheaper; the Indian power also speeds up villagers and the Aztec power raises worship.

**Progress: 6/12 done, 1 partial — 54%**

## The wonder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wonders of every tribe are placed by the land scripts as buildings, sitting into the land | done | `src/ECS/Archetypes/AbodeArchetype.cpp` (morphs with the terrain) |
| The town wants a wonder by its belief, once food, wood and abodes are seen to | done | `src/ECS/TownDesire.cpp`; test `test_town_desire.cpp` |
| A wonder is built from a big scaffold made at the workshop and much wood | todo | See ../building/construction.md and [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) (seven joined scaffolds; the wonder takes the tribe of the scaffold's home town) |
| A finished, working wonder adds its tribe's power to its owner's power for that tribe | todo | |
| A destroyed or lost wonder takes the power away again | todo | |
| A wonder's power level is set as the wonder is built and kept | todo | |
| Villagers can shelter in a wonder | todo | |

## Tribal powers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player keeps a power for each tribe, 1 until a wonder raises it | partial | kept on the magic system (`MagicSystem::SetTribalPower`), only the testbed sets it |
| Indian power: villagers move faster | done | `src/ECS/VillagerSpeed.cpp` |
| Aztec power: worship sites chant more prayer power | done | `src/Magic/WorshipBattery.cpp` (every site uses the Aztec multiplier) |
| A miracle is made stronger or cheaper by the product of its caster's powers for the tribes its table marks | done | See ../miracles/ |
| Forest miracle's wood worth more by tribal power | done | `src/ECS/Systems/ForestSystemInterface.h` |
