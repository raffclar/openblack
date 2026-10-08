# How a computer god decides

Every rival god, in the story and in skirmish, is driven by the same mind: a fixed tree of needs, from six broad aims
down to single hand actions, weighed each turn against what the god can see, how it feels about each other player and
what it has been told by the land's scripts. The scripts can tune the weights, pause the mind, take over its hand and
queue actions of their own.

**Progress: 0/66 done, 0 partial — 0%**

openblack has no computer god mind yet: a computer player is only a player entity (`PlayerArchetype`) and every script
function that would steer one is a stub (see [challenge_scripts.md](challenge_scripts.md)).

## The tree of needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mind is a tree four levels deep: 6 broad aims, 19 needs under them, 54 smaller desires and 50 hand actions at the leaves | todo | the tree is the same for every god; only its weights differ |
| Each node has a weight, its personality, which multiplies how strongly it is felt; scripts set it by the node's name, from 0 to 1 | todo | |
| A name can belong to more than one node (defeating a player is both an aim and a need) and setting it sets every node of that name | todo | |
| Each node also has a suppression value that scripts can set by name | todo | |
| Each node measures how much it is wanted by finding the best object for it in the world (a town, a creature, a site) and how much that object needs it | todo | |
| A node's object is passed down, so the action at the leaf acts on the town or creature its parents chose | todo | |
| Each turn the mind processes the tree and performs the leaf action of the strongest branch (unconfirmed how ties and thresholds work) | todo | |
| A node already processed this turn isn't worked out again | todo | |
| The six aims are looked at on their own timers: reacting to players every 50, looking after its creature every 100, defeating players every 200, its towns every 300, other players and the world every 5000 (unconfirmed units) | todo | |
| Distance matters: a desire's pull falls off with how far its object is (unconfirmed how) | todo | |

## The broad aims and their needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dealing with other players: giving them things and talking to them, each weighted a quarter | todo | |
| Looking after its creature (weight 0.4): feeding it, casting food for it, making it drink, healing it, sending it to sleep, sending it home (half weight) | todo | drinking, sleep, warming and cooling start at weight 0, so they never happen unless a script raises them |
| Teaching its creature (weight 0.5): ordinary skills, aggressive, compassionate and impressive miracles | todo | see [creatures.md](creatures.md) |
| Playing with its creature (weight a quarter): throwing food at it, and slapping, stroking, leashing it to things or casting on it for a laugh, all of which start at 0 | todo | |
| Its creature's emergencies (weight 0.95): starving, hurt or ill | todo | the ill branch has no action under it |
| Attending to its towns (0.7): the town that most wants food, the one that most wants wood, building sites needing workers and needing wood | todo | see [towns_and_influence.md](towns_and_influence.md) |
| Expanding its towns (0.7): combining scaffolds, finding where to build with one, and filling the workshop with wood | todo | |
| Attending to its worshippers (0.7): feeding them, resting them, sending villagers to a site that needs them, charging up miracles | todo | |
| Growing its towns' population (0.7): making breeder disciples | todo | |
| Expanding its influence (0.75): winning towns by impressing them or by meeting their food needs | todo | the tree lists the food branch twice and never the wood branch, so it never wins towns with wood this way |
| Defeating a player (0.1 each): the player in general, the one with more miracles, the one with a better creature, the one with more worshippers | todo | |
| Reacting to players (1): to an aggressive creature, to miracles cast at it, to things thrown at it, and defending its towns | todo | |
| Checking its towns (1): for attacks on their belief, for damage and fire, and for buildings left unprotected | todo | |
| The world aim has nothing under it | n/a | nothing to do |

## What each desire does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Feeding its creature: food from a field, from a fish farm, or a food miracle on it | todo | |
| Its starving creature: a food miracle, a field, a fish farm or another town's food | todo | |
| Its hurt creature: healing it with a miracle | todo | see [magic.md](magic.md) |
| A town short of food: a disciple, its own creature's help, a food miracle, a field, a fish farm, or food from another town for the storehouse | todo | |
| A town short of wood: a disciple, its creature's help, a wood miracle, a forest, or wood from another town | todo | |
| A building site short of workers: a disciple, its creature's help to build, or a free villager from the town | todo | |
| A building site short of wood: a wood miracle, a forest or another town's wood | todo | |
| Winning a town: its creature impressing it, throwing things to impress it, impressive miracles, or miracles that meet its needs | todo | |
| Destroying a town: with a miracle, with its creature or with a thrown object | todo | |
| Attacking a creature: with miracles, with its own creature, or by throwing a rock | todo | |
| A player casting miracles at it: using its creature as a diversion | todo | |
| A player throwing things at it: a physical shield, or its creature as a diversion | todo | |
| Defending a town: with its creature or with miracles | todo | |
| A burning building: a water miracle; a damaged one: its creature's help; an unprotected one: a shield | todo | |
| Its worshippers hungry: a food miracle on the worship site, a field, a fish farm or another town's food | todo | |
| Its worshippers tired: heal at the worship site, or taking villagers home from it | todo | |
| A worship site short of villagers: raising the town's totem, or taking villagers from the town to it | todo | |
| A belief attack on its town: a miracle to defend against it, or its creature impressing the town | todo | |
| Throwing something at a player | todo | |
| Nice and nasty messages, taunts, scares and gifts to a player are in the tree but were never made | n/a | they do nothing in the game |

## How it feels about other players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god keeps an attitude to each of the eight players | todo | |
| Scripts set it on a scale where the stored value is twice the one given, and read it back halved | todo | e.g. Nemesis's 2 is kept as 4 |
| It judges how much aggressive magic each player throws at it, from the miracles it has seen cast | todo | see [magic.md](magic.md) |
| It judges how much each player throws at it by hand | todo | |
| It judges which player is worth defeating by their miracles, their worshippers and their creature | todo | |
| Gods can be allies, with an alliance strength scripts set from 0 to 1 | todo | `SET_PLAYER_ALLY` |
| Allies may use each other's influence | todo | see [../worship/influence.md](../worship/influence.md) |

## What it has seen and learnt

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps a list of things it has seen, cleaned of things that no longer exist | todo | |
| Many of its searches cheat: it can find any field, abode, tree, town, villager, creature, player or miracle on the land without having seen it | todo | |
| It watches what other gods do and keeps it to copy later ("look and learn"; unconfirmed what it copies) | todo | |
| It checks its remembered lessons and miracles still exist each turn | todo | |

## Its queue of actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An action becomes a state of the hand made of steps (pick up, carry, drop, cast) with their arguments | todo | |
| The god is busy until the action's steps finish, and ready again after | todo | `COMPUTER_PLAYER_READY` |
| Scripts can force an action at once, on given objects, ahead of its own choice | todo | |
| Scripts can queue actions to follow one another, and clear the queue | todo | |
| A queued action whose object has gone is dropped | todo | |
| The mind can be paused, and switched off for good when a god is beaten | todo | |
| A script that moves the hand takes it over until the script releases it | todo | |

## Its hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It acts through a hand of its own that flies over the land at its speed (scripts use 8 to 4000) | todo | only the person's hand exists (`HandSystem`) |
| Its hand stops when it would leave its influence | todo | |
| It casts miracles from gestures or from its temple's icons | todo | see [magic.md](magic.md) |
| The camera can follow its hand or look at it | todo | see [../camera/](../camera/) |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god's mind, weights, queue, what it has seen, its attitudes and the miracles cast at it are saved and loaded with the game | todo | see [../engine/](../engine/) |
| Scripts can save a god's weights and load them back (unconfirmed where they are kept) | todo | |
