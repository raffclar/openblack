# Toys

The playthings lying about the lands: the toy ball (the "beach ball" of the Ogre's reward), the bowling ball, the
skittles, the dice and the cuddly toy (the tutorial's teddy bears). They are loose static objects the hand can pick up
and throw, each with its own physics, which the creature plays with when it wants to play and learns to play with by
watching the player. Toys mostly come from the land scripts and from reward chests. Where other files already cover a
rule in full, the row here links to them.

Each toy is a static object with one of five toy models, and the game tells toys apart by their model, not by their
row in the tables. The villagers' football also counts as a toy ball wherever the game asks whether something is a toy
or a ball. The toys' tables say a creature may not play with them, but the game never reads that column for statics:
any static may be played with.

**Progress: 36/65 done, 6 partial — 60%**

## The five toys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five toys, each its own model: a toy ball, a toy bowling ball, a cuddly toy, a die and a skittle | done | `MobileStaticArchetype`; models `ObjectToyBall`, `ObjectToyBowlingBall`, `ObjectToyCuddly`, `ObjectToyDice`, `ObjectToySkittle` in `src/3D/AllMeshes.cpp` |
| Anything with one of the five toy models counts as a toy, whatever its table row | done | `physics_classes::IsToyModel` (`src/ECS/PhysicsClasses.cpp`) |
| The villagers' football is a toy ball too: the creature may kick it about, play throwing games with it, it never hurts the creature, and the hand letting it go counts as playing with a toy | partial | The football is a toy (`physics_classes::IsToy`, `dc312635`): it never hurts the creature and the hand letting it go counts as toy play; open: the creature kicking it about and its throwing games (see below); the football's game is in [../story/minigames.md](../story/minigames.md) and [../town/football.md](../town/football.md) |
| Their table weights are: ball 100, skittle 250, cuddly toy 1000, die 1000, bowling ball 2000; the weight sets how loud a toy's knocks sound | done | the collision sound level uses the table weight (`DynamicsSystem::AttemptCollisionSound`); see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| The ball and the bowling ball knock like solid stone, the die and the skittle like hollow wood, and the cuddly toy with the soft sound mushrooms make | done | the info's collision sound kind (`PhysicsClassHooks::CollideSoundType`) |
| The game's description texts name them "Toy ball", "Toy cuddly", "Toy skittle" and "Toy bowling ball"; the die has no text of its own and reuses the cuddly toy's, so it is described as "Toy cuddly" | todo | openblack shows no object description texts |
| Over the hand, a toy feels like fur on a force-feedback mouse | todo | openblack has no force feedback |
| Fire warms toys and makes them glow, but never hurts them | done | data-driven; see [../physics/fire.md](../physics/fire.md) |

## Where toys are found

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial land holds 27 toys in two groups of skittles with bowling balls, and a few loose ones: six giant skittles (about 2.6 times normal size) with three bowling balls and two balls; ten normal skittles with two bowling balls; two dice and two more balls about the land | done | `MobileStaticArchetype`; see [../scripts/landT_script.md](../scripts/landT_script.md) |
| Land 1 has a cuddly toy and three tiny dice (a tenth to a sixth of normal size) lying together | done | `MobileStaticArchetype`; see [../scripts/land1_script.md](../scripts/land1_script.md) |
| The Firestorm map and a three-player map each have three cuddly toys | done | see [../multiplayer/maps/firestorm.md](../multiplayer/maps/firestorm.md) and [../scripts/playground_scripts.md](../scripts/playground_scripts.md) |
| Challenge scripts can make a toy at a place with an angle and a size | done | `CreateScriptObject` makes statics (`src/CHLApi.cpp`) |
| The tutorial's picking-up lesson makes five teddy bears on a hillside, makes any destroyed one again with a sparkle, and ends when all five are within 30 of the ditch | todo | the lesson's other commands are stubs; see [../story/tutorial_island.md](../story/tutorial_island.md) |
| A toy reward chest holds a ball, a cuddly toy or a die; opened, the toy appears where the chest stood, at normal size | todo | `CreateReward` is a stub; see [../story/rewards.md](../story/rewards.md) |
| Each toy chest has its good advisor line: "I say. A toy ball.", "Ah. How sweet. A cuddly thing.", "A die. Or is it a dice? No it's a die." | todo | when the line is spoken is in [../story/rewards.md](../story/rewards.md) |
| Two silver scrolls give the toy ball: Throwing Stones, and The Ogre, whose evil advisor calls it a beach ball | todo | see [../story/silver_scrolls/throwing_stones.md](../story/silver_scrolls/throwing_stones.md) and [../story/silver_scrolls/the_ogre.md](../story/silver_scrolls/the_ogre.md) |

## Sizes

Measured from the game's own models in its mesh pack, in the game's units (its metres: the units its land positions,
distances and the creature's height are given in). Sizes are width × height × depth of each model's most detailed
form at normal size; the people were measured standing in their model's resting pose. For comparison: a grown man
villager stands 1.60 to 1.66 tall (three Celtic men and the footballer measured) and a woman 1.44 (one measured). A creature stands 15 × its size: a new creature is size 0.20 to 0.29
(3 to 4.4 tall, the chimp smallest and the horse largest), size 1 is 15 tall, and a fully grown one at size 2 is 30
tall. At normal size the toy ball is about a seventh of a size-1 creature's height, as a beach ball is to a person,
and it is taller than a villager.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The toy ball is a sphere 2.15 across: 1.3 times a man's height, and half to two thirds of a new creature's height | done | the game's model, drawn by `MobileStaticArchetype` at the script's size |
| The bowling ball is a sphere 1.40 across, smaller than the toy ball, reaching a man's shoulder | done | same |
| The cuddly toy is 2.87 wide, 3.70 tall and 1.85 deep (its lower details reach 3.81 tall): over twice a man's height, as tall as a new creature | done | same |
| The die is a cube 2.78 on each side, 1.7 times a man's height | done | same |
| The skittle is 1.18 by 1.26 across and 3.11 tall, nearly twice a man's height | done | same |
| The villagers' football is a ball 0.37 across, under a quarter of a man's height; a second, identical ball model is also in the list | todo | openblack makes a football only from the debug creature spawner; see [../story/minigames.md](../story/minigames.md) |
| A land script's size multiplies the model's size | done | the scale passed to `MobileStaticArchetype::Create` |
| The tutorial's giant skittles (size 2.62 to 2.68) stand 8.1 to 8.3 tall; its giant bowling balls (sizes 1.84 and 2.62) are 2.6 and 3.7 across and its giant ball (size 2.5) 5.4 across; the other skittles (3.1 tall), bowling balls (1.4), balls (2.15) and dice (2.78) are at normal size | done | `MobileStaticArchetype`; see [../scripts/landT_script.md](../scripts/landT_script.md) |
| Land 1's cuddly toy (size 0.38) is 1.4 tall, villager height; its three dice (sizes 0.17, 0.17 and 0.11) are cubes 0.48 and 0.32 on a side | done | `MobileStaticArchetype`; see [../scripts/land1_script.md](../scripts/land1_script.md) |
| The tutorial's teddy bears and the Firestorm maps' cuddly toys are at normal size, 3.7 tall | partial | the maps' toys are placed; the tutorial's teddy lesson is not running ([../story/tutorial_island.md](../story/tutorial_island.md)) |
| A toy from a reward chest appears at normal size | todo | `CreateReward` is a stub; see [../story/rewards.md](../story/rewards.md) |
| The toys' table gives each toy a largest size of 3; whether the game ever enforces it was not traced (no shipped toy is placed bigger than 2.68) | todo | undetermined: the table value is known, no use of it was found |
| The hand grips a toy by its own size: the ball, die and bowling ball are held from above with a grip as wide as three quarters of the toy's height, the cuddly toy and skittle from the side at the toy's flat radius (half its wider side), both times its placed size; a bigger toy is held in a wider grip | done | `hand_grab::HoldOf`, `ObjectHoldRadius` (`src/Hand/HandGrabRules.cpp`) from the model's box times its scale (`world_objects::SizeOf`) |
| No toy is too big for the hand: only rocks wider than 3.6 are refused, so even the 8-tall giant skittles can be picked up | done | `hand_grab::ValidForPlaceInHand`; see [../hand/picking_up.md](../hand/picking_up.md) |
| Black & White's model list has no separate bowling pin or toy-only bowling model: its skittle and toy bowling ball are the only bowling models (the bowling lane end and its model belong to Creature Isle) | n/a | nothing to make |
| The "cheat box" model is a plain box 180 wide, 180 deep and 400 tall, all of it below the ground, with eight corners | n/a | not used by any shipped land; see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each toy has its own material in the physics; a toy with any other model would be as heavy as a rock | done | `physics_classes::MaterialRowOf`, `ToyRow` (`src/ECS/PhysicsClasses.cpp`); test `FencesAndToysHaveTheirOwnMaterials`; see [../physics/object_dynamics.md](../physics/object_dynamics.md) |
| The ball is very light (a tenth of water's density) and almost perfectly springy with hardly any damping, so it bounces high and long; air drag slows it quickly | done | material row: density 0.096, springiness 239 of 240, damping 0.019, grip 1.09, spin kept 0.93, drag 1.28 |
| The bowling ball is the heaviest (density 2.04, just over a rock's), grips poorly and keeps its spin, so it rolls a long way | done | material row: springiness 101, damping 0.14, grip 0.5, spin kept 0.92, drag 0.81 |
| The die lands dead (high damping), grips hard and keeps spinning, with no air drag | done | material row: density 0.8, springiness 41, damping 1.21, grip 1.30, spin kept 1.0, drag 0 |
| The cuddly toy is soft: a tenth of the springiness of the others, so it hardly bounces | done | material row: density 0.8, springiness 8.1, damping 0.39, grip 1.17, spin kept 0.8, drag 1.06 |
| The skittle grips well and stops turning quickly | done | material row: density 0.8, springiness 20, damping 0.27, grip 1.22, spin kept 0.63, drag 0.89 |
| Toys always take part in collisions, so a thrown thing knocks a standing skittle or die | done | `facts.interacts` for toys (`src/ECS/PhysicsClasses.cpp`) |
| A knocked skittle falls over and lies where it stops; nothing stands skittles up again or counts them | done | the body's last pose is kept (`DynamicsSystem`); the game has no skittle rule beyond its material and hold |
| A die lands on whichever face the physics leaves up; nothing in the game reads the face | done | no face rule exists to port |
| In the sea the ball floats for over a minute, the die, cuddly toy and skittle for about 15 seconds, and the bowling ball sinks at once; a sunk toy is gone once it is four of its sizes under | done | follows from the materials and the soaking rate; see [../physics/water_physics.md](../physics/water_physics.md) |
| Thrown, the bowling ball breaks buildings as a rock does; no other toy harms a building | todo | the rule is in `physics_classes` (`physicallyDestroysAbodes`); the building's reaction isn't ported; see [../physics/impact_damage.md](../physics/impact_damage.md) |
| The bowling ball costs a physical shield prayer power by its momentum, as a rock does | done | `StrikeShield` (`src/ECS/PhysicsGameHooks.cpp`); see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| A toy striking the creature never hurts it | done | `IsToy` in `src/ECS/PhysicsGameHooks.cpp`, the football included (`dc312635`); see [../physics/impact_damage.md](../physics/impact_damage.md) |

## The hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks up any toy | done | see [../hand/picking_up.md](../hand/picking_up.md) |
| The cuddly toy and the skittle are held from the side; the ball, die and bowling ball from above | done | `HoldOf` (`src/Hand/HandGrabRules.cpp`); test `test_hand_grab` |
| Toys are thrown and put down like any loose object | done | see [../hand/throwing.md](../hand/throwing.md) |
| A toy the player's hand lets go of, thrown or put down (not one a creature threw), makes the player's creature consider copying "play with a toy" | todo | `DynamicsSystem::ConsiderToyPlay` calls the hook, but `PhysicsClassHooks::ConsiderMimickingToyPlay` is empty |
| A toy put down gently near a town becomes one of its artefacts; toys count 0.0001 and the bowling ball 0.0002 | todo | see [../town/artefacts.md](../town/artefacts.md) |
| A toy thrown into a land's exit vortex (or lying within its reach) is taken in, as any thing that can fly is, and comes out of the next land's arrival vortex as the same toy; a toy that is an artefact stays an artefact of the same god with the same worth | todo | no vortex in openblack; the vortex writes a loose static's kind, size and turn (and its artefact worth and owner) to the crossing file and the arrival vortex makes it again; see [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Of the static objects, a creature can pick up only toys | todo | openblack's creature picks up only moving objects, villagers and food (`CreatureObjectActionSystem::CanPickUp`), so it never touches a toy |
| It never eats a toy and never uses one as a weapon to hurt something by throwing | done | openblack's creature eats only things with food value and throws only what it can pick up (`CreatureObjectActionSystem`) |
| Toys catch its eye: it is more than twice as keen to look at a toy as at any other static (0.9 against 0.4) | todo | openblack's creature has no per-object looking interest |
| Kick a ball about (the toy ball or the football): two to four rounds of walking up to it, facing it and kicking it, each followed one time in four by pointing at it, else a short pause; half the time its happy animation at the end | todo | the plan action "kick ball around" throws whatever it picks up instead and can't pick up a toy (`src/Creature/CreaturePlanActions.cpp`); the pause's length was not traced; see [../creature/object_actions.md](../creature/object_actions.md) |
| Throw a toy at something: half the time its happy animation first, then picks up a toy, moves to a throwing spot its own height from the target, throws at it, and half the time is happy again | todo | the plan action "throw ball at object" throws any pickable thing nearby |
| Throw a die: picks it up unless it already holds it, throws it at a spot 1.2 times its own height off along both map axes, watches it fly, then is happy two times in three and sad one time in three, whatever face comes up | todo | |
| Stroke a cuddly toy: picks it up and strokes it in its hand | todo | |
| Take a toy home: a toy more than 50 from its home is carried to a random spot within 5 of home and put down, or one time in five tossed aside | todo | see [../creature/home_and_pen.md](../creature/home_and_pen.md) |
| Play a throwing game with the player: picks up a toy and throws it at the hand, then is happy | todo | |
| Give a toy to a friend, and play throwing games with a friend using a toy | todo | see [../creature/friends_and_other_creatures.md](../creature/friends_and_other_creatures.md) |
| Any static, toys included, may be thrown into the sea for fun | todo | the plan action "throw in the sea" throws somewhere nearby; see [../creature/object_actions.md](../creature/object_actions.md) |
| These are the creature's ways to play: kicking a ball, throwing a toy at something, taking a toy home, stroking a toy and throwing a die are all on its list of things to do when it wants to play | partial | the plan actions exist for kicking and throwing at things, but none uses a toy; see [../creature/desires.md](../creature/desires.md) |
| Copying the player playing with a toy: likely (0.9), needs no leash, first a look of noticing the playful deed, then one of: throw a die, stroke a toy, take a toy home, a throwing game with the player, throw it about, kick a ball; feeds the wish to play, about three times | partial | the copying table has the entry (`MimicRule`, test `test_creature_learning`); nothing reports the deed (above) and the toy actions are not done; see [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |
| Catching a toy thrown at it | partial | Ported (`PhysicsGameHooks::OfferToCatchingCreatures`, `src/Creature/CreatureCatch.*`, `CreatureObjectActionSystem` catch, test `test_creature_catch`); open: simplified eligibility, forced plan and left hand (R18), and the clip not running back after a catch (audit C1–C6), see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers never go to play with a toy: the tables give toys the rocks' villager check with no wish to interact | done | openblack's villagers don't seek toys either |
| A toy thrown over villagers is a flying object they watch and point at | partial | see [../villager/reactions.md](../villager/reactions.md) |

## Cut and unused

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bowling game: ten villagers stand as pins in a triangle and a cow is the ball; each knocked-over villager scores, the game slows to half speed while anything is down, and all ten plus the cow make a strike ("You got a strike!") | n/a | a script source never compiled into the game's challenges; see [../story/minigames.md](../story/minigames.md) |
| Cow bowling, the same game with its own wording | n/a | same; see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |
| A bowling lane end, a bowling model and a skittle knock sound | n/a | only in Creature Isle's tables; Black & White's skittles knock like hollow wood |
| The Chimp Posse's die, teddy and ball, made at half size | n/a | never started by the game; see [../story/silver_scrolls/chimp_posse.md](../story/silver_scrolls/chimp_posse.md) |
| The "cheat box" model | n/a | see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |
