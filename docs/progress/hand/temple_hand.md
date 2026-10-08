# Temple hand

Inside the player's temple the hand has a state of its own: it hangs on the line of sight through the cursor just short
of the room's walls and floor, turns to face them, and plays its idle animation. The temple's rooms, tooltips and what
the hand does in them are in [../temple/](../temple/).

**Progress: 17/17 done, 0 partial — 100%**

## Placement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand goes into its temple state whenever the player is inside the temple | done | `Game::PlaceHand` (temple branch) |
| Entering, it starts from where it was and eases from there | done | `Game::PlaceHand` (hover zoomer) |
| It hangs on the line of sight through the cursor, 3.25 units short of where the line meets the room | done | `Game.h` (`k_HandTempleGap`) |
| It is never nearer than 1 unit before the limits are applied | done | `Game::PlaceHand` |
| Its distance from the camera is kept between 4 and 300 | done | `Game.h` (`k_HandTempleMinDistance`, `k_HandTempleMaxDistance`) |
| It eases away from the camera over 0.4 seconds and towards it over 0.2 | done | `Game::PlaceHand` |
| It is scaled by its distance from the camera as outside | done | `HandAnimation::ScaleAtDistance` |

## Facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its up turns over 0.4 seconds to the face of the room the cursor is on | done | `src/Game.cpp` (`_handTempleNormal`, `k_HandTempleTurnTime`) |
| It faces along the line of sight through the cursor | done | `Game::OrientHand` |

## Animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays its idle wiggle all the time, looping by the frame's time | done | `src/Game.cpp` (normal cycle) |
| It leans with the trailing cursor, clamped to 80 pixels, unless the lean is tiny | done | `HandAnimation` |
| Entering the temple blends the hand from its last pose | done | `HandCrossFade` |

## Around it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No camera edge hints are offered in the temple | done | `TempleCameraModel` has its own controls; see [../camera/temple_camera.md](../camera/temple_camera.md) |
| The hand's glow never lies on water in the temple | done | test `HandWaterGlow.NeverShowsInsideTheTemple` |
| The miracles don't hear the hand in the temple | done | `Game::UpdateMagicHand` (overWorld) |
| The temple places the hand's tooltips itself | done | `Game::UpdateHandInterface`; see [../temple/](../temple/) |
| Clicking doors, scrolls, the creature cave and the map is handled by the temple | done | see [../temple/](../temple/) |
