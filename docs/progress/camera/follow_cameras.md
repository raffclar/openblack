# Following cameras

Cameras that lock onto something and follow it: the creature, the creature's fights, and the other following cameras
the game has. Script-driven following is in [script_camera.md](script_camera.md).

**Progress: 10/18 done, 2 partial — 61%**

## Following the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A key locks the camera onto the player's creature and lets go again | done | `CreatureModeSystem`, `src/Camera/CreatureCameraModel.cpp`; `CreatureMode.CreatureKeyLocksOntoYourCreatureAndLetsGo` (the game's key flies to the creature; that the follow is reached the same way is unconfirmed) |
| A double click on a creature locks onto it, another god's too | done | `CreatureMode.DoubleClickIsTwoQuickPressesOnTheSameCreature` |
| The camera starts the creature's viewing distance away, or as close as it already is, looking at its middle | done | `src/Camera/CreatureFollow.cpp`; `CreatureFollow.StartsAtTheViewingDistanceFromAfar`, `StaysAboutAsCloseWhenAlreadyClose`, `LooksAtTheMiddleOfTheCreature` |
| It eases after the creature, arriving two seconds later at first and one second once settled | done | `CreatureFollow.EasesInTwoSecondsAtFirstAndOneLater` |
| Shift and the arrow keys turn it round the creature and tilt it | done | `CreatureFollow.ShiftAndCursorKeysTurnAndTilt` |
| Ctrl and the arrow keys turn it and draw it in and out; the wheel zooms | done | `CreatureFollow.CtrlAndCursorKeysTurnAndZoom`, `TheWheelZoomsTwice` |
| Its distance and pitch stay within bounds | done | `CreatureFollow.KeepsWithinItsBounds` |
| Ctrl and Shift together swing it to where the land falls away, for a clear view | done | `CreatureFollow.ClearViewSwingsAwayFromAHill`, `ClearViewTiltsTowardsTwentyTwoDegrees` |
| The arrow keys alone, gripping the land, the temple, a script's cinema bars or the creature leaving give the camera back | done | `CreatureFollow.CursorKeysAloneGiveTheCameraBack`; `CreatureModeSystem` |
| The camera keeps a thing in view smoothly unless it turns too fast to follow | todo | not in openblack (unconfirmed rule) |

## Fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player's creature starts a fight the camera flies to watch it from the side of the arena | done | `CreatureFightSystem` with `creature_fight::CameraOrigin`, `CameraSide` |
| The camera stays on the arena during the fight | partial | openblack follows the fighters once they move 0.3 of the arena's radius away; the game stays on the arena (`src/Creature/CreatureFight.h`) |
| Clicking near another creature fight takes the camera to watch it | partial | the camera feature exists (`camera_help::DuringFight`); the click itself not confirmed in openblack |
| The fight's camera shows text and can be left, ending the fight soon after | todo | not in openblack |
| During a fight moving the mouse always turns the camera | todo | `TODO(#710)` in `DefaultWorldCameraModel.cpp` |

## Other following cameras

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A camera that follows behind a thing, keeping to its heading | todo | not in openblack |
| A camera that watches a worship dance | todo | not in openblack (unconfirmed when the game uses it) |
| The camera that zooms in to the temple from outside as the player interacts with it | todo | entering the temple is a cut in openblack (`src/Game.cpp`); see [temple_camera.md](temple_camera.md) |
| The editor's orbit and follow cameras | n/a | openblack-only, `src/Camera/EditorCameraModel.cpp`; see `../debug/` |
