# Camera help

What the world's camera lets the player do, as the land's scripts allow it. The tutorials take most camera features away
and give them back one by one, can make the camera tilt itself, and show the player which key or button to use.

**Progress: 12/18 done, 1 partial — 69%**

## Features the scripts allow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tutorial interface level sets which camera features (zoom, turn, tilt, move, turn round the mouse) the player has | done | `src/Camera/CameraHelp.cpp`, `SET_INTERFACE_INTERACTION` in `src/CHLApi.cpp`; `CameraHelp.TheInterfaceLevelsSetTheFeatures` |
| Without a feature its controls do nothing | done | `DefaultWorldCameraModel::HandleActions`; commit "Let scripts, fights and the cinema bars shape the camera's mouse controls" |
| Turning round the mouse with the middle or both buttons needs its own feature | done | `k_RotateAroundMouse`; `src/Game.cpp` |
| Interface levels can take the camera keys away, or just the keys that fly to the temple, the creature and the realm | done | `CameraHelp::BlockedActions`; `CameraHelp.TheInterfaceLevelsSetTheKeysAndTheHandsReach` |
| The first tutorial level shortens how far from the camera the hand reaches | done | `CameraHelp::handReach`; same test |
| Unknown levels change nothing | done | `CameraHelp.UnknownLevelsChangeNothing` |
| A new land gives every feature back | done | `CameraHelp::ResetForNewLand`; `CameraHelp.ANewLandGivesEveryFeatureBackAndTheFirstHeight` |
| The game starts with every feature but the self-tilting camera | done | `CameraHelp.StartsWithEveryFeatureButSelfTilting` |

## The self-tilting camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera tilts itself towards a set pitch, a tenth of the way at most each frame, while the land isn't dragged | done | `camera_help::AutoPitchInput`; `CameraHelp.TheSelfTiltingCameraTiltsATenthOfTheWayAtMostTheFramesSeconds` |
| While self-tilting the camera keeps to a set height over the land | done | `DefaultWorldCameraModel::Update` |
| Scripts set the pitch and height the camera tilts to | partial | only the tutorial level's fixed pitch and height are used; no script sets its own values |

## Fixed rotation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can make the camera turn only about a fixed point | todo | `SET_FIXED_CAM_ROTATION` is a stub in `src/CHLApi.cpp` |
| Scripts can ask whether the camera has been turned to within the wanted rotation | todo | the "within rotation" condition is not bound |

## Showing the player the controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial shows glowing pictures of the key or mouse button to use, next to its text | todo | no key or mouse pictures in openblack |
| The game counts how often and how recently each camera control was used, to remind the player of ones they don't use | todo | nothing tracks control use |
| Reminders such as the zoom reminder play once a control has gone unused | todo | depends on the usage count above and on hand demos (see `../story/`) |

## During fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Watching the player's creature fight, the camera can't be sent to another fight | done | `camera_help::DuringFight`; `CameraHelp.WatchingAFightTheCameraCantBeSentToFights` |
| Watching a fight, the edges of the screen offer no tilting | done | `DefaultWorldCameraModel::HandleActions` |
