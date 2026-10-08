# Challenge natives: camera, cut scenes and fades

The challenge scripts' functions for the camera: setting and gliding it, following objects, cut-scene control, the cinema bars, fades, lens, shakes and the challenge log's pictures. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 6/58 done, 4 partial — 14%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Puts the camera's eye at a position at once: *set camera position to ‹position›* (called 307 times in 85 scripts) | partial | `SetCameraPosition`: sets the eye at once; doesn't check that the script holds the camera (cinema mode) |
| Points the camera at a position at once: *set camera focus to ‹position›* (called 290 times in 85 scripts) | partial | `SetCameraFocus`: sets the focus at once; doesn't check that the script holds the camera (cinema mode) |
| Glides the camera's eye to a position over a number of seconds: *move camera position to ‹position› time ‹time›* (called 769 times in 162 scripts) | todo | `MoveCameraPosition` logs "not implemented" |
| Glides the camera's point of view to a position over a number of seconds: *move camera focus to ‹position› time ‹time›* (called 737 times in 161 scripts) | todo | `MoveCameraFocus` logs "not implemented" |
| Gives the camera's current eye position: *camera position* (called 561 times in 171 scripts) | done | `GetCameraPosition`: returns the camera's eye |
| Gives the position the camera is looking at: *camera focus* (called 263 times in 111 scripts) | done | `GetCameraFocus`: returns the camera's focus |
| Whether an object is in the camera's view: *‹object› viewed* (called 53 times in 36 scripts) | todo | `GameThingFieldOfView` logs "not implemented" |
| Whether a position is in the camera's view: *‹position› viewed* (called 54 times in 30 scripts) | todo | `PosFieldOfView` logs "not implemented" |
| Takes the camera from the player at the start of a cut scene (part of the language's cinema and camera blocks): *begin cinema / begin camera (opening the block)* (called 328 times in 181 scripts) | todo | `StartCameraControl` logs "not implemented" |
| Gives the camera back to the player at the end of a cut scene: *end cinema / end camera (closing the block)* (called 450 times in 238 scripts) | todo | `EndCameraControl` logs "not implemented" |
| Brings the black cinema bars in or out: *the cinema block's bars (opening and closing it)* (called 754 times in 235 scripts) | partial | `SetWidescreen`: cinema bars and task ownership through `CinematicDirectorSystem`; snapping the bars for videos and picking inside them todo (port notes) |
| Whether the camera has finished a glide: *camera ready* (called 493 times in 153 scripts) | todo | `HasCameraArrived` logs "not implemented" |
| Keeps the camera's point of view on a moving object: *move camera focus follow ‹target›* (called 4 times in 3 scripts) | todo | `FocusFollow` logs "not implemented" |
| Takes a picture of the camera's view for the challenge log in the temple: *snapshot (a challenge log picture)* (called 137 times in 76 scripts) | todo | `Snapshot` logs "not implemented" |
| Gives one of the camera positions placed in the landscape by name: *camera position named in the landscape* (called 13 times in 11 scripts) | todo | `ConvertCameraPosition` logs "not implemented" |
| Gives one of the camera focus positions placed in the landscape by name: *camera ‹camera enum›* (called 19 times in 13 scripts) | todo | `ConvertCameraFocus` logs "not implemented" |
| Starts a camera that keeps two objects in view: *begin dual camera to [object] and [object]* (called 6 times in 5 scripts) | todo | `StartDualCamera` logs "not implemented" |
| Stops the two-object camera: *end dual camera* (called 6 times in 5 scripts) | todo | `ReleaseDualCamera` logs "not implemented" |
| Glides the camera in front of an object, at a distance, over a time: *move camera to face ‹target› distance ‹distance› time ‹time›* (called 14 times in 7 scripts) | todo | `MoveCameraToFaceObject` logs "not implemented" |
| Flies the camera along one of the camera paths made for the land: *camera path ‹camera enum›* (called 12 times in 11 scripts) | todo | `RunCameraPath` logs "not implemented" |
| Whether the cinema bars have finished moving: *widescreen ready* (called 17 times in 14 scripts) | partial | `WidescreenTransistionFinished`: `CinematicDirectorSystem` |
| Loads a set of camera zones that keep the camera away from places: *set camera zones to ‹filename›* (called 10 times in 3 scripts) | todo | `SetCameraZone` logs "not implemented" |
| Sets the following camera's distance, speed and angle and whether it stays behind: *set camera properties distance ‹distance› speed ‹speed› angle ‹angle› enable/disable behind* (called once in 1 script) | todo | `CameraProperties` logs "not implemented" |
| Shakes the camera, strongest near a position, for a time: *shake camera at ‹position› radius ‹radius› amplitude ‹amplitude› [time ‹duration›]* (called 31 times in 20 scripts) | todo | `ShakeCamera` logs "not implemented" |
| Turns a recorded sequence on an object on or off (unconfirmed): *enable/disable ‹avi sequence› avi sequence* (called 2 times in 2 scripts) | todo | `SetAviSequence` logs "not implemented" |
| Updates the challenge log picture with what is going on: *update snapshot (the challenge log picture)* (called 110 times in 60 scripts) | todo | `UpdateSnapshot` logs "not implemented" |
| Fades the screen to a colour over a time: *set fade red ‹red› green ‹green› blue ‹blue› time ‹time›* (called 116 times in 48 scripts) | done | `SetFade`: `ScriptFade` tests |
| Fades the screen back in over a time: *set fade in time ‹duration›* (called 110 times in 47 scripts) | done | `SetFadeIn`: `ScriptFade` tests |
| Whether the fade has finished: *fade ready* (called 171 times in 38 scripts) | done | `FadeFinished`: `ScriptFade` tests |
| Keeps the camera looking at an object as it moves: *set camera focus follow ‹target›* (called 32 times in 27 scripts) | todo | `SetFocusFollow` logs "not implemented" |
| Keeps the camera's eye moving with an object: *set camera position follow ‹target›* (called 5 times in 5 scripts) | todo | `SetPositionFollow` logs "not implemented" |
| Keeps the camera following an object at a distance: *set camera follow ‹target› distance ‹distance›* (called 2 times in 2 scripts) | todo | `SetFocusAndPositionFollow` logs "not implemented" |
| Puts the camera's lens back to normal: *reset camera lens [time ‹lens›]* (called once in 1 script) | todo | `SetCameraLens` logs "not implemented" |
| Changes the camera's lens (field of view) over a time: *set camera lens ‹lens› [time ‹time›]* (called 25 times in 7 scripts) | todo | `MoveCameraLens` logs "not implemented" |
| Whether an object can see the camera within an angle: *‹object› can view camera in ‹degrees› degrees* (called 4 times in 3 scripts) | todo | `GameThingCanViewCamera` logs "not implemented" |
| Updates the picture of a challenge log entry: *update snapshot picture* (called 13 times in 4 scripts) | todo | `UpdateSnapshotPicture` logs "not implemented" |
| Gives a position a distance in front of the camera: *facing camera position distance ‹distance›* (called 7 times in 4 scripts) | todo | `GetFacingCameraPosition` logs "not implemented" |
| Whether the camera is within a turn (unconfirmed): *within rotation* (called once in 1 script) | todo | `WithinRotation` logs "not implemented" |
| Narrows the screen to a window over a time: *set clipping window across ‹across› down ‹down› width ‹width› height ‹height› time ‹time›* (called 2 times in 1 script) | todo | `SetClippingWindow` logs "not implemented" |
| Widens the screen back from the window over a time: *clear clipping window time ‹time›* (called once in 1 script) | todo | `ClearClippingWindow` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keeps the camera's eye on a moving object: *move camera position follow ‹target›* (not called by the shipped scripts) | todo | `PositionFollow` logs "not implemented" |
| Changes the two objects the two-object camera keeps in view: *set dual camera to ‹obj1› ‹obj2›* (not called by the shipped scripts) | todo | `UpdateDualCamera` logs "not implemented" |
| Starts the two-object camera on an object and a fixed point: *dual camera with a point (no statement in the language)* (not called by the shipped scripts) | todo | `CreateDualCameraWithPoint` logs "not implemented" |
| Puts the camera in front of an object, at a distance, at once: *set camera to face ‹value› distance ‹value›* (not called by the shipped scripts) | todo | `SetCameraToFaceObject` logs "not implemented" |
| Gives how far away things are still drawn (unconfirmed): *get inclusion distance* (not called by the shipped scripts) | todo | `GetInclusionDistance` logs "not implemented" |
| Keeps the camera following an object at a distance: *move camera follow ‹value› distance ‹value›* (not called by the shipped scripts) | todo | `FocusAndPositionFollow` logs "not implemented" |
| Locks the camera's turning about a point: *enable/disable fixed camera rotation at ‹value›* (not called by the shipped scripts) | todo | `SetFixedCamRotation` logs "not implemented" |
| Remembers where the camera is: *store camera details* (not called by the shipped scripts) | todo | `StoreCameraDetails` logs "not implemented" |
| Puts the camera back where it was remembered: *restore camera details* (not called by the shipped scripts) | todo | `RestoreCameraDetails` logs "not implemented" |
| Sets the camera's eye, view and lens at once: *set camera position ‹value› focus ‹value› lens ‹value›* (not called by the shipped scripts) | todo | `SetCameraPosFocLens` logs "not implemented" |
| Glides the camera's eye, view and lens over a time: *move camera position ‹value› focus ‹value› lens ‹value› time ‹value›* (not called by the shipped scripts) | todo | `MoveCameraPosFocLens` logs "not implemented" |
| Lets the camera clip close to things, or puts it back: *enable/disable clipping distance [‹value›]* (not called by the shipped scripts) | done | `SetGraphicsClipping`: port notes: near plane through the cinematic director |
| Gives the remembered camera eye: *stored camera position* (not called by the shipped scripts) | todo | `GetStoredCameraPosition` logs "not implemented" |
| Gives the remembered camera view: *stored camera focus* (not called by the shipped scripts) | todo | `GetStoredCameraFocus` logs "not implemented" |
| Turns drawing the sun on or off: *draw the sun on/off (no statement in the language)* (not called by the shipped scripts) | todo | `SetSunDraw` logs "not implemented" |
| Sets one of the player's camera bookmarks to a position: *set bookmark ‹value› to ‹value›* (not called by the shipped scripts) | todo | `SetBookmarkPosition` logs "not implemented" |
| Keeps the camera looking at a rival god's hand: *set camera focus follow computer player ‹player›* (not called by the shipped scripts) | todo | `SetFocusFollowComputerPlayer` logs "not implemented" |
| Keeps the camera's eye moving with a rival god's hand: *set camera position follow computer player ‹player›* (not called by the shipped scripts) | todo | `SetPositionFollowComputerPlayer` logs "not implemented" |
