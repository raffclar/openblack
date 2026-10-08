# Fire

Anything can be heated. Each heated object holds a temperature; at its burning point it catches, burns hotter, loses
life, chars and heats everything within reach, so fire spreads from tree to tree and house to house until it burns out,
is beaten out by villagers, or is cooled by water or rain. Villagers flee, fight or skirt it; those it reaches run
burning. The miracles that start fires have their own pages: [../miracles/fireball.md](../miracles/fireball.md),
[../miracles/lightning.md](../miracles/lightning.md), [../miracles/storm.md](../miracles/storm.md),
[../miracles/blast.md](../miracles/blast.md); putting it out with water is in
[../miracles/water.md](../miracles/water.md).

openblack: `src/ECS/Systems/Implementations/FireSystem.cpp` (the fire list, spread, groups, sound),
`src/Fire/FireModel.cpp` (temperature, heat, damage, charring), `src/Fire/FireGraphic.cpp` (flames, steam, smoke),
`src/ECS/Systems/Implementations/VillagerFire.cpp` (villagers), tests in `test/test_fire.cpp`. It was audited line by
line against the game and fixed (audit "firewater"), and checked in the testbed scenarios `miracles.forest_fire`,
`miracles.fireball_building`, `miracles.water_fire` and the storm village fire.

**Progress: 71/77 done, 4 partial — 95%**

## Heat and burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every object hotter than the air carries a fire record with its temperature; the air is about 24.7 degrees everywhere | done | `FireModel.cpp`; test `FireModel.NothingCatchesBelowFortyDegreesAndHoldsAtLeastOneUnitOfHeat` |
| Each kind of object has a burning point (at least 40), a heat capacity and a burn defence from the game's tables | done | `FireSystem.cpp` reads the info rows |
| Typical materials: villagers catch at 120, animals 140, trees and bushes 110, dead trees 100, huts 150, civic buildings 160–200, wonders 250, creatures 200, fences 300 | done | Data-driven |
| Rocks, stones, toys, flowers and bonfires have no burn defence: they glow and spread heat but are never hurt | done | Data-driven; see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Features (pyramids, pillars) burn only at 2000 with a huge capacity, so they are fireproof in practice | done | Data-driven |
| At or above its burning point an object burns; it heats itself by a tenth of its heat over twice its burning point each turn, up to twice its burning point | done | Test `FireModel.ABurningThingHeatsItselfSlowlyAndHurts` |
| A burning object loses life each turn by how far it is above its burning point, times its burn defence (a villager at full heat dies in about 20 turns; a tree lasts about 1000) | done | Test `FireModel.BurningAtTwiceItsCombustionHurtsByItsDefence`; in game, trees burn down about 100 s after catching |
| Above three times its burning point it is "very hot" and bursts into extra flames | done | `FireModel.cpp`, `FireGraphic.cpp` |
| A fire's strength (size of flames, reach, sound) grows from 0.8 of the burning point to twice it, and shrinks as the object's life runs out | done | Test `FireModel.TheFireIsFiercestAtTwiceItsCombustionAndWeakAsItsObjectBurnsAway` |
| An object not burning cools by its surface over its capacity, faster the hotter it is | done | Test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| The fire record is dropped once the object is back within 0.1 degrees of the air and has no charring left | done | Test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| Below 0.6 life a burning object chars; when the fire is out the charring slowly fades back to what its life allows | done | Same test |
| A burn from a miracle or a villager's beating pulls the temperature towards the air's plus the burn | done | Test `FireModel.ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn` |
| Scripts can set an object on fire, set its temperature, ask if it is on fire or a fire is near, and stop it being hurt or set alight | done | `CHLApi.cpp` (SET_ON_FIRE, SET_TEMPERATURE, IS_ON_FIRE, IS_FIRE_NEAR, SET_HURT_BY_FIRE, SET_SET_ON_FIRE) |
| A fire set by a script, or by an object's own owner, is credited to that object's owner | done | Fixed in the firewater round (`FireSystem::SetTemperature`, `SetOnFire`) |
| Fires are processed once a turn, newest first, after the living things and before the scripts and miracles | done | `FireSystem::ProcessTurn`; fires made earlier in the turn burn the same turn |

## Catching and spreading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning object heats every object in the cells within its reach (its fire radius plus 10 m), searched outward in a spiral | done | `FireSystem::Spread` |
| It reaches a target only when their distance is less than the target's own size plus the fire's radius (up to 1.25 × the object's size) | done | `FireSystem::HeatTransfer`; game distances used |
| Tall fires and tall targets also need to overlap in height | done | Same |
| The heat passed is ten times the temperature difference, at most half the burning object's stored heat | done | Test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat` |
| A fire not yet burning only passes heat to things that catch at or below its temperature, and loses what it gives | done | Raw burning point compared (fixed) |
| A fire never heats the thing that heated it | done | `FireSystem::HeatTransfer` |
| The wind does not steer fire: the game works the wind out and throws it away | done | No wind in `Spread` |
| Fires that touch join one blaze; the blaze's head owns the list of villagers fighting it, and hands over to another hot member when it cools | done | `FireSystem::JoinBlaze`, `HandOverBlaze` |
| A dead tree's reach is 0.35 of its height; everything else uses its width | done | Fixed (`FireSystem.cpp`) |
| Fields burn their food instead of their life, even when scripts said they are not hurt by fire | done | `world_objects::ReduceLife` for fields |
| Trees catch from neighbouring trees, so whole forests burn | done | Scenario `miracles.forest_fire`: 35 trees catch in about 20 s |
| Huts catch from burning villagers and trees beside them, and villagers from burning huts | done | Scenario `miracles.fireball_building` |
| Fires start from the fireball, lightning (miracle and storm), the blast's heat and scripts | done | `MagicSystem.cpp` burns; `GameParticleWorld::StrikeWithoutMiracle` for storm bolts |
| A thing held in the hand over a fire catches from the fires in its cell | done | `HandGrabSystem::ProcessTurn` heats what is held each turn through `FireSystem::HeatHeldObject`, only inside its holder's influence as in the game (`d03601c7`); a bonfire is no fire, so it heats nothing (R15); test `HandGrabSystemWithWorld.WhatIsHeldIsHeatedEachTurnAndDroppedOnceGone`. See [../hand/holding.md](../hand/holding.md) |
| Picking up a burning object takes it out of its blaze; villagers flee a burning object carried in the hand (reach 30, running 20–50 m) | done | The pick-up calls `FireSystem::StartedMoving` and raises the burning-object-in-hand reaction; a held burning thing spreads its fire only inside its holder's influence and makes no fire reactions (`FireSystem::IsHeld`, `6afe6217`) |
| A burning object flying through the air keeps burning and heats villagers fighting fires | done | Starting physics takes it out of its fire group and it keeps burning (`DynamicsSystem::StartPhysicsAsObject` → `FireSystem::StartedMoving`); flying things are off the map for fire and make and clear no fire reactions (`FireSystem::IsInMap`, `6afe6217`) |
| When a rock splits, both halves keep its fire | done | `object_physics::SplitRock` → `FireSystem::CopyFire`; see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| A bonfire burns for ever and never hurts | done | A bonfire is a rock with an endless bonfire spot visual and no fire, so it never spreads, hurts or heats (`MobileStaticArchetype`, `41c5429e`) |
| Villagers hidden in a building, at home, held or dying never catch | done | `villager_fire::CanCatchFire` (fixed) |

## Putting fires out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In water (a water or shore cell, under 2 m up) a fire cools fifty times faster, so burning villagers and things that reach the sea go out | done | `FireSystem.cpp` |
| Rain or snow cools fires faster (up to 2.27 times in the heaviest rain) | done | Same; scenario storm village fire |
| Rain putting out a fire makes villagers stop to watch the storm, once | done | `FireSystem.cpp`; scenario `miracles.water_fire_watchers` |
| The water miracle's drops cool what they hit by 4000 degrees each | done | See [../miracles/water.md](../miracles/water.md) |
| A burning villager does not look for water; it runs about at random until its fire dies | done | `villager_fire::OnFire` |
| A fire is out when it drops below its burning point; the object then smokes for 30 turns where it stands | done | Test `FireGraphic.SteamHissesOnceAsAHotFireIsCooledAndPuffsForThirtyTurns` |
| A fire still hotter than 75 degrees that is being cooled gives off steam with a hiss, once per cooling | done | Same test |

## Villagers and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fire hotter than 100 (or burning) makes villagers within 35 m react to it, at high priority; the reaction is removed when it cools | done | `FireSystem.cpp`, `villager_fire::ReactToFirePriority` |
| A reacting villager first looks at the fire, and runs 10 m away if it is close or within the fire's safe distance | done | `villager_fire::ReactToFire` |
| Villagers of the fire's own town decide whether to fight it, by how big the blaze is, how many already fight it, its importance and how far they are from home | done | Same |
| Villagers on their way to worship don't join in | partial | The on-the-way-to-worship mark is never set in openblack yet |
| Fire fighters beat the flames from 2 m, cooling it by 8 degrees a blow, and are immune to its heat | done | `villager_fire::PutOutFireByBeating` |
| Villagers never fetch water to fight a fire; those states exist but are never chosen | done | `villager_fire::PutOutFireWithWater` returns at once |
| Villagers that won't fight go round the fire on the side they lean to | done | Test `ViaPoint.AVillagerGoesRoundAFireInItsWayOnTheSideItLeansTo` |
| A villager the heat reaches runs: a burning one 4–10 m in random directions while it burns, then goes back to what it was doing | done | `villager_fire::OnFire`; scenario `miracles.fireball_building` |
| A burning villager dies when its life runs out, credited to whoever made the fire | done | `villager_fire::DieByEffect` with the fire's death put down to whoever lit the fire (`world_objects::EffectDeath`, `732578c9`) |
| A villager leaving a fire reaction always ends up in a sensible state | done | Fixed (`villager_fire::ReactionValidate`, "end a fire reaction cleanly") |
| The town counts injured villagers as fire hurts them | done | `Town::injured` at the strict 0.7 crossing on every villager life change, fire's included (`7f763cbe`) |
| Animals react to fire and flee it | done | The fire reaction reaches animals, which flee at once and give up beyond the reaction's run-away distance (`ReactionSystem`, `AnimalSystem`, `Animal::fleeReaction`, `41c5429e`); see [../animal/](../animal/) |
| Burning animals die (fall and lie) rather than vanish | done | Animals killed by effects fall dead (`AnimalSystem::SetDying`) |

## The creature and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can catch fire; it burns very slowly (it takes about 500 turns of full heat to lose all its life) and faints rather than dies | partial | Catches and loses life (`FireSystem.cpp`); fainting via `world_objects::ReduceLife`. Fight health side unchecked |
| A creature that catches gets burn marks on its skin, not flames | done | Three tries about the groin on catching fire, burn one time in three, through the game's skin-mark ray with its triangle skips and unsigned texel wrap (`src/ECS/CreatureScars.*`, `NearestSkinIntersection`, `05c66517`, `6afe6217`, test `test_creature_marks`) |
| A creature reacts to fires near it, can put fires out, set things on fire, and stop itself burning (with the water miracle if it knows it) | todo | Only its beliefs see fire (`CreatureMindLearning.cpp`); see [../creature/](../creature/) |
| A creature's warmth comes from the climate only: fires don't warm it | done | In the game the creature's warmth comes from the climate only; openblack's weather-based temperature already matches (R19); see [../creature/desires.md](../creature/desires.md) |

## Buildings, trees and fields burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning building empties: its people come out | done | `world_objects::ReduceLife` |
| A building burnt to nothing flickers out as a ghost and goes; buildings a script holds stay as a building site | partial | Ghost and removal done (`world_objects::DestroyedByEffect`); building sites and script-held buildings missing. See [../building/](../building/) |
| A burning tree darkens, its leaves thin as it heats, and below 0.2 life it narrows (but keeps its height) until it is gone | done | Test `FireGraphic.ABurningTreeDarkensThinsAndShrinksAwayAtTheLast` (fixed to narrow only) |
| A tree burnt away is simply removed: there is no burnt-tree stump left | done | `world_objects::Destroy` |
| A burnt field loses its crop and its fire | done | `world_objects::DestroyedByEffect` |
| A burning field never lights the land around it | done | Fixed |
| Things that catch lose the reactions they gave (a pot's food, a dead tree's wood) and get them back when the fire ends | done | Dead tree wood reaction; a pot's reaction removed on catching and set up again when its fire ends if it holds something and isn't a store's pile (`41c5429e`) |

## Look and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flames: 2 on trees, 7 on things 3 m or bigger, else 2; more grow in as the fire strengthens; each lives 4.3 s, fading in over 1 s | done | Tests `FireGraphic.FlamesBySizeAndCellsByAge`, `FlamesGrowInByTheFireAndFadeOver4Seconds` |
| Flames sit at random points on the model (on trees in the lower half; on moving things on their bones) | partial | Moving things' flames sit on their bones as posed this frame (`posed_model::BonesOf`, `41c5429e`); buildings' flames aren't placed on the damaged model |
| Flame size: 0.2 of the height on trees, 0.3 on the creature and the temple, 0.5 on everything else | done | Fixed (`FireGraphic.cpp`) |
| A fire long out of sight catches up its flames when seen again | done | Test `FireGraphic.AGraphicLongUndrawnCatchesUpByAFlamesLife` |
| Burning buildings and objects darken as they char and glow red-orange with heat | done | `FireSystem::GetCharredColour`, `GetGlowColour` (fixed) |
| Big burning objects light the land around them with a flickering glow | done | `FireSystem.cpp` light map (fixed list of which objects) |
| Smoke and steam puffs drift with the smoothed wind and rise | done | `FireGraphic.cpp` |
| Only one fire crackle loop plays, on the burning fire nearest the camera on the ground; a farther fire never takes it over | done | Fixed `FireSystem::ConsiderSound`; scenario `miracles.forest_fire` |
| The crackle stops when its fire weakens below a tenth | done | Same |
| There is no catching, going-out or collapse sound | done | Matches the game |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fires (temperature, charring, blaze, fighters) are saved and loaded with the game | todo | openblack has no saved games yet |
