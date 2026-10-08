# Wild animals

The animals that live wild on the lands: hunters (lions, tigers, leopards, wolves) and grazers that aren't kept by towns
(zebras, tortoises, goats in the wild). Shared behaviour is in [animal behaviour](animal_behaviour.md).

**Progress: 1/19 done, 9 partial — 29%**

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place wild animals, alone or in flocks | todo | `CreateAnimal`, `CreateNewAnimal`, `CreateFlock` are stubs |
| Each kind has its own model, size by age, speeds and table values (sight, hunting, domain, flock size) | partial | the tables load (`GAnimalInfo`, `src/InfoConstants.h`); used only by the miracles' animals |
| Each kind plays its own clips: stand, move, eat, start and finish eating, sleep, in hand, thrown, landed, dying, dead, and for hunters stalk, pounce and hide | partial | dying and dead clips per kind (`DyingClip`, `DeadClip`), the miracles' wolves' run, leap and eat; goat and zebra dying clips undecoded |
| Animals live in a domain about where they were made, and keep to it | partial | the domain is kept for the miracles' flocks (`Flock::centre`, `domainRadius`) |
| Animals keep to the surroundings they like (land, coast; unconfirmed) | todo | |
| The chess puzzle's animals and villagers (Creature Isle) | n/a | Creature Isle only |

## Hunters

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hunters grow hungry each turn and hunt once hungry enough | partial | the miracles' wolves (`AnimalSystem::ProcessTurn`, `ReactToFoodNeeds`) |
| A hunter looks for prey in the cells spiralling out from its own, remembering where it last found some | partial | `FindPrey`; the miracles' wolves |
| It chases its prey, and leaps at it from close enough while facing it | partial | `Chase`, `Pounce`; test `test_animal_move` (AHunterMakesForTheNearSideOfItsPrey); the miracles' wolves |
| Prey brought down falls, is eaten over the turns, then dies | partial | `BringDown`, `ProcessEaten`; the miracles' wolves |
| A hunter that can't reach its prey (a villager indoors) gives it up | todo | openblack's villagers are never out of reach |
| A hunter stalks and hides before it pounces | todo | |
| Hunters have a lair near the forests, where they hide and sleep | partial | Tigers and wolves choose a lair from the land's forests on landing (`src/ECS/LandForests.*`, `cb278194`); hiding and sleeping there and the other hunters' lairs aren't ported |
| Lions frighten creatures | done | `IsFrighteningToCreature` |
| Other animals flee from hunters | todo | |
| Hunters attack villagers and livestock | partial | the miracles' wolves hunt villagers and animals |

## Grazers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grazers look for good grass, wander there and graze | todo | |
| Grazers drink at the land's drinking places | todo | the drinking places are made by the land scripts (stub) |
| Grazers flock together and follow their leader | todo | |
| Tortoises (unconfirmed what sets them apart) | todo | |
