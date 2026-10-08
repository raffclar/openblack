# Holding

How the hand carries what it has picked up: where the thing hangs, how the hand lifts to make room for it, how it sways
and turns with the mouse, and what happens when the thing goes away or the hand meets the creature. How it moves is
in [hand_physics.md](hand_physics.md).

openblack (`physics` branch): `HandGrabSystem` holds what the hand picks up (`src/ECS/Components/HandGrab.h` on the hand,
`InHand` on the held thing), hanging and posing it by the game's hold rules (`hand_grab::HoldOf`, `HandRise`) through the
same poser the miracle seeds use (`magic::HandHoldPoser`). Tests: `test/hand/test_hand_grab.cpp`,
`test/hand/test_hand_grab_system.cpp`.

**Progress: 18/28 done, 0 partial — 64%**

## Where the held thing hangs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each thing says how it is held: on the palm (most objects), the magic grip (miracle seeds), from the side (pots, piles, mobile objects, some statics), the tree grip or the villager grip (people and animals) | done | `hand_grab::HoldOf` (`src/Hand/HandGrabRules.cpp`); test `HandGrab.EachKindHangsItsOwnWay` |
| Held on the palm, the thing hangs 0.2 units under the hand | done | `hand_grab::HandRise` |
| Held in the magic grip (a miracle's seed), it hangs 3.2 times the hand's size under it | done | `HandRise`; seeds in `MagicHeldSeed.cpp` |
| Held from the side or in the tree or villager grip, it hangs its own lowering times its height under the hand, at least 1.9 units | done | `HandRise`; test `HandGrab.TheHandRisesForWhatItHolds` |
| A standing tree hangs a further tenth of its height lower | done | `HandRise` (rooted objects only) |
| The hand rises by the whole of that hanging distance (as measured the frame before); the cursor's point on the land is raised by 0.6 of it | done | The hand's rise (`HandGrabSystem::UpdateFrame`); the raised cursor point is used by the land and hand code (`GetCursorRaise`, `Game.cpp`) |
| The held thing is drawn at its own size, and the hand's pose opens by how big it is next to the hand (up to the hand's full span) | done | `HandGrabSystem::GetHeldPose`, `magic::hand_hold::HoldTimeMs` |
| Each hold plays its own still pose: palm, magic, side, tree or villager | done | `magic::HandHoldPoser` with the held thing's hold |
| Every game turn the hold is asked again of the thing, so a thing that changes (a pile growing) changes grip | done | `HandGrabSystem::ProcessTurn` |
| A held handful is drawn as the in-hand pile, sized by its amount | done | Drawn at min(amount / its step + 0.25, 5) of its size, growing smoothly (the game's rule; it doesn't step) |
| Held wood and branches use their own in-hand meshes | todo | meshes listed in `src/3D/AllMeshes.h` |

## Swaying and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held thing tilts as the cursor runs ahead of the hand: up to 0.3 radians each way, at 80 pixels of lag | done | `magic::hand_hold::CursorSway`, shared with seeds |
| A thing held on the palm, or from the side, turns half round (or a quarter round) over 0.4 seconds to face the creature it is offered to | todo | No offering to the creature yet |
| A right hand turns it the other way from a left hand | todo | |
| The held thing faces the camera, rolled about the line to it | done | `magic::HandHoldPoser` (tests in `test_hand_hold_pose.cpp`) |

## Giving to the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a creature that would take what the hand holds, the hand offers it for 1.1 seconds after leaving it | todo | see [../creature/](../creature/) |
| While the creature reaches for it, the hand is drawn to the creature's hand over half a second | todo | |
| Moving the hand more than 15 times the creature's size away calls the giving off | todo | |
| The creature takes what is held, then looks at it, eats it, plays with it or throws it as it likes | todo | see [../creature/](../creature/) |
| Things the creature took from the hand teach it the player wanted it to have them | todo | unconfirmed: giving alone teaches nothing in the code traced; see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |

## Losing what is held

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every turn, a held thing that no longer exists, or is no longer in the hand's care, is dropped from it | done | `HandGrabSystem::ProcessTurn`; test `HandGrabSystemWithWorld.WhatIsHeldIsHeatedEachTurnAndDroppedOnceGone` |
| A held thing that is destroyed leaves the hand, which empties and stops holding | done | Same |
| Each turn the held thing is kept at the hand's position on the map, so it stays in the right place for the game | done | `HandGrabSystem::ProcessTurn` |
| A held miracle that runs out leaves the hand | done | `MagicSystem`; see [../miracles/](../miracles/) |
| A scribble shakes the held thing out of the hand | done | A scribble with a held thing inside the influence makes the hand let go as a forced drop (`GestureSystem`, `HandGrabSystem::ForceDrop`, test in `test_gesture_recognition`); see [../gesture/scribble.md](../gesture/scribble.md) |
| People react to a burning thing held near them (reach 30, running 20–50 m) | done | The reaction is raised (`FireSystem::StartedMoving`); a held burning thing spreads its fire only inside its holder's influence (`FireSystem::IsHeld`, `6afe6217`), see [../physics/fire.md](../physics/fire.md) |
| A held thing carried through trees bends them | todo | the hand alone bends them (`VegetationSystem`) |
| The camera can still be moved while holding something | todo | |
