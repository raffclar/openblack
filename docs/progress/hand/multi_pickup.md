# Scooping handfuls

Holding the Action button over a pile, a village store, a field or a fish farm scoops up a handful of food or wood that
grows the longer the button is held, with a stream of particles flowing into the hand. The hand hovers over the
source, tipped down towards it, until the button is let go.

openblack (`physics` branch): `HandGrabSystem` scoops from food and wood piles, storage pits' piles included, with the
game's amounts (`hand_grab::ScoopAmount`, `ScoopTaken`) and its rising sound; fields and fish farms aren't scoopable yet.
Tests: `HandGrab.AScoopRampsUpOverItsTimeAsASquare`,
`HandGrabSystemWithWorld.APileIsScoopedIntoAHandfulThatGrowsWhileTheButtonIsHeld`.

**Progress: 21/24 done, 1 partial — 90%**

## Starting a scoop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scoop starts at once (no wait) from a food pile, a wood pile, a village store's food or wood, a field with crops or a fish farm with fish | partial | Piles, storage pits' piles and fields done (`HandGrabSystem::Press`); openblack has no fish farms |
| A pile only gives the resource it holds, and only while it holds some | done | `GameHandGrabWorld` |
| The first grab makes a handful (the hand's food or the hand's wood) of 25, or what the source has, and puts it in the hand | done | `hand_grab::ScoopTaken` |
| A handful from a poisoned pile is poisoned too | done | `b1b75864`: the handful is poisoned and streams in as poisoned food (particles 108); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| A field gives a first handful of 25 (or what it has), halved when the crop is ripe | done | First min(25, food), halved when ripe, then the ramp, also halved when ripe (`b2486ca2`); test `test_hand_grab_system` |
| The hand's distance from the camera is set to reach the source as the scoop starts | done | `HandGrabSystem::UpdateFrame` (the hand stays over the source) |
| The cursor is pinned while scooping, and freed when the scoop ends | done | Pinned for the whole scoop and freed when it ends; only the drawn cursor image is pinned, the pointer stays live (`HandGrabSystem`, `GetCursorImagePosition`, `92f4af98`, `6afe6217`) |
| Scooping starts a force-feedback effect for the kind of resource | todo | openblack has no force feedback; see [clicking_and_activating.md](clicking_and_activating.md) |
| Scooping starts a stream of particles from the source into the hand, by the kind of resource | done | The `ER_MultiPickup` rule: 8 grains a second rise from the land under the hand to the hand over 1 s (`ParticleHandRules.cpp`, `92f4af98`, test in `test_particle_miracles`) |

## While the button is held

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A handful starts at 25 | done | From the hand pots' info (`ScoopTaken`) |
| Each game turn it takes 8 + 62·t² more, t rising from 0 to 1 over 60 turns (the info's 6 seconds turned into turns) | done | `hand_grab::ScoopAmount`; test `HandGrab.AScoopRampsUpOverItsTimeAsASquare` |
| A single handful can hold up to 20000 | done | `ScoopAmount` cap |
| The handful's mesh grows with its amount | done | min(amount / step + 0.25, 5) of its size, growing smoothly (the game doesn't step it) |
| The scooping sound rises as the handful fills | done | Pitch 60 + 180·t² (`HandGrabSystem`) |
| The source loses what is scooped, and is gone once empty (a store's own pile stays) | done | Piles lose it and go when empty; a store's pile goes through the store's own taking and tops the handful up from its other piles (`ResourceStoreSystem`) |
| The hand hovers over the source at its height plus the source's height | done | `HandGrabSystem::UpdateFrame` |
| The hand is tipped down towards the source at 78.75 degrees, further when it is lower than 2.5 units | done | `hand_orientation::TipForwards`, `HandGrabSystem::GetScoopTip`, applied in `Game::OrientHand` (`43257aeb`, test `test_hand_orientation`) |
| The hand holds the handful from the side while scooping | done | `hand_grab::HoldOf` |
| The hand turns with the camera's heading as it scoops | todo | |

## Ending a scoop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go ends the scoop, as does the source emptying or the hand leaving the player's influence, and stops its particles and force feedback | done | `HandGrabSystem::ProcessTurn` / `Release` |
| The handful is then held like anything else | done | See [holding.md](holding.md) |
| A handful let go slowly is poured out, streaming down into a pile where the hand is | done | `GameHandGrabWorld::PourPot`; see [throwing.md](throwing.md) |
| A handful pressed onto a store, or a pile of the same resource, goes into it | done | test `HandGrabSystemWithWorld.AHandfulPressedOntoAStoreGoesIntoIt`; building sites aren't in openblack |
| The food and wood miracles pour from the hand the same way | done | `src/Magic/HandMotion.cpp` (pour pose); see [../miracles/](../miracles/) |
