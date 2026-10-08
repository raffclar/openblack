# Holding a miracle and pouring

While the hand holds a miracle's seed it has a state of its own: it is placed like a hand holding something, measures its
own movement for the throw, takes the hold the miracle asks for, and can be lifted and tipped by a timed pour as the
food, wood and water miracles sprinkle from it, or as a creature miracle pours onto a creature. What each miracle does is
in [../miracles/](../miracles/).

**Progress: 22/23 done, 0 partial — 96%**

## Holding a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a seed, the hand is placed as a holding hand is, under the cursor | done | `Game::PlaceHand`, `HandHoldPoser` |
| Each frame the hand's velocity is measured from how far it moved, for the throw and the spin | done | `src/Magic/HandMotion.cpp` |
| The miracle's hold picks the animation: from above, the idle hand, the horn-shaped hold or from the side | done | `src/Magic/HandHoldPose.cpp`; test `HandHoldPose.EachHoldTakesAStillFrameOfItsAnimation` |
| A seed not yet ready is held as one not yet ready, until it is | done | test `HandHoldPose.ASeedIsHeldAsAMiracleNotYetReadyUntilItIsReady` |
| The hand rises by how it holds, measured to the land under it | done | test `HandHoldPose.TheHandRisesByHowItHolds` |
| The cursor running ahead sways the hand up to three tenths of a radian | done | test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian` |
| The hand rolls back about the line to the camera | done | test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| Upright, the seed takes the hand's turn; for a right hand it is turned half round | done | tests `HandHoldPose.UprightTheSeedTakesTheHandsTurn`, `HandHoldPose.TheSeedTurnsHalfRoundForARightHandAndByItsOwnTurn` |
| Taking a seed fades the drawn hand over 0.13 seconds | done | test `HandHoldPose.TakingASeedFadesTheDrawnHandOverThirteenHundredths` |

## Pours of the sprinkling miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pour follows a natural cubic spline through four points, flat at both ends | done | `magic::CubicSpline`; test `HandMotion.ThePoursCurveIsFlatAtBothEndsAndSwellsToAlmostTwiceItsPoints` |
| The curve rises from rest, swells to almost twice its points' height in the middle and comes back to rest | done | same test |
| Food and wood pour over four seconds, raising the hand by 10 and tipping it by 1.07 radians at the points, over and over | done | `magic::k_FoodWoodPour` |
| Food and wood keep the hand where the pour began, so everything lands in one place | done | test `MiracleVisuals.APourThatClampsTheHandKeepsItWhereItBegan` |
| Water pours over eight seconds, raising the hand by 8 without tipping it, and the hand stays free to move | done | `magic::k_WaterPour` |
| The pour is stepped each game turn and drawn between turns | done | `magic::StepPour`, `magic::PourPoseAt` |
| A pour that doesn't loop stops at its end | done | test `HandMotion.APourThatDoesntLoopStops` |
| Stopping the pour lets the hand back to rest over the next game turn | done | `magic::StopPour` |
| The pour stops when the miracle closes down | done | `MagicCreatureSpells.cpp` (StopHandGrain), `MagicSystem` |
| The pour is drawn in the hand's pose: the lift raises the hand, the tip tips it forward | done | `src/Game.cpp`, `HandHoldPoser` |

## Pours onto a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature miracle cast on a creature pours onto it once, over four seconds | done | `MagicSystem::StartHandGrain` |
| It raises the hand by 0.4 of the creature's height and tips it by one radian | done | `MagicCreatureSpells.cpp` (unconfirmed against the game beyond the miracle audit) |
| Only the local player's own hand pours | done | `MagicSystem::StartHandGrain` |

## Other players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pours started and ended are sent to the other players so their view of the hand pours too | todo | see [../multiplayer/](../multiplayer/) |
