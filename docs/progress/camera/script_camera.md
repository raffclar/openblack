# Script camera

The camera commands of the challenge scripts: taking the camera from the player, placing and gliding it, following
things, the named camera positions of the camera library, the lens, and shaking. The cut scenes that use them are in
`../story/`; the cinema bars and fades are in [cinematics.md](cinematics.md).

**Progress: 3/28 done, 3 partial — 16%**

## Taking the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script starts camera control, taking the camera from the player (only one script at a time) | todo | `START_CAMERA_CONTROL` is a stub in `src/CHLApi.cpp` |
| Ending camera control gives the camera back to the player | todo | `END_CAMERA_CONTROL` is a stub |
| A script stores the camera's position and focus and restores them later | todo | `STORE_CAMERA_DETAILS`, `RESTORE_CAMERA_DETAILS` stubs |
| Scripts read the stored position and focus | todo | `GET_STORED_CAMERA_POSITION`/`FOCUS` push zeros |
| Scripts set how a following camera behaves: distance, speed, angle and whether it stays behind | todo | `CAMERA_PROPERTIES` stub |

## Placing and gliding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set the camera's position at once | partial | `SET_CAMERA_POSITION` moves the camera but doesn't take it into a script camera (the player can move it straight away) |
| Set the camera's focus at once | partial | `SET_CAMERA_FOCUS`, same limit |
| Read the camera's position and focus | done | `GET_CAMERA_POSITION`, `GET_CAMERA_FOCUS` |
| Glide the camera's position to a point over a time | todo | `MOVE_CAMERA_POSITION` stub |
| Glide the camera's focus to a point over a time | todo | `MOVE_CAMERA_FOCUS` stub |
| Set or glide position, focus and lens together | todo | `SET_CAMERA_POS_FOC_LENS`, `MOVE_CAMERA_POS_FOC_LENS` stubs |
| "camera ready": whether the camera has arrived | todo | `HAS_CAMERA_ARRIVED` always false |
| Set or glide the camera to face a thing from a distance | todo | `SET_CAMERA_TO_FACE_OBJECT`, `MOVE_CAMERA_TO_FACE_OBJECT` stubs |
| A point in front of the camera at a distance | todo | `GET_FACING_CAMERA_POSITION` pushes zeros |
| Whether a thing can see the camera within an angle | todo | `GAME_THING_CAN_VIEW_CAMERA` always false |

## Named camera positions and tracks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set or glide the camera to a named position from the game's camera library | todo | `CONVERT_CAMERA_POSITION`/`FOCUS` push zeros; the library (`Data/camera.edt`, `camera.bin`, `camera.scp`) is not read |
| Create a marker at a named camera position | todo | depends on the library |
| Run a recorded camera track by name | todo | `RUN_CAMERA_PATH` stub; see [camera_paths.md](camera_paths.md) |

## Following

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Make the camera's focus follow a thing, at once or gliding | todo | `SET_FOCUS_FOLLOW`, `FOCUS_FOLLOW` stubs |
| Make the camera's position follow a thing, at once or gliding | todo | `SET_POSITION_FOLLOW`, `POSITION_FOLLOW` stubs |
| Follow a thing with both focus and position at a distance | todo | `SET_FOCUS_AND_POSITION_FOLLOW`, `FOCUS_AND_POSITION_FOLLOW` stubs |
| Follow a computer player's hand | todo | `SET_FOCUS_FOLLOW_COMPUTER_PLAYER`, `SET_POSITION_FOLLOW_COMPUTER_PLAYER` stubs |
| A dual camera keeps two things in view | todo | `START_DUAL_CAMERA`, `UPDATE_DUAL_CAMERA`, `RELEASE_DUAL_CAMERA`, `CREATE_DUAL_CAMERA_WITH_POINT` stubs |
| Focus a thing on another and release it | todo | `SET_FOCUS_ON_OBJECT`, `RELEASE_OBJECT_FOCUS` stubs |

## Lens and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set, glide or reset the camera's lens | todo | `SET_CAMERA_LENS`, `MOVE_CAMERA_LENS` stubs |
| Shake the camera from a point with a radius, amplitude and time | done | SHAKE_CAMERA (`CHLApi.cpp`, `410656e9`) on all axes |
| Explosions and particle effects shake the camera, less the further away it is | partial | `ExplosionSystem::AddShake`, `Camera::SetShake`, particle sound rule `DoCameraShake`; not compared with the game |
| Turn close clipping on and off for close shots | done | `SET_GRAPHICS_CLIPPING`; `NearClipping.ScriptsCanClipClose` |
