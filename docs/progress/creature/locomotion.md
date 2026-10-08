# Creature locomotion

How a creature gets about: it plans a route round what is in its way, walks or runs along it at a speed that suits its
size, the slope and how tired it is, turns and steps off before setting out, and leaves footprints behind. It can wade
through shallows but not deep sea, follow things, run away, travel by teleport, and be carried off by a tornado.

**Progress: 40/50 done, 0 partial — 80%**

## Speed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walking is 8 and running 20 units a second for each unit of a size-based length; bigger creatures go faster | done | `creature_locomotion`; test `SpeedsGrowWithSize` |
| A creature is asked for a fraction of a little over its running speed | done | test `TargetSpeedIsAFractionOfALittleOverRunning` |
| It speeds up at twelve units a second each second | done | test `AcceleratesAtTwelveUnitsASecondEachSecond` |
| It brakes so as to stop in the distance left, and slows for corners | done | test `BrakesToStopInTheDistanceLeft` |
| Uphill is slower and downhill a little faster | done | test `UphillIsSlowerDownhillALittleFaster` |
| Exhausted, it only goes slowly | done | test `ExhaustedCreaturesGoSlowly` |
| Pulled on the leash it goes faster, up to twice its running pace | done | test `PullingTheLeashSpeedsItUp`; see [leash.md](leash.md) |
| Frozen it can't move | done | the freeze spell stops it (see [../miracles](../miracles/)) |
| Scripts can set a creature's speed (unconfirmed which functions) | todo | |

## Walking and running on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Below walking speed it blends standing and walking; above, walking and running, keeping in step | done | tests `SlowerThanWalkingBlendsStandAndWalk`, `FasterThanWalkingBlendsWalkAndRunInStep` |
| The distance covered drives the walk, so the feet keep to the ground | done | `creature_locomotion` blend |
| Standing still moves no feet | done | test `StandingStillMovesNoFeet` |
| Before setting off it steps off sideways or back into a walk, walks straight on if nearly facing the way, or turns on the spot | done | test `StartsByStepWalkOrTurn` |
| Turns and steps blend the 0, 90 and 180 degree animations of a side for the angle | done | test `AnglesPickTheirPairOfAnimations` |
| At corners too sharp to walk round it stops and starts again | done | `creature_locomotion` |
| Between game turns it is drawn moving smoothly | done | `CreatureLocomotionSystem::Update` |
| It faces down a slope to do some things (unconfirmed which) | todo | |

## Routes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land is a grid of 512 by 512 cells sorted once loaded: too steep and deep sea are blocked, shallows can be waded | done | `creature_route`; test `SortsTheLand` |
| Land cut off from the main part of the land is out of reach | done | `creature_route` |
| A creature stands clear of cells it can't walk on | done | test `CreaturesStandClearOfBlockedCells` |
| Things in the way are walked round as circles; big creatures walk through small trees and over low things | done | tests `WalksRoundSomethingInTheWay`, `BigCreaturesWalkThroughSmallTrees` |
| Villagers and animals are never walked round; other creatures only while they stand still; anything burning always | done | `creature_route` |
| Routes are planned over a finer lattice a budget a turn, straightened where clear, and their corners rounded | done | tests `OpenGroundIsAStraightLine`, `StraightensWhereClear`, `RoundsCornersInSmallSteps`, `FollowsTheRoute` |
| It goes round cliffs | done | test `GoesRoundACliff` |
| With no way there it gives up, puzzled | done | test `GivesUpWhenThereIsNoWay`; the lost face |
| It arrives anywhere on a ring about its destination | done | tests `ArrivesAnywhereOnTheRing`, `ArrivalRings` |
| A walk is given a time to arrive before it is given up | done | `creature_locomotion` |
| Stuck or trapped in an enclosed space, it tries to get out (unconfirmed how) | todo | see [decision_making.md](decision_making.md) |
| It uses the land's footpaths (unconfirmed) | todo | |
| A villager or animal under its feet can be trodden on | todo | see [object_actions.md](object_actions.md) |

## Going places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walking or running to a point | done | `CreatureLocomotionSystem::MoveTo` |
| Walking up to a thing, stopping short by both their sizes | done | `MoveToObject` |
| Following a thing as it moves, keeping within a distance | done | `Follow` |
| Running away from a point | done | `FleeFrom`; test `RunsAwayFromTheThreat` |
| Turning on the spot to face a point | done | `TurnToFace` |
| Led on the leash to the hand | done | `LeadTo`; see [leash.md](leash.md) |
| Following the hand, or keeping to the middle of the screen | todo | |
| Walking to the beach, up a hill, along a ridge | todo | see [idle_behaviour.md](idle_behaviour.md) |
| Travelling by teleport: walking to a stone and jumping to its partner | done | `TeleportSystem`; see [../miracles](../miracles/) |
| Carried off by a tornado, it faints where it lands | done | `TornadoSystem`, `ForceFaint`; test `test_tornado_creature.cpp`; see [../miracles](../miracles/) |
| Knocked over by a heavy blow or a falling thing, it falls and gets up again | todo | see [../physics](../physics/) |
| Moved by a script without walking | todo | |
| Its position is sent to the other players in a network game | todo | see [../multiplayer](../multiplayer/) |

## Footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only creatures leave footprints: a dark paw, hoof or hand on the land under the lower foot at each step | done | `creature_footprints`; tests `FootstepsLeavePrints`, `LowerFootGetsThePrint` |
| Each species has its own print | done | test `SpeciesCells` |
| Prints are turned with the foot and sized by the creature; the left foot's is flipped | done | tests `TurnedAndSizedPrint`, `LeftFootFlipsThePicture` |
| Prints fade in steps and are gone in about five seconds | done | tests `FadesInStepsOfAtLeast`, `GoneInAboutFiveSeconds` |
| At most 256 prints at once; new ones are dropped until old ones fade | done | test `FullTrailDropsNewPrints` |
| On the first of April every creature leaves a smiley face | done | test `AprilFoolsSmileyKeepsTheSpeciesSize` |
| Footsteps sound by the ground under the foot | done | see [animation.md](animation.md) |
