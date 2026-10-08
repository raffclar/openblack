# Leash gestures

The player puts the leash on their creature and picks which leash it wears by drawing gestures. What each leash does
to the creature is in [../creature/leash.md](../creature/leash.md).

**Progress: 12/15 done, 0 partial — 80%**

A scribble takes the leash off or closes the picker: see [scribble.md](scribble.md).

## Putting the leash on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The square spiral is the leash gesture, taken from the game's tables | done | `HandContext::leashGesture`, filled from the game's tables in `src/ECS/Systems/Implementations/GestureSystem.cpp` |
| The leash gesture is only waited for when the player has a creature that is not fighting and no picker is open | done | `gesture::Requests` in `src/Gestures/GestureRequests.cpp`; test `GestureRequests.TheLeash` |
| It is also not waited for while the miracle selection is open | todo | The selection is not ported, so this never applies yet |
| An unleashed creature needs to know at least one leash, a leashed one at least two | done | `gesture::KnownLeashes` |
| On an unleashed creature the gesture puts the leash on | done | `LeashSystem::Toggle`, called from `GestureSystem` |
| If the creature knows two or more leashes, the leash picker opens as well | done | `gesture::LeashPicker` |
| On a creature already leashed, the gesture opens the leash picker | done | `GestureSystem` |
| Recognising the leash gesture shows its trail on the land with the recognition sound | done | See [gesture_effects.md](gesture_effects.md) |

## The leash picker

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The picker waits for the gesture of each leash the creature knows, except the one it wears | done | test `GestureRequests.TheLeash` |
| The leash of aggression is a vertical scribble, the leash of learning an E, the leash of compassion a heart | done | From the game's tables (`leashSelectionGestures` in `src/InfoConstants.h`) |
| Drawing a leash's gesture changes the creature's leash to it and closes the picker | done | `LeashSystem::ChangeType`; see ../creature/leash.md for the change's effects |
| The picker closes by itself after 25 seconds | done | Timeout from the game's tables; test `GestureRequests.TheLeashPickerClosesByItself` |
| The picker closes when the leash comes off the creature | done | `LeashPicker::Update` |
| While the picker is open, the gestures of the leashes on offer are shown as icons | todo | (unconfirmed how they are laid out) |
| Only the player who leashed the creature can change or remove the leash by gesture | n/a | Only one player owns a creature in openblack's single-player game (see ../multiplayer/) |
| On the first land in a single-player game, drawing a square wave in the picker starts a hidden script | todo | (unconfirmed what the script does) |
