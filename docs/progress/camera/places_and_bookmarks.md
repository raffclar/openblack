# Places and bookmarks

The keys that fly the camera to set places (the temple, the realm, the creature, the temple's rooms), the bookmarks the
player drops on the land to fly back to, and where the camera starts on a land.

**Progress: 3/19 done, 10 partial — 42%**

## Temple and realm keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A single tap of the temple key puts the view back to a pleasing one over what the camera looks at | partial | `src/Camera/ZoomToPlaces.cpp`, `src/Input/ShortcutKeys.cpp`; no test or recording |
| A double tap of the temple key flies the camera to the player's temple | partial | `ZoomToPlaces`; no test or recording |
| Without a temple the temple key flies over the realm | partial | `ZoomToPlaces`; no test |
| The realm key flies the camera high over the middle of the island | partial | `ZoomToPlaces`; no test or recording |
| Pressing the key again while still looking there flies back to where the camera was | partial | `ZoomToPlaces::IsFlownTo`; no test |
| The keys do nothing inside the temple or while a script's cinema bars are in | done | `src/Input/ShortcutKeys.cpp` |
| The keys that go straight into each room of the temple | done | `ZOOM_TO_INSIDE_TEMPLE` … `ZOOM_TO_LIBRARY` in `src/Input/KeyBindings.h`; see [temple_camera.md](temple_camera.md) and `../temple/` |
| The creature key flies the camera to the player's creature | partial | openblack locks the camera onto the creature instead (Creature Mode, see [follow_cameras.md](follow_cameras.md)); how the game's key behaves exactly is unconfirmed |

## Bookmarks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ctrl with a number key drops a bookmark where the hand is | partial | `src/Game.cpp`, `CameraBookmarkSystem::SetBookmark`; eight bookmarks |
| A number key flies the camera back to its bookmark, looking from where it was when dropped | partial | `src/Game.cpp`; not compared with the game |
| Bookmarks show on the land as markers | partial | `CameraBookmarkSystem::Update` pulses a placeholder marker; the game's look not matched |
| Bookmarks can be cleared | todo | `ClearBookmark` exists but nothing calls it |
| Keys step to the next and previous bookmark | todo | not in openblack (unconfirmed key) |
| A key flies the camera to the player's own town | todo | not in openblack (unconfirmed key) |
| A bookmark on a moving thing follows it | todo | not in openblack (unconfirmed) |
| The camera can go back to where it was before jumping to a bookmark | todo | not in openblack |
| Bookmarks are kept in saved games | todo | no saved games; see `../engine/` |

## Start of a land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script sets where the camera starts, looking towards the player's temple | done | `START_CAMERA_POS` in `src/LHScriptX`; `test/camera/test_set_camera_pos.cpp` (land 1, two and three gods) |
| The start position with four gods | partial | `SetCameraPos.DISABLED_setCameraFourGods` is disabled |
