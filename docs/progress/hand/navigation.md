# Navigation

The player moves around the island with the hand: gripping the land and dragging it, dragging by the edges of the screen to
turn and tilt the camera, and turning with the middle button or both buttons. This file covers what the hand does while
it moves the camera; the camera's own keys, zoom, limits, paths and focusing are in [../camera/](../camera/).

**Progress: 32/40 done, 0 partial — 80%**

## Gripping and dragging the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Move button with land under the cursor grips the land at the exact point under the cursor | done | `src/Game.cpp` (UpdateHandNavigation, PlaceHand), `src/Camera/DefaultWorldCameraModel.cpp`; testbed scenario `hand.drag` |
| Dragging keeps the gripped land under the cursor, sliding the camera across the island | done | `src/Camera/CameraPan.cpp`; test `CameraPan.TheLandUnderTheCursorStaysUnderIt` |
| On a slope the plane the land is dragged along leans back towards the camera | done | `src/Camera/CameraPan.cpp`; test `CameraPan.OnASlopeThePlaneLeansBackTowardsTheCamera` |
| How far the camera moves is limited by how far the cursor went | done | test `CameraPan.TheMoveIsLimitedByHowFarTheCursorWent` |
| Land gripped too far ahead isn't dragged until the button is let go | done | `DefaultWorldCameraModel::UpdateModeDragging` (maximum grip depth) |
| Pressing with only sky under the cursor drags nothing | done | test `CameraPan.LookingAboveTheLandDragsNothing` |
| The camera stops short of land, or of the sea, in its way while dragging | done | tests `CameraPan.StopsThreeShortOfLandInItsWay`, `CameraPan.TheSeaIsMetGoingDownNearTheCamera` |
| Gripping the land plays one of six grab sounds | done | `Game::PlayHandGrabSound` |
| Gripping the land throws up a puff of dust where the hand takes hold | done | `src/3D/GripLandscapeEffect.cpp` |
| Gripping the sea splashes a ring on the water and plays the ten water sounds in turn | done | `Game::PlayHandGrabSound`, `WaterRingSystem` |
| Gripping the sea scares the fish near the hand | todo | no fish yet (see [../animal/](../animal/)) |
| Gripping is silent while a script holds the cinema bars | todo | the grab sound plays regardless (unconfirmed which scripted moments mute it) |
| How far away the hand can reach and grip is set by the land's interface level | done | `src/Camera/CameraHelp.cpp`; test `CameraHelp.TheInterfaceLevelsSetTheKeysAndTheHandsReach` |

## Dragging by the edges of the screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cursor is measured from the middle of the screen, with the cinema bars measured by a 16:9 picture | done | `src/Camera/CameraDrag.cpp`; tests `CameraDrag.CursorIsMeasuredFromTheMiddle`, `CameraDrag.TheCinemaBarsMeasureByA16To9Picture` |
| Pressing away from the edges grips the land at once | done | test `CameraDrag.PressingAwayFromTheEdgesGripsAtOnce` |
| Near the sides and bottom a press is undecided until the mouse has moved far enough, then becomes a pan or a turn | done | tests `CameraDrag.NothingIsDecidedUntilTheMouseMovesFarEnough`, `CameraDrag.TheSidesAndBottomOfferTurning` |
| A quick drag from the side towards the middle pans instead of turning | done | test `CameraDrag.AQuickDragFromTheSideTowardsTheMiddlePans` |
| Turning by the edge holds the cursor on a ring around the middle and turns the camera by the angle swept round it | done | tests `CameraDrag.EdgeRotateHoldsTheCursorOnTheRing`, `CameraDrag.EdgeRotateTurnsByTheAngleSweptRoundTheMiddle`; scenario `hand.edge_rotate` |
| At the top, dragging up and down tilts the camera and across turns it | done | test `CameraDrag.AtTheTopUpAndDownTiltsAndAcrossTurns`; scenario `hand.top_pitch` |
| A drag down the whole screen tilts by seven thirds of the field of view | done | test `CameraDrag.ADragDownTheWholeScreenTiltsBySevenThirdsOfTheFieldOfView` |
| A quick tilt downwards over land pans instead | done | test `CameraDrag.AQuickTiltDownOverLandPans` |
| Watching a fight, the camera offers no tilting | done | `DefaultWorldCameraModel.cpp` (tricons cleared in a fight) |
| Scripts choose which of pan, turn, tilt and zoom the drags offer | done | `src/Camera/CameraHelp.cpp`; tests `CameraHelp.EnablingSetsOnlyTheMaskedFeatures`, `CameraHelp.TheInterfaceLevelsSetTheFeatures` |
| With a joystick the edge hints start further in | todo | openblack has no joystick input |

## Turning with the middle button or both buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The middle button turns and tilts the camera at the game's rates while the cursor stays frozen | done | `src/Input/CursorFreeze.cpp`, `DefaultWorldCameraModel.cpp`; test `test_cursor_freeze`; scenario `hand.rotate_release` |
| Letting go puts the pointer back where it was frozen, so the hand doesn't jump to a new place | done | `CursorFreeze`; scenario `hand.rotate_release` |
| While turning, the hand keeps its normal hover on the line of sight through the frozen cursor | done | `Game::PlaceHand`, `Game::UpdateHandNavigation` |
| Both buttons zoom with up and down, and turn only once the mouse has moved far enough across | done | `camera_drag::TwoButtonTurn`; test `CameraDrag.BothButtonsTurnOnlyOnceMovedFarEnoughAcross`; scenario `hand.two_buttons` |
| Both buttons never grip the land | done | `Game::UpdateHandNavigation` |
| Holding both the zoom and rotate modifiers eases in the clear view, during which the hand grips | done | `DefaultWorldCameraModel.cpp` (clear view over half a second) |

## The hand's hints while navigating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hovering by the sides or bottom with nothing under it, the hand takes its turning pose; at the top, its tilting pose | done | `src/3D/HandNavigationPose.h`; test `HandNavigationPose.HoveringAtTheEdgeOffersTurning`; scenario `hand.edge_hover` |
| The hint poses stand the hand up towards where the camera looks | done | test `HandNavigationPose.StandsUpTowardsTheFocus` |
| Dragging, the hand grips while panning, and shows turning or tilting while it does those | done | tests `HandNavigationPose.GrippingTheLandShowsTheGrip`, `HandNavigationPose.DraggingByTheEdgeShowsTurningTiltingOrZooming` |
| Showing that it turns the camera, the hand is drawn a third of its height higher | done | `src/Game.cpp` (hand animation block) |
| Every change of state or pose fades the hand from where it was drawn over 0.13 seconds | done | `src/3D/HandCrossFade.h`; tests `HandCrossFade.*` |

## Recorded hand demos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial plays recorded hand movements that drive the real hand and camera (dragging, casting, giving wood …) | todo | `PLAY_HAND_DEMO` is a stub in `src/CHLApi.cpp`; the demo files are in `Data/HandDemo` |
| Scripts ask whether a hand demo is playing and wait for its triggers | todo | `IS_PLAYING_HAND_DEMO`, `HAND_DEMO_TRIGGER` stubs in `src/CHLApi.cpp` |
| Scripts set which keys a hand demo shows being pressed | todo | `SET_HAND_DEMO_KEYS` stub in `src/CHLApi.cpp` |
| A demo can pause the game and leave the player's hand alone while it plays | todo | arguments of `PLAY_HAND_DEMO` unused |

## Network play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's and camera's movements are sent to the other players, throttled, so they see each other's hands | todo | see [../multiplayer/](../multiplayer/) |
