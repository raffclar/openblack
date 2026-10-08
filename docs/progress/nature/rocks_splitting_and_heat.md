# Splitting and heating rocks

Rocks and boulders can be broken in two by the hand, by the creature and by hard knocks, again and again, each half a
smaller rock of the same stone. They can also be heated: stone never burns away, so a rock stays as hot as whatever
heated it, glows, sets things alight around it and cools slowly. This file goes into the detail behind the rock rows of
[rocks_and_features.md](rocks_and_features.md), [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md),
[../physics/collisions.md](../physics/collisions.md) and [../physics/fire.md](../physics/fire.md); the fire model itself
(heat, spreading, cooling, water) is in the fire file.

openblack: `src/ECS/ObjectPhysics.cpp` (knocks, splitting, the tap and the creature's blow), `src/Physics/ObjectRules.cpp`
(the knock and tap rules), `src/Fire/FireModel.cpp` and `FireSystem.cpp` (heat).

**Progress: 33/51 done, 4 partial — 69%**

## Which rocks split

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Everything the game's tables class as a rock can split: the three boulders and the flat, long, plain, sharp and square rocks in chalk, limestone, sandstone and volcanic stone, the scripts' plain rock and the meteor | done | `object_physics::IsRock` (the table's rock class) |
| Singing stones, the weeping stone, the idol, lanterns, gate totems, the altar and toys are other kinds and never split | done | Same test |
| Dead trees are handled like rocks in many ways but never split: a blow only lets a wood store take them, and they can't be tapped | done | Dead trees are not of the rock class in openblack; see [trees.md](trees.md) |
| A bonfire is never worn by knocks or broken by a tap | done | `object_physics::IsBonfire` |

## How a rock splits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rock is replaced by two rocks of the same kind, each 0.7935 of its scale (about half its volume, so half its weight), turned to its heading, at full life | done | `object_physics::SplitRock`; test `PhysicsObjects.RockHalvesAreHalfItsVolume` |
| The halves lie either side of where it was, along a line at a random angle (from the game's random numbers), each 0.7935 of its width from the centre, at its height | done | Same |
| A flying rock's halves fly on at its speed with half its spin; a resting rock's halves start still and drop | done | Same |
| A flying rock the physics has lost track of doesn't split | done | Same |
| A burning or hot rock's halves both carry its heat | done | `FireSystem::CopyFire`; see [../physics/fire.md](../physics/fire.md) |
| The halves keep the rock's owner (the player who last threw it) | todo | openblack rocks keep no owner |
| The halves are new rocks: if the rock was a town's artefact, that is lost with it | todo | openblack has no artefacts; see [../town/artefacts.md](../town/artefacts.md) |
| Nothing limits how often a rock can be split; only the hand's tap and knocks stop at rocks 0.7 m tall or less | done | `objects::RockBreaksWhenTapped`, `objects::RockWear` |
| A split from a knock has no dust, particles or sound of its own; only the tap and the creature's blow play a sound | done | `object_physics::KnockRock` |

## Knocks and landings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every turn, every rock in the physics that took a blow (from what it hit, what hit it, or the ground) is worn by it | done | `PhysicsGameHooks::ReactToImpact` → `object_physics::KnockRock` |
| A blow wears a rock only when it is harder than four times the rock's weight and the rock is taller than 0.7 m | done | `objects::RockWear`; test `PhysicsObjects.HardKnocksWearRocksAway` |
| A rock is never worn by another rock, so rocks thrown at rocks bounce off whole | done | Same |
| Each such blow takes (strength over its weight − 4) × 0.005 of its life; below 0.01 life it splits | done | Same |
| A rock thrown hard into the ground or a building can wear and split itself on landing | done | Every body's blow is measured, not only what was hit |
| Miracles and everything else split rocks only through such blows, when something throws a rock about; no miracle, script or other rule splits a rock directly | done | Only the knock, the tap and the creature's blow split rocks, in the game and in openblack |

## The hand and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a rock taller than 0.7 m in the player's influence splits it, with one of four cracking sounds in turn, and the player's creature sees the player playing | done | `object_physics::TapRock`; see [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md) |
| The tooltip over a tappable rock says "hit to break" | todo | tooltip work |
| At the blow of its smash, the creature splits the rock it hits, with one of four smashing sounds in turn | done | `object_physics::SmashRock` from `CreatureObjectActionSystem` |
| The creature's blow on a villager or animal does nothing (the game's smash only breaks rocks and buildings) | done | Same |
| The creature can choose to smash a stone in half by stamping on any rock | todo | No such action is chosen in openblack; see [../creature/object_actions.md](../creature/object_actions.md) |
| The creature can choose to break a rock: it picks up a rock it can lift, throws it at a spot three of its heights away, then goes to shatter it | todo | What the shatter step does at the rock is not traced; see [../creature/object_actions.md](../creature/object_actions.md) |

## Rocks as wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every rock holds wood: its table's wood value (200 for every rock and boulder) times its scale | todo | openblack gives rocks no wood |
| Taking wood from a rock shrinks it by the share left, and takes its life down with it; a rock with all its wood taken is gone | todo | |
| Nothing in the game takes wood from a plain rock: villagers only go for wood to dead trees, which share this rule, and rocks are not stores | todo | Who could take it (the hand or the creature) is not traced; there is no quarrying |

## Heating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every rock and boulder burns at 500 degrees, holds heat like 4000 units, and is never hurt by fire | done | Read from the game's tables (`FireSystem` materials) |
| Stone never heats itself: even at 500 or more a rock does not burn hotter, it only cools whenever nothing heated it that turn | done | `fire::Step` (no burn defence: no self heating) |
| A rock gets only as hot as what heats it: burning trees (at most 220 degrees) and huts (300) can't bring it to 500; the fireball (up to 6000), lightning and scripts can | done | Heat passed is capped at the temperature difference (`fire::HeatTransfer`) |
| A fireball's ball and its burn heat a rock like any other object | done | `MagicSystem` burns; rocks take burns |
| A rock held in the hand is heated by the fires in its cell, but only while the hand is inside its player's influence | partial | `FireSystem::HeatHeldObject` heats it wherever the hand is |
| A hot rock held in the hand passes its heat on only inside the holder's influence | partial | openblack doesn't know what the hand holds when spreading (`FireSystem` `IsHeld`), so a held rock heats everywhere |
| The hand is never hurt by a hot rock and never drops it | done | Nothing in either game ties the hand to heat |
| From 400 degrees flames grow on a rock, full at 1000; at 500 or more it is on fire | done | `fire::FireFraction`, `FireGraphic.cpp` |
| Below 500 a hot rock heats only things that catch at or below its temperature (villagers 120, trees 110, huts 150, fences 300) and loses the heat it gives; at 500 or more it heats everything within reach without losing any | done | `fire::HeatTransfer`, `FireSystem::Spread` |
| A thrown hot rock sets alight what it lands among by the same rules, its reach being its fire's radius plus 10 m | partial | Burning flying things keep burning; `FireSystem` doesn't yet treat flying things as off the map; see [../physics/fire.md](../physics/fire.md) |
| Hotter than 100 degrees a rock makes villagers within 35 m react as to a fire | done | `FireSystem` fire reaction |
| Villagers flee a burning rock carried in the hand | partial | See [../physics/fire.md](../physics/fire.md) |
| In the open a rock cools by its surface over its 4000 units of heat, so a big rock stays hot for minutes; rain cools it faster | done | `fire::Step` |
| Dropped in water (or on the shore, under 2 m up) it cools fifty times faster, hissing with steam once while hotter than 75 | done | `fire::Step`, `FireGraphic.cpp` |
| A hot rock glows red-orange, a thousandth of full per degree with a 20% flicker, full at 1000 | done | `fire::graphic::GlowColour`, `RenderingSystem.cpp` |
| A burning rock crackles like any fire, the nearest fire on the ground holding the loop | done | `FireSystem::ConsiderSound` |
| A script can ask whether something is on fire near a place; a rock at 500 or more counts | done | `FireSystem::IsFireNear` |

## The creature and hot rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can set something alight with a burning thing it can lift (a burning rock among them): it picks it up, goes to the target, faces it, throws it down there and waits | todo | No such action in openblack; see [../creature/object_actions.md](../creature/object_actions.md) |
| The creature's own "start a fire" action always gives up at once | todo | |

## Scripts with hot rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 2's lost-treasure riddle asks for "something hot": it is solved when a wolf and a creature's dropping are within 10 m of the altar, a shield miracle within 5 m, and anything on fire (a rock at 500 or more will do) within 10 m | todo | The condition is `IS_FIRE_NEAR` (done); the challenge itself isn't checked running; see [../story/](../story/); the quest: [the_riddles.md](../story/silver_scrolls/the_riddles.md) |
| Land 5: while the player's creature is within 200 m of the front of Nemesis's last town, every 6 seconds a burning rock (half size, 2000 degrees, not blown by the wind) is lobbed from high above to land within 30 m of a spot in front of the town over 6 seconds, trailing smoke | todo | Story script; see [../story/](../story/) |
| Each such rock that comes to rest (not caught in the hand) a few seconds later bursts: a small fireball is cast where it lies and the rock goes in an explosion | todo | Same |
| The "fire on high" challenge throws the same burning rocks at a town, and praises a good catch by the hand | todo | Challenge script; see [../story/](../story/) |
