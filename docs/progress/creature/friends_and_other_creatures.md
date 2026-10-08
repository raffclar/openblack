# Friends and other creatures

When creatures meet they size each other up. Each keeps a feeling for every other creature it knows, and from that
they make friends, play together, teach each other, sulk or quarrel, and go looking for a friend they miss. Fights
between creatures are in [fighting.md](fighting.md).

**Progress: 8/56 done, 1 partial — 15%**

## Noticing and judging other creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Another creature is among the most interesting things a creature looks at | done | `src/Creature/CreatureLook.*` (interest by kind); test `InterestByKind` |
| A creature keeps a feeling, from nasty to nice, for each other creature it knows | done | `creature_learning::AttitudeTo` in the creature's mind model |
| It judges another creature by whose it is, its species and whether it is stronger | partial | the decision trees test these ([beliefs_and_opinions.md](beliefs_and_opinions.md)); the creature-specific belief record is todo |
| How much more life and strength an opponent has, and how many dangerous miracles it knows, weigh on whether to take it on | todo | |
| Whether another creature seems friendly | todo | |
| Another creature can be frightening, and it runs from it | done | frightening things include other creatures (`CreaturePlanActions` target "frightening") |
| Inspect another creature: walks up and looks it over | todo | |
| Look at another creature with a friend | todo | |
| A creature only reacts to another again after a while | todo | see [reactions.md](reactions.md) |
| Being led together on a leash makes two creatures warmer (compassion leash) or cooler (aggression leash) to each other | done | `LeashSystem`, `AttitudeChange`; test `LeashedCreaturesWarmOrCoolToEachOther` |
| Scripts can force two creatures to be friends | todo | `CREATURE_FORCE_FRIENDS` is a stub in `CHLApi.cpp` |

## Asking and answering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature asks another to be friends; the other decides whether to accept | todo | |
| A creature asks another to play; the other may refuse | todo | |
| A creature asks another to fight; the other may refuse | todo | fights start without asking (see [fighting.md](fighting.md)) |
| A creature asks a friend to teach it; the friend may oblige | todo | |
| A creature busy elsewhere can't join in, and the asker waits for it to be free | todo | |
| The player is told when the other creature refused to fight, play or be friends | todo | see [lessons_and_help.md](lessons_and_help.md) |
| A friend doing something worth following draws the creature to join in | todo | |

## Friendly actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smile at a friend | done | plan action "smile at friend" (walks up and smiles) |
| Wave at a friend | done | plan action "wave at friend" |
| Kiss a friend | todo | |
| Follow a friend around | todo | |
| Dance with a friend | todo | |
| Run a race with a friend | todo | |
| Throw stones into the sea with a friend; play throwing games; throw stones at a can | todo | |
| Sit on top of a hill with a friend | todo | |
| Eat, drink, poo, sit and sleep with a friend | todo | |
| Eat from a field or eat fish with a friend, or get the friend to fetch it | todo | |
| Be happy with a friend | todo | |
| Go to the beach with a friend | todo | |
| Show a friend a miracle: fireball, lightning, storm, food, wood | todo | |
| Show a friend an object, its home or its citadel | todo | |
| Give a friend food or a toy | todo | |
| Teach a friend: the friend learns the actions and miracles it is shown | todo | see [learning_by_observation.md](learning_by_observation.md) |
| Talk to a friend, tell it a joke, howl at it | todo | |
| Get a friend's attention | todo | |
| Help a town with a friend | todo | |
| Cast an amusing miracle on another creature: freeze, small, big, weak, strong, fat, thin, invisible, nice, angry, hungry, frightened, tired, ill, thirsty, itchy | done | plan actions "cast make creature …"; see [creature_casting.md](creature_casting.md) and [../miracles](../miracles/) |
| Cast warming or cooling on another creature, cure its illness, give it food | todo | |

## Unfriendly actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Argue with a friend | todo | |
| Mope about with a friend | todo | |
| Confuse a friend | todo | |
| Show off to a friend | todo | |
| Kiss a friend's backside | todo | |
| Order a friend around, and the friend obeys | todo | |
| Tell another creature to go away | todo | |
| Show another creature it hates it | todo | |
| Attack a town with a friend | todo | |
| Pick a fight with a creature it is angry with | done | see [fighting.md](fighting.md) |

## Missing a friend

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pine for a friend that is gone | todo | the wish to see a missed friend is a desire with no actions yet ([desires.md](desires.md)) |
| Look for a missing friend | todo | |

## Minds and control

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Obey another creature: wants to do what a creature it looks up to tells it | todo | the desire exists, nothing feeds it ([desires.md](desires.md)) |
| Swap minds with another creature: they act on each other's learning | todo | `SWAP_CREATURE` is a stub |
| A computer player's creature takes on another god's creature, by the computer's choice | todo | see [../multiplayer](../multiplayer/) |
| The dominant creature of a pair leads, the other follows | todo | |
| The opposing creature in a challenge is set by script | todo | `OPPOSING_CREATURE` is a stub |
