# Villager reactions

Villagers react to what happens around them: they flee frightening miracles and watch pleasing ones, gather round
the creature, fight fires, run from predators, point at flying objects, mourn the dead and are impressed, which gives
their town belief. Each reaction has a reach, an urgency and a time from the reaction table.

**Progress: 12/44 done, 6 partial — 34%**

## How reactions work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reaction spreads to the villagers in its reach, each taking it up by urgency and distance | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp`, `src/Magic/VillagerReactionRules.cpp`; test `test_reactions.cpp` |
| A villager remembers the last few kinds it reacted to and won't react to the same kind again too soon | done | `src/Magic/VillagerReactionRules.cpp` |
| A more urgent reaction of another kind takes over the current one | done | `ReactionSystem.cpp` |
| Only villagers able and alive, in a state that allows it, react | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp` |
| After reacting, the villager goes back to what it was doing | done | `VillagerReactions.cpp` |
| Impressive things give belief to the villager's town, with a belief symbol rising from it | done | `VillagerReactions.cpp`, `src/Magic/Impressiveness.cpp`; see ../town/belief_and_conversion.md |
| Awed villagers' voices are heard now and then near the hand | partial | `VillagerReactions.cpp` times them; see ../audio/ for the samples |

## Miracles and magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flee a frightening miracle, straight away or across its path, then turn to watch it from a distance | done | `VillagerReactions.cpp` |
| Turn to watch a nice or impressive miracle | done | `VillagerReactions.cpp` |
| Walk to a magic shield, stand amazed under it looking out while the town wants protection | partial | `src/ECS/Systems/Implementations/VillagerShieldShelter.cpp`, `MagicShieldVillagers.cpp`; plays no animations |
| React when a shield is struck or destroyed | partial | the reactions are spread (`ReactionSystem.cpp`) but have no villager state of their own |
| Walk (or run) into a teleport stone, jump and carry on | done | `src/ECS/Systems/Implementations/VillagerTeleport.cpp`; test `test_teleport_rules.cpp` |
| Be bewildered by a magic tree: turn to it and stare | todo | |
| Run to magic food or wood dropped nearby and take it | todo | states for food and wood reactions are unported |
| React to a fight won | todo | |

## Fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers near a blaze react; their own town's come to beat it out when needed, the rest go round it | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`; test `test_fire.cpp` |
| Firemen fetching water to put out fire give up at once, as the game does | done | `VillagerFire.cpp` |
| A villager set on fire runs about until it is out or dies | done | `VillagerFire.cpp` |
| Villagers run to their burning abode | todo | |
| A burning object in the hand alarms the villagers below | partial | Picking up a burning thing raises the reaction (`FireSystem::StartedMoving` from `HandGrabSystem`); `FireSystem` still treats held burning things as on the map |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers turn to face the creature, approach it and inspect it | todo | |
| They respect it, worship it, or flee it, by their town's attitude to the creature | todo | |
| A villager is scared stiff by a frightening creature | todo | |
| Villagers walk towards the creature when it does something interesting | todo | |
| A villager controlled by the creature (picked up or led) does as it is made to | todo | See ../creature/ |
| Villagers crowd round a creature fight and watch it | todo | |

## The hand and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Approach the hand when it is near | todo | |
| React to a villager in the hand | todo | |
| React to something dropped by the hand, or put in the storage pit | todo | |
| Watch and point at a flying object thrown overhead | partial | Point or run (`villager_physics`); see [../physics/thrown_living.md](../physics/thrown_living.md) |
| Get out of the way of a falling tree | todo | |
| Inspect an object new to them, and tell others about it | todo | |
| React to a newly built building and to a new scaffold | todo | |
| Run after the ball and pick it up | todo | See play_and_gossip.md |
| React to an object crushed nearby | partial | the reaction is spread (`src/ECS/WorldObjects.cpp`), villagers have no state for it |

## People and animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flee a predator, then look to see if it is safe | todo | the flee reaction is spread for animals (`AnimalSystem`), not for villagers |
| Go and hide in a nearby building when danger comes | todo | |
| Point at, go towards, look at and mourn a dead villager | todo | See death.md |
| Faint, panic and be confused at shocking sights | todo | |
| Crowd round to watch something | todo | |
| React to a breeder | todo | |
| Cheer the town's celebration when it is won over | todo | See ../town/belief_and_conversion.md |
| React to a missionary | todo | |
| Fight a villager of another town | todo | |
