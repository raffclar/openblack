# Gesture trails and effects

What the player sees and hears when drawing gestures: the glowing chain behind the hand while it draws, and the trail
of sparkles and light a recognised gesture leaves on the land in the player's colour. Also gestures drawn by scripts and
the tutorial.

**Progress: 20/24 done, 0 partial — 83%**

## A recognised gesture's trail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every recognised gesture except the scribble leaves a trail on the land | done | `gesture::ShowsRecognition`, `GestureSystem::LayTrail`; seen in game |
| The trail needs at least two of the drawn points to lie on the land | done | `src/Gestures/GestureTrailBuilder.cpp`; test `GestureTrailBuild.NeedsTwoPointsOnTheLand` |
| The trail is built from the points of the drawn path and from the gesture's ideal shape, fitted to where it was drawn on the screen and laid on the land under it | done | test `GestureTrailBuild.TheShapeFitsTheDrawnBoxAndHasAsManyPointsAsThePath` |
| Each gesture's ideal shape comes from its own path file in the game's data, the circle's for a gesture without one | done | `PathSymbol<n>.cam` through the camera path cache in `GestureSystem.cpp`; test `GestureTrailSymbol.PutInTheUnitSquareAndTakenEvenlyAlongIt` |
| Seen from a low camera, the shape is squashed up the screen until it is no more than twice as deep as wide | done | test `GestureTrailBuild.ADeepShapeIsSquashedUpTheScreen` |
| Where the screen shows no land under the shape, its point is put a fixed distance from the camera | done | `src/Gestures/GestureTrailBuilder.h` |
| A recognition sound plays as the trail appears, without position for the player's own gestures | done | `src/Particles/ParticleGestureRules.cpp`; test `GestureEffects.ATrailIsHeardAndItsParticlesFlowToTheShape` |
| Another player's gesture is heard from where their hand was | n/a | No network play (see ../multiplayer/) |
| Sparkles in the player's colour flow from the drawn path to the gesture's shape over about half a second | done | `src/Particles/ParticleGestureRules.cpp`; test `GestureTrailMaths.TheTransitionEasesFromTheDrawnPathToTheShape` |
| The sparkles are sized by the length of the shape | done | `src/Particles/GestureTrail.h` |
| The shape is lifted towards a camera looking down on it | done | test `GestureTrailMaths.TheShapeIsRaisedTowardsACameraAboveIt` |
| The trail is revealed from both ends | done | test `GestureTrailMaths.TheTrailGrowsFromBothEnds` |
| The sparkles wiggle, each in its own way, then disperse and shrink away | done | test `GestureTrailMaths.TheWiggleRisesAndFallsThenShrinksOnceDispersed` |
| After a couple of seconds the sparkles flash and the hand glows in the player's colour, both dying away | done | tests `GestureTrailMaths.FlashesAndGlows`, `GestureEffects.TheHandGlowsAsTheTrailFlashesAndTheTrailGoes` |
| A sheet of starry light rises along the shape and fades over its life | done | `src/Particles/LightSheet.cpp`; tests `GestureTrailMaths.TheSheetRisesAndFallsOverItsLife`, `GestureTrailSheet.ItsStrengthRunsAlongItAndTheWaveRolls` |
| The sheet of light is drawn additively, from both sides, its stars sliding along it | done | `src/Particles/LightSheet.cpp`, `src/Particles/ParticleDrawFrame.cpp` |
| Gestures drawn in quick succession each leave their own trail | done | Trails wait in a queue, one taken a step (`ParticleWorldGestures.cpp`) |

## The chain behind the drawing hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chain follows the hand while it holds a miracle it can power up, or one a circle sizes | done | `GestureSystem::IsGesturing`, `src/Particles/ParticleGestureRules.cpp`; test `GestureEffects.TheChainFollowsTheGesturingHand` |
| The chain also follows the hand while the miracle selection is open | todo | The selection is not ported (see [miracle_gestures.md](miracle_gestures.md)) |
| Its links are laid each time the hand has moved far enough, older links moving down the chain | done | `src/Particles/ParticleGestureRules.cpp` |
| The chain is sized by the hand and by how far the hand is from the camera | done | test `GestureTrailMaths.TheChainIsSizedByTheHandsDistance` |
| When the hand stops gesturing the chain is left to fade and goes a few seconds later | done | `src/Particles/ParticleGestureRules.cpp` |
| Turning the chain on or off starts or stops a force-feedback effect | n/a | Force-feedback mice are not supported |

## Gestures drawn by the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can draw a gesture's trail on the land as if the player had drawn it | todo | The script function is a stub in `src/CHLApi.cpp` (PLAY_GESTURE) |
| Tapping some objects draws a gesture's trail over them | todo | (unconfirmed which objects) |
| The tutorial plays recorded hand demos in which the hand draws gestures for the player to copy | todo | PLAY_HAND_DEMO and IS_PLAYING_HAND_DEMO are stubs in `src/CHLApi.cpp`; see ../hand/ and ../story/ |
