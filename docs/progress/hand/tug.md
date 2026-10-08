# Pulling things free

Grabbing a standing tree doesn't lift it at once: the hand pulls at it, the tree leans and stretches towards the hand
like a spring, and it only comes free once the pull is strong enough for its weight. Too heavy for the hand's strength,
it never comes free. Everything else the hand can take comes free as soon as the hand has faded into its pulling pose.

openblack (`physics` branch): the pull is in `HandGrabSystem` (`StartPull`, `Pull`) with the game's rules in
`hand_grab::PullAt` (`src/Hand/HandGrabRules.cpp`); the uprooted tree leaves its roots hole (`GameHandGrabWorld`).
Tests: `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree`, `HandGrab.APulledTreeLeansAndStretchesTowardsTheHand`,
`HandGrabSystemWithWorld.AHeavyTreeIsNeverPulledFree`, `HandGrabSystemWithWorld.ALightTreeIsUprootedAndCreaks`.

**Progress: 22/22 done, 0 partial — 100%**

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only standing trees are really pulled; big forests, fields, piles and fireballs wait for the press to become a hold instead, and anything else comes free once the hand's pulling pose has faded in | done | `HandGrabSystem::Press`, `hand_grab::PullAt` |
| The hand takes hold at the point it gripped, measured from the thing's base on the land | done | `HandGrabSystem::StartPull` |
| The slope of the land under the thing is taken as the plane the pull works in, set at a height found from the camera's ground distances to the base and to the hand | done | Plane from the land normal, its height from the camera's ground distances (`HandGrabSystem` pull, `src/Hand/HandGrabRules.h`) |
| The pull only starts once the hand's 0.13-second fade into the pulling pose is done | done | `hand_grab::k_PullBlendSeconds` in `HandGrabSystem::Pull` |
| The hand's hold distance is the thing's hold lowering times its height | done | `StartPull` (the 0.3 × hand-size minimum the game computes is overwritten at once, so it never shows) |
| The hand holds it with the pose its kind is held with (palm, side, tree or villager grip) | done | While pulling, the hand takes the thing's hold pose with the 0.13 s fade, opening by the tug's own rule (`HandGrabSystem::GetPullPose`, `hand_hold::TugTimeMs`, `aa7b8b9a`, `6afe6217`) |

## Pulling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's point follows the cursor across the plane of the land under the thing | done | `HandGrabSystem` (cursor ray on the pull plane) |
| The pull is a spring of 1000 per unit between where the hand is and where it gripped the thing | done | `hand_grab::PullAt` |
| The pull is capped at the hand's greatest force | done | `hand_grab::MaxForce`; see [hand_physics.md](hand_physics.md) |
| The pull turns the thing about its base, with a heaviness of 1000 against turning and a damping of 4000 times its turning speed | done | `PullAt`; test `HandGrab.APulledTreeLeansAndStretchesTowardsTheHand` |
| As it is pulled the thing stretches along its height, up to 1.3 times, easing over 0.3 seconds | done | `HandGrab::stretch` |
| Things of the rock material don't lean while pulled | done | `PullAt` (only trees are pulled, so it never shows) |
| The thing is drawn leaning and stretched, with the hand on it | done | The tree is drawn leaning and stretched (its pose is written each frame) and the hand takes its pull pose (`aa7b8b9a`) |
| The pull gives the thing a velocity, kept for when it comes free | done | `HandGrab` |

## Coming free

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree comes free once the hand's greatest force is at least its weight (its mass times 9.81) and the pull beats its mass | done | `hand_grab::PullAt`; test `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree` |
| Something too heavy for the hand's force never comes free | done | test `HandGrabSystemWithWorld.AHeavyTreeIsNeverPulledFree` |
| A tree that comes free is uprooted, leaving its roots hole | done | `GameHandGrabWorld` (roots ground mark); see [../nature/](../nature/); a firefly hiding in it leaves a one-shot seed there ([../nature/fireflies.md](../nature/fireflies.md)) |
| A person or animal comes free at once and is redrawn off the ground | done | Taken once the fade is over (`HandGrabSystem`) |
| Things that aren't trees come free at once | done | `PullAt` |
| Once free, the thing is held as anything else | done | See [holding.md](holding.md) |
| Uprooting a tree plays its creak and counts towards the player's alignment | done | test `HandGrabSystemWithWorld.ALightTreeIsUprootedAndCreaks` |
| Letting go before it comes free leaves it rooted, leaning and stretched as it was last drawn, until its next growth step redraws it | done | `HandGrabSystem::Release` (the pull wrote its pose) |
