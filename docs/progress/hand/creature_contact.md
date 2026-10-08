# Creature contact

The hand's side of touching a creature: taking hold of it, stroking and slapping it, how the hand rests on and turns to
the body, and what the creature is told when the hand lets go. What the creature learns from it is in
[../creature/](../creature/). Giving the creature something to hold or eat is in
[holding.md](holding.md).

**Progress: 27/46 done, 3 partial — 62%**

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button over a creature takes hold of it | done | `CreatureHandSystem`, `src/Creature/CreatureHandRules.h` |
| Any player's creature can be held, not only the player's own, unless it is asleep or frozen; never an ogre | done | `creature_hand::MayHold`; test `test_creature_interaction` |
| A press let go quickly, before stroking or slapping, is a click, which puts the leash on instead | done | `creature_hand::WasClick` |
| The nearest creature along the line of sight is the one taken hold of | done | `CreatureHandSystem::CreatureAlong` |
| Holding a creature can't be done while the hand holds a miracle | done | `MagicSystem::IsHandBusy` |
| Scripts and the testbed can put the hand on a creature and stroke or slap it | done | `CreatureHandSystemInterface` (commanded hold) |
| Taking hold starts the hand facing back towards the camera and settles it over a second | todo | openblack turns the hand to the body at once |
| Taking hold starts the hand with its stroking pose and a clean slate: no part stroked yet, no slap yet | done | `HandOnCreature` starts empty |
| Holding a creature zooms the camera in on it | todo | (the tutorial says to hold the button over the creature to zoom in) see [../camera/](../camera/) |
| Letting go stops any force-feedback effect of the contact | todo | no force feedback |

## Where the hand rests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand meets the body where the line of sight through the cursor meets the creature's mesh | partial | openblack meets capsules round the bones (test `CreatureFeedback.TheHandTouchesTheBodyWhereTheLineOfSightMeetsIt`), not the mesh |
| Missing the body, the hand tries again along the line through the middle of the cursor's last two places | todo | |
| The hand is held a little short of the body, by about a twentieth of the creature's height, easing there over 0.3 seconds | partial | `CreatureHandSystem` rests the hand on the body; the gap and ease are not the game's |
| Coming nearer than the body allows, it snaps to the nearer place at once rather than easing | todo | |
| Away from the body, the hand keeps back from the camera by a quarter of the creature's height | todo | |
| The hand is drawn larger with the creature's size, so it suits the creature | todo | openblack keeps the hand's usual size |
| Far from the creature's middle (more than 0.6 of its height), the hand is drawn 0.3 of its height back towards the camera | todo | |
| On the body, the hand turns to face the body's surface over 0.9 seconds | todo | |
| Off the body, the hand turns back upright over 2.5 seconds | todo | |
| The body is felt as capsules from each joint to its parent, as posed | done | test `CreatureFeedback.TheBodyIsACapsuleFromEachJointToItsParentsPlaced` (openblack's stand-in for the mesh) |

## Stroking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the body the hand plays its tickling cycle as it strokes | partial | openblack plays the stroking cycle (`src/Game.cpp`); the game plays the tickle cycle on the body |
| The hand must rest on the body for a second, with the button held, before it strokes | done | `creature_feedback::k_StrokeHoldMs` (1000 ms) |
| A stroke lands on the nearest of nine parts of the body: the head, each armpit, the belly, the groin, each foot and each hand | done | test `CreatureFeedback.AStrokeLandsOnTheNearestPart` |
| A new stroke needs another part than the last and two seconds since the last | done | test `CreatureFeedback.StrokesNeedANewPartAndTime` |
| The creature plays the pleased animation of the part stroked, mirrored for the left side | done | `creature_feedback::k_RewardAnimations`, `k_RewardMirrored` |
| It pulls a face for two seconds: an aah for the head and belly, an ooh for the groin, a smile elsewhere | done | `creature_feedback::k_RewardFaces` |
| A stroke doesn't interrupt an action less than 0.8 of the way through | done | `k_StrokeInterruptsAfter` |
| Each stroke adds 0.1 to the running sum, at most 1 | done | `k_StrokeAmount` |

## Slapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand moving across the creature faster than five of its heights a second shows the slapping cycle | done | `CreatureHandSystem` (slap shown 0.3 s) |
| A slap needs the hand between the ground and 1.1 of the creature's height | done | `k_SlapAbove` |
| Below 0.4 of its height a slap hits the feet, below 0.7 the waist, above that the head | done | test `CreatureFeedback.SlapsAreClassedByHeightAndSpeed` |
| Faster than nine heights a second it is a hard slap, slower a gentle one, with the gentle animations | done | `k_SlapSpeed`, `k_HardSlapSpeed`, `k_GentleOffset` |
| The slap plays to the side the hand swept towards | done | `ClassifySlap` (mirrored) |
| Slaps are at least a second apart | done | `k_SlapIntervalMs` |
| The hand's sweep only counts as a slap as the cursor crosses the middle of the screen | todo | (unconfirmed reading of the game's test) |
| A slap doesn't interrupt an action less than 0.35 of the way through | done | `k_SlapInterruptsAfter` |
| A gentle slap takes 0.1 off the sum and a hard one 0.2, twice as much if the creature was enjoying itself (sum past 0.25), never below -1 | done | `AfterSlap`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| Every third hard slap in a row throws up an effect on the creature, stronger the harder the slap | todo | |
| With a force-feedback mouse a gentle and a hard slap each have their own jolt | todo | |
| In a network game a hard slap knocks the creature's body about | todo | (unconfirmed) |
| Punching the creature | n/a | the punch cycle is in the hand's animations but the game never plays it |
| Tickling as a separate act | n/a | the game has no tickle apart from stroking; its tickle cycle is what the hand plays while stroking |

## Letting go and after

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go tells the creature's mind how it was treated, from -1 to 1 | done | `CreatureHandSystem::Release`; see [../creature/](../creature/) |
| Feedback this slight only makes the creature look at the player | done | `k_SlightFeedback` |
| The status panel shows the reward until the hand next takes hold | done | `CreatureHandSystem::GetLastFeedbackSum` |
| The creature reacts to each stroke and slap | done | `CreatureHandSystem`, `CreatureMindReactions.cpp` |
| The creature looks at, follows, turns to or runs from the player's hand nearby | todo | see [../creature/](../creature/) |
| The creature copies what it sees the hand do to others | todo | see [../creature/](../creature/) |
