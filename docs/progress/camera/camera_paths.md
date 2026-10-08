# Camera paths

Recorded camera flights: the paths some miracles take their caster's camera along, the land's intro flight, the camera
tracks of the camera library, and how the game's camera hands over from one kind of camera to another.

**Progress: 7/17 done, 5 partial — 56%**

## Miracle camera paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle's effect takes its caster's camera along a path placed at the miracle | partial | `CameraPathSystem::FollowPlaced`, started from `ParticleSystem.cpp`; used by the forest miracle; a re-audit of the miracle cameras is still owed |
| The camera glides from where it is onto the path's start, then follows the path a little behind, sped up | partial | `CameraPathSystem`; same re-audit |
| The path lasts one play of its animation, or ends with its miracle | done | commit "Forest camera path: lasts one play of its animation and can be taken back" |
| A movement key that would move the camera this frame, or gripping the land, takes the camera back at once | done | `src/Camera/CameraPathControl.h`; `CameraPathControl.AMovementKeyTakesTheCameraBackOnlyWhenItWouldMoveIt`, `GrippingTheLandAlwaysTakesTheCameraBack`; testbed `miracles.forest_camera_taken_back` |
| Turning, tilting and zooming don't take it back; meanwhile the player's other camera controls do nothing | done | `CameraPathSystem::HoldsCamera`; same tests |
| The hand stays free and drawn while a path has the camera | done | `CameraPathSystemInterface.h` |
| The near plane comes in close while a path has the camera | done | `src/Game.cpp` near plane, `NearClipping.ScriptsCanClipClose` |
| Other spells' camera paths (the tree goddess and teacher paths in the spell animations) | todo | not wired (unconfirmed which effects use them) |

## Path playback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A path is sampled smoothly between its points and holds the last | done | `components/cam`; `CameraPath.SamplesBetweenItsPointsAndHoldsTheLast` |
| The camera glides to where it is sent on a smooth curve arriving at a set speed after a set time | done | `src/Camera/CameraZoomer.cpp`; `Zoomer.ArrivesAtItsDestinationInTime`, `ItsSpeedIsHowFastItsValueChanges` |
| The land's intro flight over the island as a land begins | todo | `cam.cam` and `flying.cam` are loaded but only played from the debug camera window (`src/Debug/Camera.cpp`) |
| The camera library's tracks: recorded flights with smooth curved segments that scripts play | todo | `Data/camera.edt` is not read; see [script_camera.md](script_camera.md) |
| Scripts can slow the camera down for slow motion | todo | not in openblack (unconfirmed) |

## Handing the camera over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Other cameras (creature, temple, fight, path) take over from the player's and hand it back where they found it | partial | `Camera::SetModel` swaps models (creature mode, temple, editor); no general stack of cameras |
| A camera that is no longer valid (its thing gone) gives way to the one before it | partial | creature mode lets go when the creature leaves; nothing general |
| Some cameras stop the player drawing gestures while they move | partial | gestures check whether the camera moves; see `../gesture/` |
| The camera's state is kept in saved games | todo | no saved games; see `../engine/` |
