# Birds

The flying animals: crows, doves, swallows, pigeons, seagulls, bats and vultures, flying in flocks over the land, and
the doves or bats that circle each temple. The flock miracle's doves and bats are in `../miracles/`.

**Progress: 4/19 done, 8 partial — 42%**

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make flocks of birds and the birds in them | todo | `CreateFlock`, `CreateAnimal` and `CreateNewAnimal` are stubs in `src/LHScriptX/FeatureScriptCommands.cpp`; birds can be made only by the miracles and the testbed |
| Each kind has its own models, clips (fly, stand, eat, sleep, thrown, dead) and table values | partial | the tables load (`GAnimalInfo`); the miracles' doves and bats play their flapping clips (`AnimalSystem.cpp`) |
| A bird is born at a random age and sized for it | done | `BirthScale` (`src/Animals/AnimalRules.cpp`) |
| Doves circle a good player's temple, bats an evil one's, changing as the alignment changes (unconfirmed exact rule) | todo | the temple's dove and bat kinds are in the tables only |
| Seagulls keep to the coast (unconfirmed) | todo | |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flock's leader flies legs about where the flock lives, between its kind's inner radius and the flock's reach | partial | `AnimalSystem::StartWander`, run for the miracles' flocks only |
| Followers fly to points near the leader, then keep a formation behind it by their place in the flock | partial | `FollowFlock`, `FormationSlotOf`, test `test_animal_move`; miracles' flocks only |
| Birds keep a height above the land within their kind's band, climbing slowly | partial | test `test_animal_move` (BirdsClimbSlowlyAndKeepOffTheLand); miracles' flocks only |
| Birds turn by at most their kind's turn angle and bank into turns | partial | `AnimalMove`, `Zoomer`, tests `test_animal_move`; miracles' flocks only |
| Birds land to eat and to sleep, on the ground or on objects, and take off again | todo | |
| Flocks of the same kind can merge, and a small flock joins a larger one | todo | |
| Birds sleep at night | todo | |

## Birds and the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bats and vultures frighten creatures | done | `AnimalSystem::IsFrighteningToCreature` |
| Birds look at and flee from miracles and the hand (unconfirmed which kinds) | todo | |
| A bird killed falls out of the sky | done | Dying birds start in the physics with their flight speed and the game's spin (`AnimalSystem`) |
| A dead bird lies its time, then goes in a puff of grey smoke | partial | it lies its time and goes; no smoke yet |
| Tornadoes carry birds off | done | `AnimalSystem::ProcessTurn`, `TornadoSystem` |
| The hand can pick up birds the table allows, and throw them | partial | Animals the table allows are picked up and thrown (`HandGrabSystem`, `DynamicsSystem::LetGoFromHand`); birds' own thrown and landed clips aren't played |
| The creature watches birds, and may catch and eat them (unconfirmed) | partial | the creature's eye is caught by animals (`CreatureMindSystem.cpp`); see `../creature/` |
