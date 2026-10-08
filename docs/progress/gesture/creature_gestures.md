# Creature gestures

Gestures that involve the creature beyond the leash: the player's gestures during a creature fight, and the gestures
the creature itself draws in the air when it casts a miracle. The creature's emotes (blowing a kiss, waving and so on)
are creature actions, in [../creature/](../creature/); the leash gestures are in [leash_gestures.md](leash_gestures.md).

**Progress: 0/7 done, 1 partial — 7%**

## In a creature fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the player's creature fights with the fight controls up, gestures choose its fight moves instead of the usual gestures | todo | `src/Creature/CreatureFight.h` has the moves, but no gesture drives them; the leash gesture is already held back during a fight (`HandContext::Creature::fighting`) |
| Drawing a star makes the creature do its special move | todo | The star is the special-move gesture in the game's tables (`src/InfoConstants.h`); the special move exists in `CreatureFightSystem` but is not tied to the gesture |
| A creature that knows miracles can be made to cast them in a fight by gesture | todo | (unconfirmed which gestures; the fight interface keeps its own miracle selection) |

## The creature drawing gestures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before casting a miracle the creature draws that miracle's gesture in the air with its hand | partial | `src/Creature/CreatureCastAgenda.cpp` adds a gesture step and `src/Creature/CreatureLayers.cpp` plays the gesture animation; not checked against the original's timing |
| The creature's hand follows the shape of the gesture as it draws, along the gesture's path | todo | (unconfirmed) The original keeps an ideal path for each gesture that the creature's hand is moved along |
| A glowing chain trails behind the creature's hand while it draws the gesture | todo | The creature's gesture chain effect is listed in `src/Particles/ParticleTypes.cpp` but never started |
| A miracle cast on the creature shows a gesture effect that starts and dies away with the spell | todo | (unconfirmed what it looks like) |

## Gestures with no use in the shipped game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's tables have gestures for zooming to the creature and for ending a gift to it, both left empty | n/a | Not used by the original game |
