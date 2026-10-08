# Clicking and activating

What the Action button does when the hand isn't picking something up: tapping things (knocking on houses, splitting
rocks, breaking scaffolds, opening reward chests, starting challenges), activating them (the temple's entrance, miracle
bubbles and icons, leash posts), double clicks, locked selection, and the force-feedback mouse the game supported.

**Progress: 14/44 done, 3 partial — 35%**

## Tapping in general

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A quick press and release of the Action button over a thing taps it rather than taking hold | partial | done for miracle bubbles, leash posts, creatures and the temple entrance; nothing else can be tapped |
| Each kind of thing says whether it can be tapped at all; a thing that can't is left alone | partial | only the kinds above are tested |
| Village centres, dead trees and bonfires can't be tapped | done | nothing in openblack taps them |
| Taps are applied at the next game turn, from the place the hand reported, so every player sees the same | todo | see [../multiplayer/](../multiplayer/) |

## Houses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any house can be tapped | todo | |
| Tapping a house knocks on it: everyone inside comes out to see who knocked | todo | see [../villager/](../villager/) |
| The knock counts the people living there, for the town's figures | todo | (unconfirmed what the count is shown in) |
| On a finished house, the player's own hand plays its tap-house animation where it is | todo | the tap-house cycle is loaded but never played |
| On a finished house, one of nine knocking sounds plays at the hand, in turn | todo | |

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rock can be tapped only when it is taller than 0.7 m (a rock too wide for the hand to lift, over 3.6, is tapped at once on a press; a liftable one by a short press), inside the player's influence | done | `object_physics::CanTapRock`, `TapRock`, `HandGrabSystem`; tests in `test_hand_grab_system` |
| Tapping a rock splits it in two | done | `object_physics::SplitRock`; checked in game; see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Splitting a rock plays one of four cracking sounds at the hand, in turn | done | `object_physics::TapRock` (rock tap 130+n, n cycling) |
| The player's creature is shown the deed and leans towards copying it (a mild empathy of one half) | todo | see [../creature/](../creature/) |

## Scaffolds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scaffold worth more than one, not in use and not tied to a building, can be tapped ("Tap To Break") | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Tapping it breaks it back into single scaffolds, clearing an old building site it stood for | todo | |
| Breaking a scaffold plays one of four sounds at the hand, in turn | todo | |

## Reward chests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a closed reward chest opens it ("Tap To Open") with its opening sounds; an open one only plays the tap sound | todo | see [../story/](../story/) |
| A chest of food or wood spills a pile of it where it stood | todo | |
| A chest can give a scaffold for a town's building | todo | |
| A chest can give a random one-shot miracle, picked from the good list when the player is good, the evil list when evil, and either at random when neutral, among those allowed on the land | todo | |
| A chest can teach a miracle: the player or the town gains it, shown by a gesture drawn on the land or a miracle sign over the town | todo | |
| A chest can give belief to a town, with a spot of light | todo | see [../worship/](../worship/) |

## Challenge markers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a challenge's marker (the silver and gold scrolls, signposts) starts its challenge, stopping any help being given | todo | see [../story/](../story/) |
| A "did you know" marker plays a chime and shows its text instead | todo | |
| Tapping one tells the help system, which can explain challenges the first times | todo | see [../interface/](../interface/) |
| A marker tapped is remembered as activated | todo | |

## Miracles and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a one-shot miracle bubble or a dispenser's bubble puts its miracle in the hand | done | `MagicSystem` (tap of the left button); see [../miracles/](../miracles/) |
| Tapping a worship site's miracle icon takes its miracle into the hand once charged | todo | see [../miracles/](../miracles/) |
| Tapping another player's fireball catches it | done | `MagicSystem.cpp`; see [../miracles/](../miracles/) |
| Tapping a leash post, or the player's creature, puts the leash on or takes it off | done | `LeashSystem::HandleInput`; see [../creature/](../creature/) |
| Tapping in a creature fight aims the player's creature's blows | done | `CreatureFightSystem::Press`; see [../creature/](../creature/) |
| Tapping a land puzzle's totem plays its sound and resets its choice | todo | (unconfirmed which land's puzzle) see [../story/](../story/) |

## The temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping the player's own temple's entrance takes them inside | done | `src/Game.cpp`, `TempleExteriorSystem::EntranceAt`; see [../temple/](../temple/) |
| The entrance only works once the land's script allows the temple | todo | TODO in `src/Game.cpp` |

## Holding the button down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button over the player's creature takes hold of it to stroke or slap it | done | see [creature_contact.md](creature_contact.md) |
| Holding the Action button on a field, fish farm, pile, totem or creature locks onto it, and the hand works it until let go | todo | see [totem.md](totem.md) and [pouring.md](pouring.md) |
| Locked onto a thing, the hand stays on it while the player's influence holds | todo | |
| A double click on the land flies the camera there | done | `DefaultWorldCameraModel` (double click); see [../camera/](../camera/) |
| A double click on a creature in Creature Mode locks onto it | done | `src/Creature/CreatureMode.cpp` (DoubleClicks) |
| A double click is a second press within half a second, within a small box of the first | done | `src/Creature/CreatureMode.h` |
| Pressing the Action button where nothing takes it (the sky, out of reach) does nothing | done | `src/Game.cpp` |

## Force-feedback mouse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With a force-feedback mouse, what the hand holds or is over is felt by its own texture (bonfires, dead trees, fragments, people, puzzle pieces, miracles) | todo | effects in `Data/Immersion`; not done |
| Scripts start and stop force-feedback effects | todo | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` stubs in `src/CHLApi.cpp` |
| Scripts ask whether a force-feedback mouse is present | partial | `IMMERSION_EXISTS` always answers no, which is right for a player without one |
