# The scribble

A quick back-and-forth scribble is the game's "no" gesture: it shakes a miracle out of the hand, calls off a power-up,
takes the leash off and closes the pickers. What it does depends on what the hand holds and what is open.

**Progress: 9/15 done, 2 partial — 67%**

## Recognising a scribble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scribble is strokes back and forth that must be wider than they are tall | done | Two templates in `Data\Gestures.jty`; test `GestureMatcher.RecognisesAScribbleButNotATallOne` |
| A scribble is only recognised when there is something for it to do | done | `gesture::Requests` in `src/Gestures/GestureRequests.cpp`; test `GestureRequests.NothingToDoNothingWaitedFor` |
| A scribble leaves no trail on the land and makes no recognition sound | done | `gesture::ShowsRecognition`; test `GestureRequests.EveryGestureButAScribbleIsShownRecognised` |
| Scribbling makes the hand shake, with the shake sound and the bands flying off | done | See ../hand/ (hand shake); `DiscardHeldSeed` in the magic system plays it at the game's volume |

## With a miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a power-up is being charged by the worship icon, a scribble calls it off instead of dropping the miracle | todo | openblack has no pending power-up (levels apply at once); waits on worship icons |
| With a ready miracle from a worship icon in the hand and no power-up asked for, a scribble drops it back where it came from, even outside the player's influence | done | test `GestureRequests.OutOfInfluenceOnlyASeedBeingPoweredUpIsScribbledAway` |
| Any other miracle in the hand is only scribbled away inside the player's influence | done | `HandContext::inInfluence`; test `GestureRequests.ASeedThatCantPowerUpIsStillDroppedByAScribble` |
| A miracle scribbled away gives its prayer power back to the worship site it came from | partial | `SeedRefund` in `src/Magic/PrayerRules.cpp`; no worship sites yet, so the refund has nowhere to go in a normal game (see ../worship/) |
| A help message follows a scribble that drops a miracle or calls off a power-up | todo | See ../interface/ |

## With other things in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Inside the player's influence, a scribble shakes out anything the hand holds that may be shaken out, not only miracles | partial | Only miracles are scribbled away; held objects have `HandGrabSystem::ForceDrop`, which the scribble doesn't call yet (see ../hand/) |
| With the hand holding the leash, a scribble takes the leash off the creature | done | `Purpose::ShakeOffLeash`; test `GestureRequests.TheLeash` |
| A leash tied to something rather than held in the hand is not taken off by a scribble | done | `HandContext::Creature::tied` |
| With an empty hand while a worship icon is charging, a scribble cancels the charging | todo | Waits on worship icons |

## Closing pickers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash picker open, a scribble closes it and leaves the leash on | done | `Purpose::ClosePicker`; see [leash_gestures.md](leash_gestures.md) |
| With the miracle selection open, a scribble closes it | todo | The selection is not ported (see [miracle_gestures.md](miracle_gestures.md)) |
