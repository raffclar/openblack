# Villager tools and carried items

What a villager holds in its hand: the tool of the job it is doing (axe, fishing rod, crook, spade, hammer, saw or
mallet), the wood or food it is carrying, and the football. Tools are only drawn props; the only thing a villager ever
lets fall as a real object is a log of the wood it carries. The load itself (how much food and wood, the storage pit
trips, the slowdown) is in [jobs.md](jobs.md); a thrown villager's flight is in
[../physics/thrown_living.md](../physics/thrown_living.md).

openblack (`physics` branch): villagers hold no load and draw nothing in their hands. The carried-object list is in
`src/Enums.h` (`CarriedObject`) and the tool meshes load with the mesh pack (`src/3D/AllMeshes.h`), but nothing picks
or draws them. The physics calls a "drop what it carries" step when a villager is thrown or launched
(`PhysicsGameHooks::InitialisePhysics`, `DynamicsFromHand.cpp`), and that step is empty
(`PhysicsClassHooks::DropCarriedResource`).

**Progress: 0/67 done, 3 partial — 2%**

## The objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager can hold one of fourteen objects: axe, fishing rod, crook, saw, bag, ball, hammer, heavy mallet, scythe, spade, a plain piece of wood, and a log of evergreen, fruit tree or hardwood; or nothing | partial | The list is `CarriedObject` in `src/Enums.h`; nothing uses it |
| Each object is one mesh from the mesh pack, the same for every tribe: the "O_" objects of `Data/AllMeshes.h` (axe 342, bag 343, ball 344, evergreen branch 347, fruit branch 348, hardwood branch 349, crook 354, fishing rod 355, hammer 367, heavy mallet 378, saw 383, scythe 384, spade 390, wood in hand 406) | partial | The meshes load and are named in `src/3D/AllMeshes.h` (`ObjectAxe`, `ObjectWoodInHand`...); none is ever drawn in a hand |
| The pack's second set of tools (the "U_" axe, bag, ball, crook, fishing rod, hammer, mallet, saw, scythe, spade) is never held: info.dat uses them as furniture scenery | todo | Undetermined where the furniture is placed; nothing in the carrying code uses them |
| The logs, wheat and dead fish "in hand" meshes of the pack (372, 405, 370) are not villager objects either: a villager carrying food shows a bag, never fish or wheat | todo | |

## Which tool each job uses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters hold an axe from the moment they head for the forest, while they look for a tree and while they chop, and when they go to a big forest; finishing work puts it away | todo | |
| Fishermen hold a fishing rod on the way to their fishing spot and while they fish | todo | |
| Shepherds hold a crook through all their work: looking for a flock, taking control of it, leading it to water, food and home, waiting for it and fetching strays; the slaughter itself is not drawn | todo | |
| Farmers hold a spade on the way to the field and while they dig up the crop, but sow empty-handed | todo | |
| Builders, at each stroke of building, pick one of three clips at random and hold its tool: hammering with a hammer, sawing with a saw, or the sledgehammer swing with a heavy mallet; an into-clip already started keeps its own tool | todo | |
| Housewives carry a bag from the storage pit to their home (picked up with the pot-on-the-head clip, walked home with the carrying-pot clip) | todo | |
| Villagers fighting a fire with water hold a bag while they throw the water; fetching the water and beating the flames are done empty-handed | todo | |
| A footballer picking up the ball for a restart holds a ball and runs with it until it puts it down; footballers hold nothing while playing, celebrating, watching or doing the wave | todo | The real ball on the pitch stays a separate object, kicked back to the pitch at the restart; see play_and_gossip.md; [../town/football.md](../town/football.md#the-match) |
| Traders, missionaries, worship suppliers, workshop suppliers and builders on their way to the site have no tool of their own: they show their load (a bag of food or a piece of wood) | todo | |
| Breeder disciples and children hold nothing, and leaders have no tool | todo | There is no craftsman job; supplying the workshop is done by the storage pit trips above |
| Disciples doing a job go through that job's states, so they hold its tool (a forester disciple has the axe, and so on) | todo | See disciples.md |
| The scythe is never held: no state and no code gives it | todo | |
| Villagers taking wood from a rock, a tree or a pot play the chopping clip with nothing in their hands | todo | |

## When the tool or load shows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What a villager holds is worked out again each time it picks a new animation: its load first, then the state it is heading for, then the state it is in; a state that names an object (or "nothing") overrides the one before | todo | |
| The load shows when the villager holds more than 50 wood (a piece of wood or a log) or else more than 100 food (a bag); smaller loads are carried but not seen | todo | Thresholds from info.dat (`minWoodToShowGraphic`, `minFoodToShowGraphic` in `src/InfoConstants.h`), unused |
| A villager holding both shows the wood if it has more than 50, even when it has more food | todo | |
| A builder whose next state is building never shows a bag of food | todo | |
| A badly wounded villager (life at or below the crawling level) shows no load, but the tool of its job state still shows | todo | |
| The tool and the load disappear while the villager decides what to do next, has nothing to do, gossips, mourns, inspects or points at something, is scared stiff, confused, poisoned, on fire, fainting or weak on the ground, flees an object or a predator, flies, lands, drowns, dies or is being eaten | todo | |
| At worship the villager holds nothing: walking to the site, dancing, praying, resting at the altar and eating there; the same for dancing outside worship and the artifact dance | todo | |
| At home the villager is not drawn at all (sleeping, eating, cooking, housework), so neither is what it holds; sleeping on the floor, eating dinner, giving birth and making love all hold nothing | todo | |
| Walking home, going for a drop-off, fleeing a creature or panicking, the villager keeps showing its load | todo | |
| A villager under a script keeps the object it had when the script took it over, as long as the script holds it or plays an animation on it | todo | |

## Drawing and animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The object is drawn fixed to the grip bone at the end of the villager's arm (bone 15 of its skeleton), turning with it through every frame of the animation | todo | Which hand bone 15 is (left or right) could not be determined from the code or data |
| It is drawn only with the villager: not while the player's hand holds the villager, and not in a state whose animation is hidden (inside homes, hiding at the worship site) | todo | |
| A villager walking with any object except a saw, hammer or ball uses the carrying walk clip; running or sprinting with one uses the carrying run clip; men and women share both | todo | No villager plays its state's clip in openblack yet |
| A wounded villager crawls or limps instead, whatever it carries | todo | |
| The landing clip has a variant for landing on the feet while carrying, but landing itself always holds nothing, so the plain one is always played (outside scripts) | todo | |
| Picking up and putting down loads have their own clips: picking up sticks at the storage pit, putting them down at a building site, picking up and putting down a bag; a drop-off at the pit always uses the bag clip, even for wood | todo | |

## Sounds of tool use

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Chopping a tree sounds two axe blows and two axe releases per clip (blows at about 1.5 s and 3.6 s) | todo | Clip sound events from `Data/SmallSounds.SAS`; openblack's animation sound table (`src/Audio/AnimEffectTable.cpp`) plays only creature sounds |
| Sawing alternates the forward and backward saw sounds, each stroke stopping the other's | todo | |
| Both the hammering and the sledgehammer clips play the sledgehammer sound once per clip; the separate hammer sound is never used by a villager | todo | |
| Fishing plays one cast sound per clip | todo | |
| Sowing and digging up crops are silent: the digging sounds belong to two farmer clips the farming states never play | todo | |
| Kicks in football each play the kick sound | todo | See play_and_gossip.md |

## Picked up, thrown, hurt or killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking a villager up with the hand takes nothing from it: it keeps its wood and food while held, and a gentle put-down leaves them with it | todo | openblack villagers have no load |
| A villager the hand lets go of so that it flies, rather than being set down, lets its wood fall as a log that flies on beside it with the villager's speed | todo | The hook is called from `DynamicsFromHand.cpp` but is empty (`PhysicsClassHooks::DropCarriedResource`); see ../physics/thrown_living.md |
| Anything else that launches a villager does the same first: being knocked by a flying object or an explosion, thrown or spat out by a creature, or taken by a tornado or vortex | todo | Called from `PhysicsGameHooks::InitialisePhysics`, empty |
| A log falls only when the villager shows a load or a tool and holds more than 50 wood; a villager holding nothing visible keeps its wood through the throw | todo | |
| Food is never dropped by a throw: the villager keeps it and still has it when it lands | todo | |
| Tools are never dropped, lost or picked up: they are only drawn, and come back as soon as the villager returns to its work | todo | |
| Every death lets the log fall where the villager is (if it shows something and holds more than 50 wood): starving, old age, drowning, chanting, a miracle or spell, a fall, being sacrificed, being eaten by an animal, and a script letting go of a villager whose life has run out | todo | See death.md |
| A villager eaten by a creature drops nothing | todo | |
| On death everything else the villager carried, food and the rest of the wood, is lost; it never becomes a pile | todo | |
| A hungry villager eats from the food it is carrying (eating out, eating at home, clearing away dinner, eating at the worship site) | todo | |

## The dropped log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The log is a fallen pine trunk holding exactly the wood the villager carried, drawn with the mesh of what the villager was holding (a log of the carried tree type, plain wood, or even the tool it was showing) | todo | openblack has fallen trees (`src/ECS/Archetypes/DeadTreeArchetype.h`) but never makes one from a villager |
| It is placed on the ground and raised clear of anything it overlaps before it flies | todo | |
| It flies without hitting other objects, so it hurts nothing and knocks nothing over on its way | partial | The no-collision rule exists (see ../physics/collisions.md), but no log is ever made |
| Once down it is an ordinary fallen tree: the hand can pick it up and throw it, it can burn, and villagers wanting wood come to collect it | todo | See ../nature/ |
| Picked up again, a log of evergreen, fruit or hardwood gives back that tree type; a log drawn with any other mesh counts as evergreen | todo | |

## Logs by tree type

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wood taken from a tree, a felled tree or a fallen trunk is carried as a log of that tree's type; wood from a storage pit, a pot, a rock or a big forest is carried as its own kind | todo | The tree type is read from info.dat (`carriedType` in `src/InfoConstants.h`), unused |
| Conifers, pines, cypresses and copses give evergreen logs; beech, birch, cedar, oak, olive, palms, bushes, hedges and burnt trees give hardwood; big forests give hardwood; storage pits, pots and rocks give plain wood | todo | From the tree records of info.dat |
| No tree gives fruit-tree logs, so the fruit branch is never seen in a villager's hand | todo | |
| The tree type stays with the villager after it drops its wood, until its next wood pick-up replaces it | todo | |

## Scripts and story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can set the object a villager holds (any of the list, or nothing); the next animation change overwrites it unless the villager is under the script | todo | SET_OBJECT_CARRYING only logs "not implemented" (`src/CHLApi.cpp`) |
| Land 2's introduction of the disciple gives its villager a heavy mallet | todo | |
| The worship tutorial gives the villager building the worship site a heavy mallet | todo | |
| Land 5's waving villagers are given a fishing rod and an axe | todo | |

## Things the game does not do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers never carry scaffolds: the code for builders bringing one from the workshop is left empty, so only the hand and the creature move them | todo | jobs.md and ../building/workshop_and_scaffolds.md list builders carrying scaffolds; the game never does it |
| Mothers never carry babies; children walk after their mothers | todo | |
| There are no tribe-specific tools: every tribe uses the same meshes | todo | |
| A forester does not carry the tree it fells: the felled trunk lies where it fell, and villagers who want wood come to it and carry the wood away as a log | todo | See jobs.md |
| What a villager in the creature's hand shows, and whether it keeps its load there | todo | Undetermined: the creature's holding of villagers was not traced |
