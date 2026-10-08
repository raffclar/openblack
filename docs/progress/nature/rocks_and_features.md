# Rocks and features

The fixed and loose scenery on the land: rocks and boulders of each land's stone, pillars and spikes, ruins and
monuments, gates, and the small things the land scripts scatter.

**Progress: 8/19 done, 3 partial — 50%**

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place rocks and boulders (flat, long, sharp, square and round) in chalk, limestone, sandstone and volcanic stone | done | `MobileStaticArchetype` |
| Rocks can be picked up by the hand and thrown | done | `HandGrabSystem` (rocks up to 3.6 wide), thrown into the game's physics (`DynamicsSystem`); tests `HandGrab.RocksWiderThanTheHandCanLiftStay`, `HandGrabSystemWithWorld.ARockIsTakenOnceTheHandHasFadedIntoItsPull`; see `../hand/` |
| A thrown rock tumbles, lands and hurts what it hits | done | The game's physics (`DynamicsSystem`) and blows (`PhysicsGameHooks::ReactToImpact`); see `../physics/` |
| A rock hit hard enough splits in two | done | `object_physics::KnockRock`, `SplitRock`; the hand's tap and the creature's blow split tall rocks too (`TapRock`, `SmashRock`); test `test_physics_objects`; detail in [rocks_splitting_and_heat.md](rocks_splitting_and_heat.md) |
| The creature picks up rocks, throws them and attacks with them | todo | see `../creature/` |
| Footpaths go round rocks | todo | |
| Challenge scripts make rocks | todo | `CHLApi.cpp` notes no rock archetype yet |

## Features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place features: pillars and spikes in each stone, temples, statues, pyramids, the needle, the acropolis, the mine, the ark and its dock and wreck, the whale, craters, tombstones | done | `FeatureArchetype` |
| Features with a physics shape are solid | done | Features are bodies in the game's physics when something moves near them (`physics_classes`, `DynamicsSystem` wake walk); Bullet is gone |
| Planned features are built up over time (unconfirmed which) | todo | |
| Features catch fire and burn | partial | `FireSystem.cpp` burns them; unconfirmed whether they can burn in the game |
| Features cast shadows | done | `RenderingSystem.cpp` |
| Mushrooms and toadstools the creature can eat (magic mushrooms change it) | todo | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) and `../creature/` |

## Animated scenery

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place animated scenery: gates and land transitions | partial | `AnimatedStaticArchetype` places them; they don't move |
| Gates open when enough gate stones are laid (unconfirmed exact rule) | todo | see `../story/` |
| Gate totems (ape, cow, tiger, blank) stand by the creature gates | partial | placed as scenery only |

## Loose objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fences, lanterns, barrels and other loose objects placed by the land scripts | done | `MobileObjectArchetype`, `MobileStaticArchetype` |
| Loose objects can be picked up, thrown and broken | todo | see `../physics/` |
| Some loose objects crush new buildings or block them (unconfirmed which) | todo | |
