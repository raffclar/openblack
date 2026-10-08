# Animal behaviour

What all animals share: their needs, how they move and react, how they are drawn, and how they die.

**Progress: 10/31 done, 5 partial — 40%**

## Needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals get hungry, thirsty and tired, and need to breed, each at its kind's rate | partial | only hunger, for the miracles' wolves (`AnimalSystem::ProcessTurn`) |
| Each need is met by looking for food, water, a place to sleep or a mate in the cells around | todo | |
| Animals eat what their kind eats: meat, plants, grass or any | todo | the food kinds are in the table (`FoodType`) |
| A starving animal dies | todo | |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal turns by at most its kind's turn angle, harder close to its goal, and steps its speed | done | `src/Animals/AnimalMove.cpp`, test `test_animal_move` |
| It goes only where it can reach without circling | done | test `test_animal_move` (WhatItCanReachWithoutCircling) |
| Its clip plays by the ground it covers while moving, else by the clock | done | `src/Animals/AnimalAnimation.cpp`, test `test_animal_animation` |
| It is drawn between its last two turns | done | `AnimalSystem.cpp` drawing |
| It looks round as it goes, within its view angle | todo | |
| It walks round obstacles and buildings | todo | see `../physics/` |
| Ground animals keep their feet on the land and cast a ground blob shadow | partial | they follow the land; their blob shadows wait for their models' points (port notes) |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals flee from fire, falling trees, flying objects and fights | todo | the reaction system has the reactions for villagers (`ReactionSystem.cpp`), not animals |
| Animals look at miracles and flee the frightening ones | todo | |
| Animals look at and go to food and wood the hand or a miracle put down | todo | |
| Animals react to being picked up and dropped by the hand | todo | |
| Animals react to the creature's gifts and to the creature itself | todo | |
| Animals react to a shield, a teleport and a magic tree | todo | |
| Animals look at a death and at someone fainting | todo | |
| Animals chase a ball (unconfirmed) | todo | |

## Hand, physics and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks up the animals the table allows; held, they play their in-hand clip | partial | Picked up by the species' rule (`HandGrabSystem`, `hand_grab::ValidForPlaceInHand`, test `HandGrab.AnimalsOnlyWhenTheirKindAllows`), leaving their flock; the in-hand clip isn't played |
| Thrown animals fly, land and play their landed clip, hurt by a hard landing | partial | Fly, land with their posture and heading, hurt by hard landings (`LandAnimal`, `HurtLiving`); the landed clip isn't played through before they decide |
| Animals drown in deep water | done | Sunk animals die and are gone (`PhysicsGameHooks::HasSunk`) |
| Hurting or helping an animal moves the player's alignment (kind or nasty) | done | `AlignmentWeight` (`src/Magic/AreaEffect.cpp`) for miracles |
| Scripts take control of an animal, play its clips and let it go | todo | |

## Death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal with no life left falls dying, playing its kind's dying clip once, then lies in its dead clip | done | `AnimalSystem::KillByEffect`, `ProcessDeath`, `DyingClip`/`DeadClip`; goat and zebra clips undecoded |
| A dead animal lies 600 turns and one more, then goes | done | `k_TurnsToDieOver` |
| Killed again, its body only lies its full time afresh | done | `KillByEffect` |
| It goes in a puff of grey smoke | todo | noted in `AnimalSystem::ProcessDeath` |
| Miracles hurt and heal animals as other living things | done | `MagicLiving.cpp` |
| Fire burns animals to death | partial | `FireSystem.cpp`; the fire audit noted animals not dying in some cases |
| A dead animal is food for hunters and the creature (unconfirmed) | todo | |
