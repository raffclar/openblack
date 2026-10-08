# Hand physics

How the hand's movement is turned into motion for what it holds: the spring that drags the hand (and what hangs from
it) after the cursor, the speed it lets go with, the hand's strength, and how the hand's motion is measured for
miracles. Pulling anchored things is in [tug.md](tug.md); the flight of thrown things is in [../physics/](../physics/).

openblack (`physics` branch): the spring is `hand_grab::HandSpring` (`src/Hand/HandGrabRules.cpp`), run by
`HandGrabSystem`. Tests: `test/hand/test_hand_grab.cpp`, `test/hand/test_hand_grab_system.cpp`.

**Progress: 20/21 done, 0 partial — 95%**

## The held thing's spring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ready to throw, the hand doesn't move rigidly: a spring pulls it after its target, and the held thing hangs from it | done | `hand_grab::HandSpring` in `HandGrabSystem::UpdateFrame` |
| The spring pulls with a stiffness of 260 per unit of stretch and is damped by 40 per unit of speed | done | test `HandGrab.TheSpringSettlesOnItsTargetAndIsCapped` |
| It is stepped in fixed steps of 10 ms of game time, as many as the clock allows and at least one a frame, counting all the time held | done | tests `HandGrab.TheSpringStepsAtLeastOnceAFrame`, `TheSpringTakesAsManyStepsAsTheGameTimeAllows`, `TheSpringCatchesUpWithTheTimeHeldBeforeItTookHold` |
| Its speed is capped at 124 units a second | done | `HandSpring` |
| The spring takes hold only once the hand is ready to throw, starting on its target with no speed the frame after; a refused release turns it off again | done | test `HandGrabSystemWithWorld.TheSpringTakesHoldTheFrameAfterThePressAndThrows` |
| Things that follow the hand directly, such as a miracle's seed, skip the spring | done | the seed follows the hand (`MagicSystem::UpdateHand`) |
| The hand is drawn where the spring puts it, lifted by what it holds | done | `HandGrabSystem::UpdateFrame`; see [holding.md](holding.md) |

## Letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The spring's velocity at the moment of letting go is the throw's velocity | done | `HandGrabSystem::LetGo` |
| A thrown thing leaves without spin; 180 ms later, whatever was let go and still has a body gets a one-turn twist of 1.6 × mass × speed about the level axis across the hand's motion | done | `hand_grab::ReleaseSpinTorque`; tests `HandGrab.ATwistTurnsAboutTheLevelAxisAcrossTheHandsMotion`, `HandGrab.TheTwistIsCancelledWhenItsCountdownEndsOnNothing`, `HandGrabSystemWithWorld.WhatIsLetGoGetsItsTwistAFifthOfASecondLater` |
| The point a fifth of a second of velocity ahead of the held thing is worked out but not used: the throw starts from the held pose | done | `DynamicsSystem::ReleasePose` |
| The thing's turn as the hand held it is kept for its flight | done | `ReleasePose` |
| The hand's velocity, spin, position and angles are what other players receive to replay the throw | todo | see [../multiplayer/](../multiplayer/) |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's greatest pulling force is 600000 times one more than its strength, and nothing ever sets the strength above 0 | done | `hand_grab::MaxForce` |
| The hand's size is kept between 0.05 and 2 times its standard size | done | The game's clamp only bounds a size factor that is only ever set to 1, so openblack's factor of 1 matches (R16 §4) |
| How much the hand can pull free depends on the thing's weight against that force | done | `hand_grab::PullAt`; see [tug.md](tug.md) |

## The hand's measured motion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's movement over each frame is smoothed into a velocity | done | `src/Magic/HandMotion.cpp` |
| A miracle thrown from the hand (a fireball) leaves with that velocity | done | `MagicSystem`; see [../miracles/](../miracles/) |
| Spinning the hand while holding a miracle gives it a spin from the hand's sideways acceleration | done | `HandMotion`; see [../miracles/](../miracles/) |
| Below a small speed the hand is taken as still | done | `HandMotion` |
| A spring-smoothed copy of the cursor trails it, which leans the hand and tilts what it holds | done | Leans the hand (`HandAnimation`) and tilts held things (`magic::hand_hold::CursorSway`) |
| Fast hand movement blows smoke and bends trees | done | `ChimneySmokeSystem`, `VegetationSystem` |
