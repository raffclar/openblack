# Thrown villagers, animals and the creature

Villagers and animals as physical bodies: how they fly when thrown or knocked, land in one of three poses, get up, or die
from the fall; and why the creature never flies. Picking them up and holding them is in [../hand/picking_up.md](../hand/picking_up.md) and [../hand/holding.md](../hand/holding.md), damage
from hits in [impact_damage.md](impact_damage.md), drowning in [water_physics.md](water_physics.md), and the creature's
falls in fights in [../creature/fighting.md](../creature/fighting.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack (`physics` branch): villagers and animals fly in the game's physics with their own 12-point bodies
(`shapes::LivingBody`, `DynamicsSystem`). A villager thrown, dropped or knocked enters FLYING; on landing it takes the
game's posture and heading, plays its landing (or dies of the fall, or drowns on water) and decides afresh
(`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); animals land, die of the fall and move their flock's home
(`PhysicsGameHooks.cpp`); the creature is struck and hurt (`HurtCreature`). The rules are in
`src/Physics/LivingRules.cpp` (test `test/test_physics_living.cpp`). openblack doesn't draw villagers with their state's
clip yet, and the creature's catching and sway aren't ported.

**Progress: 17/24 done, 5 partial — 81%**

## Being let go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal put down gently on ground that is not too steep stands where it is put, without flying, and goes to its landed state | done | Put down without flying into its landed state (`LetGoFromHand`, `villager_physics::Land`); villagers and animals play their landing out before deciding |
| A thrown villager or animal goes into its flying state and plays its kind's thrown animation (villagers: thrown, thrown dead, thrown by a vortex; each animal kind its own) | partial | FLYING with its exit rules and the thrown or thrown-dead clip drawn (`villager_physics::StartFlying`, `VillagerPose`); villagers carried by a tornado play the plain thrown clip, as the game does (`732578c9`); the vortex clip belongs to villagers flung out of an arriving vortex, which openblack doesn't have, see [../story/portals.md#arriving](../story/portals.md#arriving) |
| A thrown villager drops what it was carrying, which flies on separately with the same speed and hits no other object | todo | `PhysicsClassHooks::DropCarriedResource` is empty |
| Animals taken from a flock leave it for a flock of their own; on landing, that flock's home moves to where they came down | partial | Leaving the flock done (`HandGrabSystem` take, empty flocks deleted); on landing lions, leopards and spell wolves move their home when they lead, and tigers and wolves pick a lair from the land's forests with the game's scoring quirk (`src/ECS/LandForests.*`, test `test_land_forests`, `cb278194`); open: the wolf flock's flag that stops the lair choice has no openblack meaning |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Their bodies are simple 12-point shapes (head, waist and feet) with twice the air drag of objects | done | `shapes::LivingBody`; test `PhysicsShapes.LivingThingsAreTwelvePointBoxes` |
| Villagers are heavy and grippy (density just under water's, the highest friction); animals are lighter and slide more | done | Material rows 7 and 8 (`MaterialTable`, `physics_classes::MaterialRowOf`) |
| They tumble only from what they hit and the hand's twist after release | done | `Body`, `hand_grab::ReleaseSpinTorque` |
| A villager or animal hit by a flying object while standing is knocked into flight | done | Knocked resting obstacles (`DynamicsSystem::ProcessTurn`, `PhysicsGameHooks::InitialisePhysics`) |

## Landing and getting up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| They land on their back, on their front or on their feet, by which way their body leans sideways at the end of the flight | done | `living::VillagerLandingPose`, `living::AnimalLandingPose` from the turn-start axes; test `test_physics_living` |
| On landing they are stood up facing the way they were heading, on the ground | done | `living::VillagerLandingHeading`, `AnimalLandingHeading`; `villager_physics::Land`, `LandAnimal` |
| They then play a landing animation (each animal kind its own) and get up to decide what to do next | done | Villagers (308/309/306/307) and animals (their kind's landing clip) play it through, drawn (`VillagerPose`, `AnimalPose`), then decide (`villager_physics::Landed`, `AnimalSystem`) |
| Nothing special happens to a breeder that lands: it decides afresh what to do | done | It decides afresh after its landing (`villager_physics::Landed`) |
| One that lands on water starts drowning instead | done | `villager_physics::Land` (water cell, shallow shore included) |

## Dying from a fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hits harder than 2 G hurt them; how much is in [impact_damage.md](impact_damage.md) | done | `PhysicsGameHooks::ReactToImpact`; see [impact_damage.md](impact_damage.md) |
| A villager whose life runs out in the air dies only when it lands, killed by the player who threw it | done | Dies where it lands, put down to the thrower (`villager_physics::Land`, `villager_fire::DeathCause`) |
| A villager's corpse that is thrown lands dead again | done | `villager_physics::Land` (dead → DEAD, in water → DYING) |
| An animal whose life runs out in the air starts dying when it lands | done | `LandAnimal` → `AnimalSystem::SetDying` |

## Reactions of others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby point at or run from things flying by (the game never uses its watch state for this) | partial | Reaction 9 follows the object; villagers run (closer than twice its speed, across the map) or point with the game's clip choice, turning to watch, until it's no longer in the air (`VillagerPhysics`); sheltering villagers react only inside the shield (`6afe6217`); open: dancers have no dance groups |
| The creature watches flying objects and may learn from the player's throwing of people | partial | It learns from throws through the impact feedback and the sea (`PhysicsGameHooks::ImpactFeedback`, `HasSunk`) and catches flying things as the game does (`CreatureCatch`, `CreatureMindSystem::ForceCatch`, `cf273ae0`, `6afe6217`); the waiting stand pose and the step's check over the block of its height, cells read a row on (`creature_route::StepBlockCircles`, `dc312635`) also follow it, and 'held by another creature' always holds as openblack's creatures hold none; open: the step check's circles of the objects in that block, which the game lays out per kind of object for its route planner and openblack's routes don't yet |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is never a physics body that moves: the hand can't throw it and hits never send it flying | done | `physics_classes::CanBecomePhysicsObject` is false for creatures; its body is static |
| It is an obstacle: thrown things bounce off it and hurt it | done | Hit on its bones' ellipsoids (`shapes::Creature`) and hurt (`HurtCreature`); see [impact_damage.md](impact_damage.md) |
| Its falls in fights (recoils and being knocked out) are animations, not physics | partial | `src/Creature/CreatureFight.h` recoils and knock-outs, some directions guessed; see [../creature/fighting.md](../creature/fighting.md) |
| No physics path makes the creature fall over: its falls belong to its fights and animations | todo | See [../creature/fighting.md](../creature/fighting.md) |
| Miracle animals (wolves, bats, doves) have their own thrown and landed animations; the puzzle challenge's living pieces never fly | done | Per-species thrown, held and landed clips from the game's table (`living::ClipsOf`, test `test_physics_living`), played through; puzzle pieces never fly (`physics_classes`) |
