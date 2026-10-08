# World camera controls

How the player moves the camera over the island: the movement, turn, tilt and zoom keys, the mouse wheel, the middle
button, both buttons together, dragging round the edge of the screen, and double clicking to fly somewhere. Dragging the
land itself with the hand is in [../hand/navigation.md](../hand/navigation.md).

**Progress: 25/31 done, 4 partial — 87%**

## Keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The arrow keys move the camera forwards, backwards, left and right over the land | done | `src/Camera/DefaultWorldCameraModel.cpp`; recorded game runs `MoveBackwardForward`, `MoveRightLeft` in `test/camera/test_camera.cpp` |
| A movement key's step grows with the camera's height above what it looks at, within set bounds | done | `DefaultWorldCameraModel::GetZoomScale`; same recorded runs |
| The camera moves the same distance per second whatever the frame rate | done | `src/Camera/KeyboardMoveSpeed.h`; `KeyboardMoveSpeed.DefaultLeavesTheGamesDistanceUntouched` |
| Shift with the arrow keys turns the camera round what it looks at and tilts it | done | `ROTATE_ON` in `DefaultWorldCameraModel::HandleActions`; recorded run `TiltUpDown`, `TiltUpPanLeft` |
| Ctrl with the arrow keys zooms in and out | partial | `ZOOM_ON` handled in `DefaultWorldCameraModel::HandleActions`; no recorded run of the keys themselves |
| The turn keys turn the camera left and right | partial | `ROTATE_LEFT`/`ROTATE_RIGHT` bound in `src/Input/KeyBindings.h`; not compared with a recording of the game |
| The tilt keys tilt the camera up and down | partial | `TILT_UP`/`TILT_DOWN` bound; not compared with a recording of the game |
| Holding Ctrl and Shift together eases in the clear view over half a second, and the hand grips while it does | partial | `_clearView` in `DefaultWorldCameraModel`; what the view itself does in the world camera is not confirmed (unconfirmed) |
| Moving, turning and zooming keys do nothing while a script's cinema bars are in or the camera is not the player's | done | `CinematicDirectorSystem::IsInterfaceActive`, `src/Input/ShortcutKeys.cpp`; see [cinematics.md](cinematics.md) |
| A setting to speed the movement keys up or down | n/a | openblack-only (editor camera speed), `src/Camera/KeyboardMoveSpeed.h`; see `../debug/` |

## Mouse wheel and buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wheel zooms towards and away from what the camera looks at, a fixed step per notch | done | `k_WheelZoomPerNotch` in `DefaultWorldCameraModel.cpp`; recorded run `ZoomOutIn` |
| Zooming speeds up with height, so a notch covers about the same share of the view at any altitude | done | `GetZoomScale`; recorded runs `ZoomOutIn`, `TiltDownZoomOut` |
| Holding the middle button turns and tilts the camera round the point under the mouse | done | `Mode::ArcBall`; recorded run `MiddleDragRightUp` |
| Holding both buttons and moving up and down zooms | done | recorded run `TwoButtonZoomOutIn` |
| Holding both buttons and moving across turns the camera, once the mouse has moved far enough across | done | `camera_drag::TwoButtonTurn`; `CameraDrag.BothButtonsTurnOnlyOnceMovedFarEnoughAcross` |
| While the mouse turns the camera the cursor and the hand stay where they were, and the pointer is put back there after | done | `src/Input/CursorFreeze.cpp`; `test/input/test_cursor_freeze.cpp` |
| Double clicking the land flies the camera there along a curved flight | done | `Mode::FlyingToPoint`, `SetFlight`; recorded run `DoubleClickFlyTo` |
| The flight picks the heading that gives the clearest look at the place, avoiding hills in the way | done | flight scoring over 32 headings in `DefaultWorldCameraModel.cpp`; recorded run `DoubleClickFlyTo` |
| A flight plays one of four whoosh sounds as it starts | done | `DefaultWorldCameraModel::SetFlight` |

## Dragging round the edge and at the top

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A drag decides what it is from where it was pressed and how the mouse first moves; nothing happens until it has moved far enough | done | `camera_drag::DragClassifier`; `CameraDrag.NothingIsDecidedUntilTheMouseMovesFarEnough` |
| A drag started at the sides or bottom of the screen turns the camera round its focus by the angle the cursor sweeps round the middle | done | `camera_drag::EdgeRotate`; `CameraDrag.EdgeRotateTurnsByTheAngleSweptRoundTheMiddle` |
| While turning round the edge the cursor is held on a ring around the middle of the screen | done | `CameraDrag.EdgeRotateHoldsTheCursorOnTheRing` |
| A quick drag from the side in towards the middle pans instead of turning | done | `CameraDrag.AQuickDragFromTheSideTowardsTheMiddlePans` |
| A drag from the top of the screen tilts the camera when moved up and down and turns it when moved across | done | `CameraDrag.AtTheTopUpAndDownTiltsAndAcrossTurns` |
| A drag down the whole screen tilts by seven thirds of the field of view across | done | `CameraDrag.ADragDownTheWholeScreenTiltsBySevenThirdsOfTheFieldOfView` |
| A quick tilt down over land pans instead | done | `CameraDrag.AQuickTiltDownOverLandPans` |
| Pressing away from the edges grips the land at once | done | `CameraDrag.PressingAwayFromTheEdgesGripsAtOnce`; the drag itself in [../hand/navigation.md](../hand/navigation.md) |
| With the cinema bars in, the mouse controls measure by the 16:9 picture | done | `camera_drag::ViewHeight`; `CameraDrag.TheCinemaBarsMeasureByA16To9Picture` |

## Camera hints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Where the cursor rests, the camera offers hints of what a drag would do: turning at the sides and bottom, tilting at the very bottom, everything at the top | done | `camera_drag::IdleTricons`; `CameraDrag.TheSidesAndBottomOfferTurning`, `TheTopOffersEverything`, `TheMiddleOffersNoHints` |
| The hints pick the hand's pose (see the hand's poses) | done | `CameraModel::GetHandCues`; [../hand/](../hand/) |
| Moving, zooming or tilting with the keys sets the matching hints too | todo | `TODO(#709)` in `DefaultWorldCameraModel.cpp` |
| The game keeps how long the camera has been left alone, which other parts of the game read | todo | `DefaultWorldCameraModel::GetIdleTime` logs "not implemented" |
