# Script camera (GCamera and CameraModeScript)

How the script moves the camera in runblack.exe v1.42 (W120) and what openblack has: the GCamera zoomers, the script
camera mode, the arrival rule, the CHL camera opcodes, the FOV and the hand-over when releasing control.
The `camera.edt` tracks are in [camera-tracks.md](camera-tracks.md); the START/END_CAMERA_CONTROL locks, in
[audio.md](audio.md) (`Help/ScriptControl`).

- [GCamera and its zoomers](#gcamera-and-its-zoomers)
- [The script mode](#the-script-mode)
- [Arrival rule](#arrival-rule)
- [One frame of GCamera::Update](#one-frame-of-gcameraupdate)
  - [The lens](#the-lens)
- [Following](#following)
- [Dual camera](#dual-camera)
- [Shake](#shake)
- [Zones and fixed rotation](#zones-and-fixed-rotation)
  - [Player camera features (CameraHelp)](#player-camera-features-camerahelp)
- [Opcodes](#opcodes)
- [Releasing the camera](#releasing-the-camera)
- [openblack](#openblack)
- [Test hooks](#test-hooks)
- [Pending](#pending)

## GCamera and its zoomers

**Faithful.** GCamera (0x1D8 bytes) stores the camera in LH3DLib Zoomers (`SetDestinationWithSpeedAndTime` 0x407D60,
`Update` 0x442720; [engine-math.md](engine-math.md#zoomer-lh3dlib), `src/Common/Zoomer.h`): the focus at +0x88/+0xB8/+0xE8,
the position at +0x118/+0x148/+0x178 and the FOV (radians) at +0x1A8. A MOVE starts from the current value and speed and
arrives with speed 0 at T; T < 0.001 places it immediately; when T ends the value is pinned to the destination. The mode
stack is at +0x28 (12 modes, 0x441CEA) with the index at +0x58; the player uses `CameraModeNew3` (vtable 0x8C7BFC), which
always allows exiting.

- The GCamera ctor sets the FOV to 70 degrees at once (0x441A78..0x441A83): `SetCameraFov(fn_00443670, 0)`, where
  fn_00443670 = fld [0x8C762C] = 1.2217305.
- GCamera +0x68 = seconds since the last mode change: 0 in `SwitchToViewMode` 0x441CD0, plus the frame's camera seconds
  at 0x441FCD (before the mode's Update), 2 after fn_0044BB30 (0x44C141).
- The script opcodes find the current mode with `__RTDynamicCast` to 0x9CE188 (CameraModeScript); the dual opcodes use
  0x9CE790 (CameraModeTwoObjects; 0x6ED3F0 / 0x6ED43D).
- `GCamera::SetCameraFov` 0x443680: with t == 0 or t < 0.001 it sets the value; otherwise it moves towards the FOV with
  final speed 0 (an inline copy of 0x407D60, 0x4436CB..0x4437D7).
- Degrees to radians is [0x92B20C] = 0.0174532924 (SET_CAMERA_LENS, MOVE_CAMERA_LENS, CAMERA_PROPERTIES 0x6EE025).

## The script mode

**Faithful.** START_CAMERA_CONTROL (0x6ECCA0), outside the citadel, calls `fn_00461140`: if `CantExitCurrentMode`
0x441B70 there is no mode and the control fails; otherwise, `CameraModeScript` (ctor 0x461180, vtable 0x8C7D5C, inherits
from `CameraModeFollow`). While it lives, `CanExit` 0x461B70 = 0: **no other task takes the camera**. It does not touch
the zoomers. `SetCameraPosition/Focus` 0x461370/0x4612B0 set them and `MoveCameraPosition/Focus` 0x4616F0/0x461430 move
them (with final speed 0); all four first release the track and the following. `RunPath` fn_00461A80 loads `Track%d` and
`UpdatePath` 0x461AB0 advances with the game ms of the frame: position = `CameraWayRunner::Get`, focus = Bezier of the
focus way in the segment and the t of the **position** runner, and `SetPositionAndFocus` 0x4438C0 sets them.

- `fn_00461140` is called from START_CAMERA_CONTROL at 0x6ECCBA; the citadel never has a script mode (StartCameraControl
  0x6ECD33).
- CameraModeScript is a CameraModeFollow built with no thing (ctor 0x44B800): time factor 0.2 (0x461189), «behind» 1
  (0x461185..0x461193), heading 0 (from `Set(0)`, 0x44BA7D), and mode seconds from 0 (`SwitchToViewMode` from 0x44B947).
- (inferred) The ctor 0x44B800 calls `Set(0)`, which writes neither pitch nor distance; they are never read before Set,
  fn_0044BA90 or CAMERA_PROPERTIES write them.
- CameraModeScript +0x48 = alive: 1 from the ctor 0x461180, 0 from `Delete` 0x4611E0. `CanExit` 0x461B70 = (+0x48 == 0).
  `IsStillValid` 0x4611D0 = +0x48.
- `CameraModeScript::Update` 0x461290: `UpdatePath`, then `CameraModeFollow::Update` 0x44C160 (call at 0x4612A1).
- `SetCameraPosition` 0x461370 calls `Set(0)` (0x46137E) and sets +0x54 = −1. `SetCameraFocus` 0x4612B0 calls
  `SetCameraFocus(0)` 0x4619B0 (0x4612BE) and sets +0x50 = −1. `MoveCameraPosition` releases at 0x461702 and
  `MoveCameraFocus` at 0x461442.
- Dropping the track is fn_00461A60: it frees the ScriptedCamera (fn_00446AC0) and sets +0x58 = 0.
- `RunPath` fn_00461A80 first calls `Reset` 0x461A30: +0x4C = 0, +0x48 = 1, the old ScriptedCamera freed, +0x50 = +0x54 =
  −1, +0x08 kept. Then it starts track `number` from the beginning.

## Arrival rule

**Faithful.** `GCamera::Arrived` 0x443050: with no mode, 1; otherwise, vt+0x34 of the mode. `CameraModeScript::Arrived`
0x461B40: with a track, duration ≤ ms travelled; without a track, `CameraMode::Arrived` 0x441700: |position − destination|²
< 0.001 **and** |focus − destination|² < 0.001 ([0x8AA3B0]). `CameraModeNew3` also uses 0x441700 (its vtable +0x34). It
does not look at the FOV.

## One frame of GCamera::Update

**Faithful** (0x441F80, from `ProcessGraphicsEngine` 0x54D879, per frame):
1. dt = `GetCameraTimeInc` 0x555820 · 0.001 [0x8AC418] = wall-clock ms (`g_delta_time`), capped at 0.1 s [0x8AB22C].
2. `Update` of the mode (vt+0x08; Script: `UpdatePath` and the following `CameraModeFollow::Update` 0x44C160).
3. The 6 zoomers with dt; NaN → the last good one (0x4420D9).
4. Position and focus almost equal (< 0.001) → position x − 1, y + 1, only in what is drawn (0x4421D5).
5. World disc: if the position's **destination** is more than 3500 from (2560, 0, 2560), new destination
   d / (|d| · 0.000285796) + centre in 3 s (0x44222C).
6. Citadel: what is drawn does not move (0x442337). Otherwise, what is drawn stays 1 m above the ground, also raising the
   focus (0x44242B).
7. FOV: its zoomer with `g_game_time_inc` · 0.001 (**game time**, not wall-clock) and `LH3DTech::ChangeFov` 0x8195B0.

- The world disc constants: centre [0x8C7618] = 2560, squared limit [0x8C7614] = 1.225e7 (3500²), scale [0x8C7610] =
  0.000285796, time 3 s (0x40400000).
- **(approximate)** The world disc in openblack (`Camera::UpdateZoomers`) is skipped while the camera's model has a lens
  of its own (`CameraModel::GetLens`), which only the temple's has: our temple interior sits at the origin, about 3620
  from the centre, outside the disc. The original clamps in the citadel too (0x44222C comes before the citadel test at
  0x442337), so its interior is not where ours is; where the original puts it has not been read. Test:
  `CameraSphere.ACameraModelWithALensOfItsOwnStaysOffTheWorldDisc` (test_temple_camera).
- The 1 m ground clearance is [0x8AA390] (0x44242B..0x4424AB). It is measured under the nudged position. It is skipped
  in CameraModeFree and when GCamera+0x78 is set.
- NaN in the zoomers: the last good drawn camera is kept in the statics 0xC59B48 (position) and 0xC59B38 (focus), first
  (1000, 0, 1000) (0x4420D9..0x4421D5).
- The FOV goes to `ChangeFov` at 0x4425C3, and not inside the citadel (0x4424F6).
- `LH3DTech::UpdateCamera` 0x819920 is called from `GCamera::Update` 0x442622, outside the citadel and not during a
  playback.

### The lens

- `ChangeFov` 0x8195B0 / `UpdateViewPort` 0x81909C: T = tan(fov / 2) (0x8195B8..0x8195D7), fx [0xE83A00] = 1 / T, fy
  [0xE83A04] = aspect / T, with aspect [0xE839EC] = W / H.
- `Get3DPointFromScreen` 0x81B370 works with the depth along the camera's forward axis, not with a ray length. It uses
  [0xC3812C] = near·T and [0xC38130] = near·T / aspect.
- In camera space: ((x − hW)·near·T / hW, (hH − y)·near·T / aspect / hH) × depth / near, z = depth. Then the camera's
  rotation and g_camera take it to the world: eye + right·x + up·y + forward·z (0x81B3BE..0x81B43A).
- Projection of `LH3DSprite::Draw` (0x840930..0x8409D0) / `ProjectPoint` 0x819390, without clipping: sx = (X / Z + 1)·hW,
  sy = hH − Y / Z·hH, with X = fx·x and Y = fy·y.

## Following

**Faithful** except where marked (`CameraModeFollow`, from which the script mode inherits).
- Fields: +0x08 thing followed by the position, +0x4C (Script) thing followed by the focus (`GetFocusThing` 0x4611F0 =
  +0x4C or, if null, +0x08), +0x0C heading, +0x10 pitch, +0x14 distance, +0x18 time factor (0.2 in Script), +0x1C
  «behind» (1 in Script).
- `Set(cosa)` 0x44BA00: heading and pitch of the zoomers' **destinations** (`GetHeadingAndPitchFromPoints` 0x4428D0),
  distance = height · 8 (`GetThingViewingDistance` 0x441F20); with «behind», heading 0. `fn_0044BA90(cosa, d)`: the same
  with distance d and without setting the heading to 0. `fn_0044BB30` places immediately (Zoomer::SetPosition) and sets
  GCamera+0x68 = 2.
- `CameraModeFollow::Update` 0x44C160, per frame: distance clamped to 2..1500 and stored; T = (+0x68 > 2 ? 1 :
  2 − +0x68 / 2) · factor (0x44C1A5: 0.4 s after the mode change, 0.2 s after 2 s; factor 0 → place); focus towards the
  thing's point (MapCoords: x, z / 6553.6, y = ground + altitude +0x1C; in `Update` the Game3DObject's translation if
  it has one; plus half the height; flock: `Flock::GetFlockPos` 0x530570 with the leader's half height); position =
  `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from that point with the distance, pitch ≥ 0.241661 (stored) and
  the heading, which with «behind» on a MobileWallHug is heading − (object's angle − π/2) (0x44C785).
- `Validate` (0x461270 + 0x44BB10, **once per turn** from `GGame::ProcessTurn` 0x54E74E): releases the thing that is no
  longer there.
- Set/Move of a point release the following on its side (0x461370 `Set(0)`, 0x4612B0 `SetCameraFocus(0)`);
  `RunPath` releases only +0x4C.
- Face of an object (`fn_006ED710`, 106/107): focus = MapCoords point + half height; position at distance d with the
  heading `GetFacingDirection` (vt+0x4EC; normalised to ≤ 2π) and pitch 0.1.
- FollowUs: `SET_FOCUS_AND_POSITION_FOLLOW(Son, 3)` and `CAMERA_PROPERTIES(3, 0, 22.5, true)`: the camera sticks to the
  child, 22.5° relative to where it is facing.
- Constants: distance kept in 2 ([0x8C785C]) .. 1500 ([0x8C78E4]): ≤ 2 → 2, < 1500 → itself, else 1500
  (0x44C16B..0x44C1A2). Pitch at least [0x8C78E0] = 0.241661 (0x44C758..0x44C778, 0x44BF7E), stored back.
- T constants: the threshold 2 s [0x8C7874], 1 [0x8C7870], 2 [0x8C786C]; "≤ 2" takes the slow start (0x44C1A5..0x44C1E1).
- `GetThingViewingDistance` 0x441F20 = GetHeight (vt +0x42C) × 8 ([0x8C7108]).
- With «behind» on a MobileWallHug the heading correction is GameAngle (+0x5C) × 2 × [0x8C78DC] (π/2048) − [0x8C78D8]
  (π/2) (0x44BFD1 / 0x44C7B1, 0x44C775..0x44C7C1). CAMERA_PROPERTIES writes the script mode at 0x6EE0C1..0x6EE0D9;
  «behind» is tested `!= 0` at 0x44C7E6.
- The thing's point: MapCoords +0x14, x and z × 1/6553.6 ([0x8AA3A4]), y = `LH3DIsland::GetAltitude` 0x803090 + the
  altitude above the land +0x1C (0x44BCD2..0x44BD03).
- In `CameraModeFollow::Update`, an Object with a Game3DObject (+0x40) uses that matrix's position (Game3DObject +0x38)
  instead (0x44C513..0x44C539, 0x44C715..0x44C73B), but fn_0044BB30 does not. For villagers and animals that matrix is
  the one drawn between turns (fn_0051AF00).
- A creature as the focus thing writes 0xCC62E4..0xCC6310 (1 − life, its body's +0x1C / +0x30) in 0x44C1E5..0x44C245,
  read elsewhere. Not ported.
- Without a position thing, `Update` follows the computer player: `GetComputerPlayerFocus` (vt +0x4C) != −1
  (0x44C59C..) and `GetComputerPlayerFollow` (vt +0x50) != −1 (0x44C905..). fn_0044BB30 has the same branches
  (0x44BD52.., 0x44C056..). Not ported.
- fn_0044BB30 only runs with +0x20 == 0 (0x44BB46). It places with `Zoomer::SetPosition` 0x441AC0 (and an inline copy
  for x) and does not clamp the distance (0x44BF73..0x44C13C).
- `SetCameraFocus(thing)` 0x4619B0 first drops the track (0x4619B4). Then +0x4C = the thing if `IsAvailable` (vt +0x2C)
  is 1, else 0 (0x4619BD..0x4619DC).
- `GameThing::IsAvailable` 0x401810 = !(byte +0xA & 1). Only `SetCameraFocus` (0x4619BD) and `Validate` (0x461273 /
  0x44BB13) read it here.
- `GetFacingDirection` (vt +0x4EC): GameThingWithPos 0x4024B0 = 0; MobileWallHug 0x60C020 =
  ConvertGameAngleToScawenAngle(+0x5C); Creature 0x477EC0 = its LH3DCreature's angle (+0x160 → +0x58 → +0x84) + 2π − 2.5.
- `GetHeadingAndPitchFromPoints` 0x4428D0(a, b): v = a − b. If |v.x| and |v.z| are both below [0x8C7620] = 0.01 (a
  double), heading 0 and pitch 1.5393804 (0x3FC50A6B, 0x442925 / 0x44292B).
- Otherwise heading = π ([0x8C36A0]) − fn_007FAA50(v), as a 24-bit `fsubr` (0x44293F). fn_007FAA50 gives 0 when x² + z²
  ≤ 1e-6, else fn_007FA990(−z, x). Pitch = fn_007FA990(sqrt(x² + z²), y) (0x442950..0x442976).
- `SetPointFromPointDistanceHeadingAndPitch` 0x442810 keeps cos(pitch) as a float and then does each product in order
  (0x442811..0x442856).
- Face of an object (fn_006ED710): the focus is the MapCoords point plus half the height, with no flock and no
  Game3DObject (0x6ED728..0x6ED76F).
- Face heading (0x6ED774..0x6ED7D9): a heading ≥ 8π ([0x942190]) logs "Invalid heading" (0xC0C208) and carries on.
  While it is > 2π ([0x8AB210]), 2π is subtracted. Pitch 0.1 (0x6ED7D0).
- With no object, fn_006ED710 logs "no object to face" (0xC0C218) and then reads through the null thing (inferred: it
  would crash) (0x6ED717..0x6ED725).

## Dual camera

**Faithful** except where marked (`CameraModeTwoObjects`, 0x30 bytes, vtable 0x8C7DD0, «Dual Cam»). It is stacked on top of the script mode (ctor 0x461BB0; with a point fn_00461CB0; one
equal to the current one deletes itself).
- `Update` 0x461DE0, per frame: T = (+0x68 > 1.5 ? 1 : 2 − +0x68 / 1.5), **without factor**; A and B = MapCoords of the
  two things (or the point); focus = midpoint raised by the mean height · 0.5; distance = ((separation in x/z + the two
  `Get2DRadius`, 30 if it is not an Object) · factor (1, or 1.2 with a point) + larger height · 1.4; heading = π/4 − the
  direction of B − A; pitch π/8; the zoomers towards those destinations in T.
- 093 START (0x6ED2E0), 094 UPDATE (0x6ED370, `SetObjects` 0x461C90), 095 RELEASE (0x6ED410: `Delete` and `PopViewMode`,
  GCamera+0x68 = 0), 105 WITH POINT (0x6ED460, without checks). The end of control (fn_006ECD70) removes a dual
  before deleting the script mode. With the dual on top, the script opcodes find «the wrong mode».
- Per turn, `CheckStackedModesForValidity` 0x441D40 removes the dual whose things are no longer there (`IsStillValid`
  0x461D90).
- Constants (ctor 0x461BB0 / fn_00461CB0): heading +0x20 = π/4 (0x3F490FDB), pitch +0x24 = π/8 (0x3EC90FDB), distance
  factor +0x28 = 1 (0x461C2C) with two things or 1.2 (0x3F99999A, 0x461D62) with a point. Debug name string at 0x461C60.
- Fields: +0x08 thing A (0x461BC8), +0x0C thing B (0x461BCB), +0x10 the point (0x461CCC..0x461CDF), +0x1C two things (1)
  / point (0) (0x461BCE / 0x461CE2), +0x2C alive (1 at 0x461C25..0x461C41).
- T constants: 2 ([0x8C7DBC]) right after the mode change, down to 1 ([0x8C7DC0]) at 1.5 s ([0x8C7DC4])
  (0x461DED..0x461E26).
- A and B are MapCoords points (x, z × 1/6553.6, GetAltitude 0x803090 + the altitude +0x1C), not the Game3DObject nor a
  flock's GetFlockPos. With a point, B has no +0x1C (0x461E2A..0x461EB2).
- The midpoint is (B + A) × 0.5 ([0x8AA3B4], 0x461EB6..0x461F10). With a point, the second height in the mean is 1
  (0x461F2A).
- The pitch is kept ≥ [0x8C78E0] and stored back (0x462003..0x462021).
- The radius of a thing that is not an Object is 30 (0x46204A, [0x8BF51C]). An Object's comes from `Get2DRadius` (vt
  +0x64) via `dynamic_cast<Object*>` (0x462031 / 0x462067). The larger height is multiplied by 1.4 ([0x8C7E18],
  0x4620B1).
- Heading = +0x20 − the direction of v = B − A (fn_007FAA50, a 24-bit `fsubr`, 0x46211C). It stays +0x20 when |v.x| and
  |v.z| are both ≤ 0.01 ([0x8C7A10], a double; 0x4620E9 / 0x4620FC).
- The position heads for `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from the focus (0x462126..0x462318).
- START_DUAL_CAMERA checks neither the camera mode nor the citadel. START / UPDATE_DUAL_CAMERA pop b then a; either
  missing → "Thing invalid for dual cam" (0xC0C1EC) and nothing.
- `IsStillValid` 0x461D90 = +0x2C while +0x08 (and +0x0C with two things) is still there and `IsAvailable` is 1.
  Otherwise `CheckStackedModesForValidity` deletes it (vt+0 with 1, 0x441D86), walking from the bottom of the stack
  (0x441D57). It is called from `GGame::ProcessTurn` 0x54E743.
- When the current mode is deleted, the new current mode gets Restart (vt +0x10, 0x441DEB). Restart does nothing for
  CameraModeScript (0x44A390) and CameraModeTwoObjects. The mode seconds are not reset there.
- RELEASE: `CameraModeTwoObjects::Delete` 0x461C50 (vt +0x30, called at 0x6ED44D) sets +0x2C = 0. `GCamera::PopViewMode`
  0x441C50 calls Cleanup (vt +0x18, nothing), deletes the mode, lowers the index and sets +0x68 = 0 (0x441C86).
- The end of control (fn_006ECD70) removes only one dual camera (`ReleaseDualCamera` 0x6ED410 at 0x6ECDB1). It deletes
  the script mode only if that mode is current (0x6ECDD0..0x6ECE2E). With another mode current (a second dual camera,
  or the player's), it logs "We are in the wrong camera mode! - excep" (0xC0C14C) and the script mode stays.
- openblack: `script_camera::State::duals`, a layer on top of the script mode (no mode stack: **(approximate)**); the
  dual takes the player's zoomers as `BeginFrom` and, if it goes away with no script underneath, gives them back.

## Shake

**Faithful.** SHAKE_CAMERA 201 (0x6EE0F0) → `PSysGlobal::StartCameraShake` 0x68F400 → `LH3DCameraChecker::Create` 0x821050
(radius, point, amplitude, ms). It is applied by fn_008210C0 from `LH3DTech::UpdateCamera` 0x819920, **only to what is
drawn** (never to the zoomers): the shake closest to the camera, if it is within its radius (no falloff with distance);
amplitude = remaining / total · amplitude; six `Random` 0x81D180 rolls (pos.z, pos.y, pos.x, focus.z, focus.y, focus.x) or
two with «only y». Each drawn frame (fn_00821270) subtracts `g_delta_time` and it is freed on reaching 0.
openblack: `src/Camera/CameraShake.{h,cpp}` (`camera_shake::`, with `graphics::lh3d::Random`) and
`script_camera::ApplyShake`, every frame from `Game.cpp` with the camera being drawn (the script's or the
player's), as a draw-only offset of `Camera::SetDrawOffset`: the zoomers do not shake.

- `LH3DCameraChecker` is 0x24 bytes: `LH3DMem::Alloc(0x24)` zeroed, then put at the head of the list g_first [0xEB99A8]
  (0x82106D..0x821078), newest first.
- Checker fields: +0x04 max distance (0x82107E), +0x08 point (0x821085..0x821095), +0x14 amplitude (0x8210A0), +0x18
  total ms (0x8210A7), +0x1C remaining ms (0x8210AA), +0x20 «only y» (0x8210AD).
- `StartCameraShake` 0x68F400: ms = seconds × 1000 ([0x8AB228]) stored as a float, then `fistp` (0x68F406..0x68F41E). It
  always passes «only y» = 0 (0x68F426).
- fn_008210C0 (called from `UpdateCamera` at 0x819A0B) does nothing with no shakes or with [0xC383B8] == 0 (0x8210CB /
  0x8210D3). That flag is 1 in .data and never written.
- The nearest shake is measured from g_camera ([0xEA1DB8] / [0xEA1DC4]), the camera drawn the frame before. A later
  shake wins only if strictly nearer, so the newest wins ties (0x8210E0..0x821195). The distance is fn_004C2B90
  (subtract) and fn_004A1BA0 = sqrt((z·z + y·y) + x·x) (0x4A1BA8..0x4A1BB8).
- a = remaining / total × amplitude (0x8211A9..0x8211B7). A shake of 0 ms divides 0 by 0 (`fidiv` 0x8211B1) and draws a
  NaN camera until it is freed.
- Roll order: «only y» rolls position y then focus y (0x8211D1..0x8211F0). Otherwise six rolls: position z, y, x, then
  focus z, y, x (0x8211F7..0x821264).
- fn_008210C0 is the real "AdjustCameraPosTarget". bw1-decomp's symbol 0x437E70 with that name is another function.
- The countdown fn_00821270 runs from `LH3DRender::StartFrame` 0x82F270 (sub g_delta_time, `jg` keeps it,
  0x821287..0x8212CE).
- Other shake creators, not ported: `CameraModeNew3::Update` 0x45FC84 (the force field: 100, 1.0, 400 ms),
  `AddSoundToAtom` 0x69DDBD and fn_006E63C0 0x6E6453.
- Debug camera g_camera_mode [0xEA9EC8], read by `UpdateCamera` at 0x81997A before the shake (0x81997A..0x8199DD): 2
  draws the camera from [0xEA1B58] towards [0xEA1B68], 1 only turns the focus to [0xEA1B68], any other value does
  neither. GCamera's zoomers are not touched.
- Writers of [0xEA9EC8]: PLAY_JC_SPECIAL 1 / 2, `Intro::ReleaseAll` 0x5DFCCA and `CleanGameForScriptReboot` 0x6EBC0C (0).
  The GCameraEditor's writers are debug only.

## Zones and fixed rotation

- SET_CAMERA_ZONE 142 (0x6ED890): `ResetExclusionFile(1)` 0x455320 and `LoadExclusionFile` 0x455370 of
  `.\Data\Zones\%s` (segment «cameraexc»: flags, two limits of 500, n force-field points, exclusions),
  force field switched on. **Faithful** the loader and `InsideInclusion` 0x455E20 (`src/Camera/PlayerCameraScript.{h,cpp}`,
  `player_camera::`); the nine `.exc` in `Data\Zones` are read correctly. **Pending:** what the player camera does with it
  (`CameraModeNew3::Update` 0x45F982: repositioning, shake, pulse, drawing the field, influence 0x5CD32F).
  GET_INCLUSION_DISTANCE 150 (0x6ED990) therefore always gives FLT_MAX **(approximate)**.
- SET_FIXED_CAM_ROTATION 209 (0x6EE1A0): only with the player mode; `ForceRotateAboutPoint` 0x457330 stores the point
  (`player_camera::Get().fixedRotation`). **Pending:** that `DefaultWorldCameraModel` rotates around it (0x45AB00,
  0x460135). No map uses it. Without CameraModeNew3 it logs "Wrong camera mode" (0xC0C29C).
- `ResetExclusionFile(id)` 0x455320: `RemoveByID` 0x454A40 (the CameraExclusions with +8 == id); [0x9CE6B0] = 1
  (0x45532F); DrawForceField [0xC5E144] = 0 (0x455339); [0xC5E14C] = [0xC5E148] = 0 (0x45533E / 0x455343); [0x9CE6AC] =
  [0x9CE6A8] = 500 (0x455348 / 0x455352); ForceFieldPointCount = 0 (0x45535C).
- SET_CAMERA_ZONE path ".\Data\Zones\%s" (0xC0C248, 0x6ED8F7), opened with `LHReleasedFile::Open` 0x7BD730.
- After `LoadExclusionFile` (0x6ED92A), SET_CAMERA_ZONE sets [0x9CE6B0] = 1 and DrawForceField = 1 whatever the file
  says (0x6ED936..0x6ED940).
- A zone file that does not open logs "Couldn't load zone file-%s" (0xC0C22C) and the zone stays reset
  (0x6ED956..0x6ED96F).
- `LoadExclusionFile` 0x455370 opens segment "cameraexc" (0x9CE6F4, `LHFile::OpenSegment` 0x455395).
- Read order (0x45539A..0x455466): int32 header, [0x9CE6B0], DrawForceField, [0xC5E14C], [0xC5E148], floats [0x9CE6AC]
  and [0x9CE6A8], the point count and the points (12 bytes each), the record count and the record size.
- The force-field points go to `ForceFieldPoints` [0xC5B130]..ForceFieldPointCount [0xC5E130]: room for 1024 (0x3000 /
  12). Each is an inclusion polygon point (x, z) with a height.
- Records of size 0x28 (0x45546F) become CameraExclusions of that id (fn_00454960, +0 and +8 kept; list [0xC5E160];
  0x4554CF..0x455510). Any other size is read into a buffer and thrown away (0x45547C..0x4554B9).
- Readers of the zone flags (meaning not read): [0x9CE6B0] by `CameraModeNew3::GetAltitude` 0x459C7B; [0xC5E14C],
  [0x9CE6AC], [0xC5E148], [0x9CE6A8] by `CameraModeNew3::Update` 0x45C28C / 0x45C29B / 0x45C2B5 / 0x45C2BC.
- The inclusion distance [0xC5E13C] is FLT_MAX ([0x8C7BB0]) at start (0x4548D0) and in `ResetCameraModeNew3` 0x460B2E.
  `CameraModeNew3::Update` writes 1e10 (0x45FCE1) while there is no force field.
- `InsideInclusion` 0x455E20 only tests when DrawForceField is on (0x455E29..0x455E3F) and the polygon has at least 3
  points (0x455E5E).
- It counts ray crossings from the last point round the polygon: odd = inside (0x45624C..0x456252). Epsilons [0x8C7BD8]
  = 1e-8 (squared distances) and [0x8C79D8] = 1e-4 (a double). The best ray parameters start at 1e20 (0x455EA2 /
  0x455EAF). The nearest polygon point in x/z is the output (0x4561CB..0x45624A).
- `ForceRotateAboutPoint` 0x457330: +0x88 = 1 and +0x7C = the point (0x457338..0x45734F), or +0x88 = 0 for null. `Update`
  copies +0x7C over +0x12C (0x45AB00, 0x460135). `Reinitialise` 0x458B22 (and 0x458D23, 0x45E8D6) clears it.

### Player camera features (CameraHelp)

- `CameraHelp`'s EnabledFeatures [0x9CDD6C]: initial value 0x1BF, also set by ClearMap and
  SET_INTERFACE_INTERACTION(NORMAL). Auto-pitch parameters [0x9CDD64] = π/6 and [0x9CDD68] = 75.
- `CameraHelp::EnableCameraFeatures` 0x447430: features = (features & ~mask) | new. Every SET_INTERFACE_INTERACTION level
  passes mask −1, so the whole set is replaced (auto-pitch included).
- Auto-pitch fn_004473F0(p1, p2, on): EnableCameraFeatures(on ? 0x40 : 0, on ? 0 : 0x40), then stores p1 and p2.
- Writers: SET_INTERFACE_INTERACTION 0x70B220, `GScript::CleanGameForScriptReboot` 0x6EBCAE and `GGame::ClearMap`.
- Readers: `HandStateCamera::Update` (bits 0x1, 0x2, 0x4), `HandStateHolding::Update` (0x1, 0x2),
  `GInterface::CalculateCanSelectLock` (0x1), fn_005D6980 (0x4), fn_005C9D00 (0x2) and
  `CameraModeNew3::GetCameraFeatures` 0x456260 (vt +0x58).
- What `CameraModeNew3::Update` does with the features: without Zoom the zoom input is 0 (0x45C0E4..0x45C0F1); without
  Rotate it skips rotation (0x45C6CC); without Pitch it skips pitch (0x45C809). The double-click flight needs bit 0x10
  (0x45DDA3..0x45DDA8), the land grab bit 0x08 (0x45C943..0x45C948), and the second zoom bit is read at 0x45ADC8.
- The bit names (Zoom, Rotate, Pitch...) are inferred from the SCRIPT_INTERFACE_LEVEL names that set them (consistent
  over the 16 levels at 0x70B7A8).
- Player zoom scale (0x45C104..0x45C1B5): 3 × the height difference between the camera and focus destinations (GCamera
  +0x118 / +0x88 y), at least 60 ([0x9CE65C]), at most 240 ([0x8AB418] × 60) when the camera is below the focus
  (0x45C108..0x45C111) and 2000 ([0x9CE660]) when above.
- Zoom delta = input × 0.0015 ([0x8C7CF8], 0x45C0D5) × scale.
- The player's zoom, rotate and pitch of the frame go to `CameraHelp::CameraHelpCallback` 0x449140
  (0x45C38E..0x45C3AD / 0x45C6A8..0x45C838): help events 25..29 ([intro.md](intro.md#timers-help-events-and-field-of-view)).
- The double-click flight reports at 0x45DE85, or 0x306 DoubleClickObject at 0x45DE1E when it was on an object.
- GConfirmation feeds: fn_00454900 (0x45C7D6) and fn_00454930 (0x45C8B1) get the frame's turn and tilt over the frame's
  seconds, 0 on frames without them (0x46053F / 0x46055C).
- `CameraModeNew3::FindBestAngle` 0x459144 uses `LH3DIsland::GetNormal` 0x803630 and the point + normal (0x459149).

## Opcodes

**Faithful** except where marked. Those that move check the mode: with no mode «Script camera has been removed!»
(0xC0C0CC); another mode «We are in the wrong camera mode!» (0xC0C0EC) and they do nothing; `SET_CAMERA_POSITION`
0x6EC8F0 says nothing. 003 / 004 / 287 also have a "Script moving camera in citadel" note (0xC0C110).

| CHL | Original | What it does |
|---|---|---|
| 001 / 002 SET_CAMERA_POSITION / FOCUS | 0x6EC8F0 / 0x6EC9A0 | sets (without script mode it does nothing) |
| 003 / 004 MOVE_CAMERA_POSITION / FOCUS | 0x6ECAA0 / 0x6ECBA0 | moves in t s of wall-clock time |
| 035 HAS_CAMERA_ARRIVED | 0x6ED170 | arrival rule |
| 119 RUN_CAMERA_PATH | 0x6ED7F0 | camera.edt track |
| 279 SET_CAMERA_LENS | 0x6EE2E0 | **`SetCameraFov(70°, x)`: the argument is the time** (copied) |
| 280 MOVE_CAMERA_LENS | 0x6EE280 | FOV = lens · 0.0174533 in t s of game time |
| 283 / 284 STORE / RESTORE_CAMERA_DETAILS | 0x6EE330 / 0x6EE390 | stores what is drawn / `SetPositionAndFocus` |
| 286 / 287 SET / MOVE_CAMERA_POS_FOC_LENS | 0x6EE3C0 / 0x6EE4B0 | position, focus and FOV, **the lens not converted to radians** (copied; no map uses them) |
| 314 / 315 GET_STORED_CAMERA_POSITION / FOCUS | 0x6EE630 / 0x6EE6A0 | what was stored |
| 377 GET_FACING_CAMERA_POSITION | 0x6EE710 | position + d · forward vector (inferred: unit vector towards the focus) |
| 049 / 276 FOCUS_FOLLOW / SET_FOCUS_FOLLOW | 0x6EDF30 / 0x6EDB40 | the focus follows the thing (0x4619B0) |
| 050 POSITION_FOLLOW | 0x6EDE70 | the position follows the thing (`Set` 0x44BA00) |
| 277 SET_POSITION_FOLLOW | 0x6EDA80 | `Set` + place immediately (fn_0044BB30) |
| 178 / 278 (SET_)FOCUS_AND_POSITION_FOLLOW | 0x6EDDA0 / 0x6ED9B0 | `fn_0044BA90(cosa, d)` (278 also places immediately) |
| 180 CAMERA_PROPERTIES | 0x6EDFF0 | distance, time factor, heading (° · 0.0174533), «behind» |
| 106 / 107 SET / MOVE_CAMERA_TO_FACE_OBJECT | 0x6ED500 / 0x6ED600 | face of an object, set / move in t |
| 203 SET_AVI_SEQUENCE | 0x6FC050 | (approximate) without video: only removes the fade to black (`SetupScreenFadeBackToNormal(0)` 0x6EBB00), as if the video ended instantly |
| 093 / 094 / 095 START / UPDATE / RELEASE_DUAL_CAMERA | 0x6ED2E0 / 0x6ED370 / 0x6ED410 | dual camera |
| 105 CREATE_DUAL_CAMERA_WITH_POINT | 0x6ED460 | dual camera with a point |
| 201 SHAKE_CAMERA | 0x6EE0F0 | shake (only what is drawn) |
| 142 SET_CAMERA_ZONE / 150 GET_INCLUSION_DISTANCE | 0x6ED890 / 0x6ED990 | player camera zone (loaded; its effect, pending) |
| 209 SET_FIXED_CAM_ROTATION | 0x6EE1A0 | player's fixed rotation point (stored; its effect, pending) |

Not ported: the PC player following 372/373 (0x6EDC00 / 0x6EDCD0).

The field-of-view tests (011 / 012, used by the intro, [intro.md](intro.md#timers-help-events-and-field-of-view)):
- 011 GAME_THING_FIELD_OF_VIEW = `GScript::IsGameThingFieldOfView` 0x6F8130. 012 POS_FIELD_OF_VIEW =
  `GScript::IsPosFieldOfView` 0x6F8060.
- Both use the drawn camera's `LH3DTech::g_world_to_clipping` [0xEA9E40], the near plane [0xE839E0] and the screen of
  g_info_transform.
- 012: false inside the temple or with no view (0x6F80ED..0x6F8109), else the point test (0x6F810F).
- 011: false with no thing or inside the temple (0x6F819F..0x6F81B4). For an Object (`dynamic_cast`, 0x6F81BA), its 3D
  object's (+0x40) mesh (vt +0xF8; no mesh → false) bounding sphere goes through fn_0081F1A0 →
  `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80, with the box centre at 0x868CBC. Any other GameThingWithPos uses its
  point (x, GetAltitude + its +0x1C, z) (0x6F81F4..0x6F822B).

## Releasing the camera

**Faithful.** `fn_006ECD70` (END_CAMERA_CONTROL 0x6ECEF0 and the task stop fn_006ECF20): removes the dual camera; if the
mode is the script one it deletes it and creates a `CameraModeNew3`, which starts from the current zoomers (no jump); the
FOV **always** returns to 70° in 0.5 s; then the script state (`Help/ScriptControl`).

## openblack

> **Code rules.** The script camera's state lives in ECS components or Locator services, not in globals; the zone files
> and `camera.edt` load through the resource caches; resources are owned with RAII and standard types; the camera maths
> are tested with fakes in `test/`, never through the locator; comments describe behaviour in plain English, with no
> decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

- `src/Camera/ScriptCamera.{h,cpp}` (`script_camera::`): the position, focus and FOV zoomers, the script mode
  (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame` (steps 1-5 and 7) and
  `DrawnCamera` (steps 3-6). `UpdateCamera` does it every frame from `Game.cpp` and, while the script mode
  drives, the player model (`DefaultWorldCameraModel`) neither moves the camera nor reads keys (Script has no keys,
  0x44C3BD). Positions and foci go in `Zoomer3` (`Common/Zoomer.h`, the same one as the player camera).
- **Hand-over of the zoomers (faithful):** GCamera has a single set of zoomers for all modes. openblack has the
  player's (`Camera::GetOriginZoomer/GetFocusZoomer`) and the script's: `Begin` copies the player's as they are (value,
  speed, destination and time: the script mode continues towards where the player's was going, 0x461180 does not touch
  them) and `End` returns the script's to the camera (`HandBack`), from where the player starts like
  `CameraModeNew3::Initialise` 0x456640. With the hooks `OPENBLACK_CAMERA_LOCK/FLY` the player never released the
  camera and nothing is copied.
- `CHLApi.cpp`: the opcodes of the table; `StartCameraControl` passes `cameraTaken = script_camera::Begin(...)`;
  END_CAMERA_CONTROL and the task stop (Game.cpp) call `script_camera::End`. When loading a map, `Reset`.
- **(approximate)** While the script drives, the player camera carries what is drawn (with the nudge and the metre above
  the ground), not the script zoomers. Without script mode, 035 compares the player's zoomers with their destination (the
  same rule 0x441700). `SetPositionAndFocus` does not have the early exit of 0x4438C0. The game ms of the frame are those
  of the previous frame's clock (the original updates the camera after the turns).
- **(inferred)** 284/286 without script mode also set the player camera.
- The FOV goes to `config.cameraXFov` (degrees) only when its zoomer changes: a player's own FOV lasts until a
  script touches the lens.

## Test hooks

- `test_script_camera`: a single mode, arrival, 0.1 s cap, placing with T < 0.001, disc, nudge and ground, FOV with
  game time and the return to 70° in 0.5 s; `ScriptCameraFollow.*`: T rule, distance and pitch, point from
  distance/heading/pitch, heading and pitch between points, «behind», place immediately, face of an object, things that
  disappear.
- `test_script_camera_dual`: the dual (pace, focus, distance, heading, with point, validity, end of control), the shake
  (radius, decay, rolls), the zone loader and `InsideInclusion`.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` win over the script camera (`Drives()`, not original).
- In game: Land 1, when the tutorial requester's answer does not skip the tutorial, runs FollowUs and
  CreaturesInGlade with the script camera.

## Pending

- 372/373 (following the PC player's hand, `GComputerPlayer::GetHandPos` 0x657FE0).
- SET_AVI_SEQUENCE 203 with video: `PlayFullScreenMovie("data\intro.bik")` 0x54D920 pauses the game and returns it at
  58 s; without Bink in openblack there is no movie nor pause (approximate). Sequence 2 (video of the spell falling).
- (approximate) The creature's angle (LH3DCreature +0x84) does not exist: no heading adjustment and `GetFacingDirection` 0.
  The GameAngle of a villager comes from `WallHug::yAngle` rounded to 2048 per revolution.
- Player camera: shake, zone (repositioning, force field) and fixed rotation.
- (approximate) No mode stack: the dual always goes on top of the script; a dual over the player mode is
  approximated.
- `CameraModeNew3::Reinitialise` 0x4589B0 (how the player takes the camera back), not read.
- Still unread for the debug camera: the focus object [0xEA9ECC] (its +0x38, 0x8199E1..0x819A01) and fn_00819F50's copy
  of the test (0x819FAA, the falling spell's camera).
- CameraHelp's features: "[0x9CDD6C]'s initial value, ClearMap's and SET_INTERFACE_INTERACTION(NORMAL)'s (0x80 / 0x100:
  only here, pending)": the meaning of bits 0x80 / 0x100 is not read.
