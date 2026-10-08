# Hand animations

Every animation of the god hand and when the game plays it. The animations are in the hand's animation file, named by the
hand spec: each cycle is followed by two lean ranges (sideways, and back and forth) that the trailing cursor blends in.
Some slots in the spec are spare and never used.

**Progress: 19/35 done, 6 partial — 63%**

## How they play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The animations are loaded from the hand's animation file, laid out by the hand spec | done | `Game::LoadHandAnimation`, `src/3D/HandAnimation.cpp`; test `HandAnimationTest.SpecSlotsHoldTheExpectedAnimations` |
| Looping cycles run on by the frame's milliseconds, wrapping at their length | done | `HandAnimation::Update` |
| Each state keeps its own cycle time; the camera state's restarts whenever it is entered | done | `HandAnimation` |
| A cycle's sideways lean range is blended in by the trailing cursor's sideways gap, clamped to 80 pixels | done | test `HandAnimationTest.CursorLagLeansTheHand` |
| Its back-and-forth lean range is blended in by the vertical gap | done | `HandAnimation::ApplyLean` |
| A lean smaller than a ten-thousandth is left out | done | `HandAnimation` (minimum lean) |
| Moving the camera, the sideways lean goes the other way | done | `HandAnimation` (camera state) |
| Changing animation blends from the last pose over 0.13 seconds | done | test `HandAnimationTest.GrippingCrossFadesToTheGripCycle` |
| Holding something takes a still frame of a hold, picked by how big the thing is for the hand, without leaning | partial | done for miracle seeds (`HandAnimation::UpdateHeld`); nothing else can be held |
| A right hand plays the same animations on the mirrored mesh | done | `src/Game.cpp` (scale) |
| The bones keep their lengths, and the palm stays at rest in the standing pose | done | tests `HandAnimationTest.PosesStayRigid`, `HandAnimationTest.StandPoseKeepsThePalmAtRest` |

## The standard set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wiggle: the idle fingers, hovering over land, sea or sky | done | `src/Game.cpp` |
| Wiggle in the temple, all the time | done | see [temple_hand.md](temple_hand.md) |
| Wiggle while holding a miracle whose hold is the idle hand | done | `HandHoldPose.cpp` |
| Wiggle while held to a creature but not touching its body | done | `src/Game.cpp` (creature cycles) |
| Point | todo | loaded but never chosen (unconfirmed when the game plays it) |
| Hold from above: holding things and miracles of that hold | partial | miracles only |
| Hold from the side: holding things and miracles of the three side holds, and holding a totem | partial | miracles only; see [totem.md](totem.md) |
| Can pick up, fingers open: over a miracle's worship icon | todo | openblack keeps the wiggle |
| Hold fingers: over a leash, held still at its first frame, or half way through while the leash is being taken | todo | openblack keeps the wiggle |
| The two spare slots of the standard set | n/a | empty in the game's file |

## On a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroke: the hand's feeling cycle, played hovering over any object | todo | openblack plays the stroke cycle on the creature's body instead |
| Tickle: the caress played while the hand rests on a creature's body, the cycle the creature state starts in | partial | openblack plays the stroke cycle there |
| The caress only plays once the hand has rested on the body a second | partial | openblack's own timing (`CreatureFeedback.StrokesNeedANewPartAndTime`) |
| Slap: played once the hand sweeps across the creature faster than 3.25 times the creature's size a second | partial | openblack slaps by its own speed classes (test `CreatureFeedback.SlapsAreClassedByHeightAndSpeed`); unconfirmed they match |
| Punch | todo | loaded but never chosen (unconfirmed when the game plays it) |
| The spare slots of the reward and punishment sets | n/a | empty in the game's file |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grip: gripping and panning the land | done | see [navigation.md](navigation.md) |
| Rotate: turning by the edge, and offered at the sides and bottom | done | `src/3D/HandNavigationPose.h` |
| Pitch: tilting, and offered at the top | done | `HandNavigationPose` |
| Zoom: only reachable through a camera hint the game never sets, so in effect unused | done | `HandNavigationPose` |
| The spare slot of the camera set | n/a | empty in the game's file |

## Set animations

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tap house: played once where the hand is when it knocks on a house's roof, then the hand goes back to its state | todo | see [clicking_and_activating.md](clicking_and_activating.md) |
| Knocking also plays one of nine knocking sounds in turn | todo | see [hand_sounds.md](hand_sounds.md) |
| While a set animation plays, the hand stays upright at the place it started | todo | |
| Beckon | todo | loaded but never chosen (unconfirmed when the game plays it) |

## Special holds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Horn: the horn-shaped hold of some miracles | done | `HandHoldPose.cpp` |
| The hold the spec lists after the horn | todo | loaded but never chosen (unconfirmed what the game uses it for) |
| The spare slot of the special holds | n/a | empty in the game's file |
