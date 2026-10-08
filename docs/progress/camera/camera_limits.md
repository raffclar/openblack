# Camera limits and collision

Where the world's camera is allowed to go: how high and how far out, how close to the land and sea, how far it may tilt,
the zones a land keeps it out of or inside, and how near it draws.

**Progress: 5/15 done, 6 partial — 53%**

## Bounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera keeps within a disc around the island and is pushed back when it strays outside | partial | `DefaultWorldCameraModel::ConstrainDisc` (centre 2560, 2560, radius 5120); not compared with the game at the edge |
| The camera never rises above a ceiling height | partial | `ConstrainAltitude` (30 000); not compared with the game at the ceiling |
| The camera floats at least a little above the land or sea under it | partial | `k_FloatingHeight` 3 in `DefaultWorldCameraModel.cpp`; not checked against the game on steep land |
| The camera's tilt is kept between looking slightly up and nearly straight down | done | pitch clamp in `DefaultWorldCameraModel::TiltZoom`; recorded runs `TiltUpDown`, `TiltDownZoomOut` |
| What the camera looks at is kept on the land rather than drifting off over the sea | partial | "drag focus on land" step in `DefaultWorldCameraModel::UpdateModeDragging`; behaviour away from drags not checked |
| The focus is kept within a set distance of the camera | todo | not found in openblack (unconfirmed exact rule) |

## Collision

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving the camera stops short of land in its way | done | `camera_pan::StopShort`; `CameraPan.StopsThreeShortOfLandInItsWay` |
| A move that goes down meets the sea and stops there | done | `CameraPan.TheSeaIsMetGoingDownNearTheCamera` |
| Zooming or turning into a hill does not put the camera inside it | partial | `ConstrainCamera` re-floats the camera after each frame; not compared with the game |

## Zones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land can have exclusion domes the camera cannot enter, and the camera is eased out of one it strays into | todo | the land's zone files (`Data/Zones/*.exc`) are not read |
| Scripts can set an inclusion zone that keeps the camera inside an area, as the first land's tutorial does in stages | todo | `SET_CAMERA_ZONE` is a stub in `src/CHLApi.cpp` |
| Clearing a land's zones as a new land loads | todo | no zones in openblack |

## Drawing distance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The near plane comes in close to the ground and goes out with height, so land and objects aren't cut away | done | `src/Camera/NearClipping.h`; `NearClipping.FollowsTheHeightOverTheLand` |
| Scripts and camera paths can bring the near plane right in for close shots | done | `CinematicDirectorSystem::SetCloseClipping`; `NearClipping.ScriptsCanClipClose` |
| The world camera's field of view is the game's default lens | partial | `config.cameraXFov`; the game's exact default lens not confirmed (unconfirmed) |
