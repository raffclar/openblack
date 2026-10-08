# Miracle gestures

Gestures are how the player works with miracles: a spiral calls up the miracle selection and the miracle's own gesture
brings it into the hand, a circle sizes a storm or a shield, and each miracle's power-up gestures make it stronger. The
miracles themselves, and what they do once cast, are in [../miracles/](../miracles/).

**Progress: 7/25 done, 4 partial — 36%**

## Sizing with a circle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A circle is waited for while the hand holds a miracle sized by a circle (the storms, the tornado, the two shields) and the Action button is held | done | `gesture::Requests` in `src/Gestures/GestureRequests.cpp`; test `GestureRequests.AStormSeedWaitsForACircleThenPowersUp` |
| The circle is waited for whether or not the miracle in the hand is ready to cast yet | done | test `GestureRequests.ACircleIsWaitedForWhetherOrNotTheSeedIsReadyOrCast` |
| The circle's middle on the land and its size across the land are handed to the miracle, which is cast there at that size | done | `GestureEvent::Kind::Circle` in `src/ECS/Systems/GestureEventsInterface.h`, taken by the magic system (`src/Magic/CastInput.cpp`) |
| Once drawn, the circle is remembered for five seconds of game time and no new circle is waited for meanwhile | done | `gesture::k_CircleSeconds`; the magic system keeps its own 50-turn copy (two game-paced clocks; minor) |
| Releasing a circle-sized miracle with no circle remembered fails, with the failure sound | done | `src/Magic/CastInput.cpp`; seen in game in the testbed's circle-casting scenario |
| Starting the power-up system forgets a remembered circle | partial | openblack forgets it when the miracle in the hand changes, not when a power-up begins |

## Powering up the miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle has up to three power-up gestures (spiral, inverse spiral and others), read from the game's tables | done | `GetPowerUpGesture` in `src/Magic/MagicTables.h` |
| Power-up gestures are only waited for while the hand holds a ready, uncast miracle made at one of the player's worship icons | partial | `HandContext::Seed::canPowerUp` follows the icon rule, but worship sites and their icons are not ported, so in a normal game no seed can be powered up; works in the testbed (see ../worship/) |
| Every power-up level's gesture is waited for except the level already asked for | done | `gesture::Requests`; test `GestureRequests.AStormSeedWaitsForACircleThenPowersUp` |
| Drawing a power-up gesture asks the worship icon for the extra power; the level only changes once the icon has charged it, and any surplus goes back to the site | partial | `MagicSystem` raises the level at once and takes the cost from the player's prayer power (`PowerUpHeldSeed`); waits on worship icons |
| A help message is given when a power-up is asked for | todo | No help system hooks for gestures yet (see ../interface/) |

## Calling up a miracle by gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing in the hand, a spiral opens the miracle selection for the player's own miracles | todo | Waits on worship sites and their icons |
| An inverse spiral opens the selection for miracles meant for the creature | todo | |
| With the selection open, drawing a miracle's own gesture brings that miracle from its worship site into the hand | todo | Each miracle's gesture is in the game's tables (`src/InfoConstants.h`) but nothing uses it |
| The selection only offers miracles the player has and whose worship site can supply them | todo | (unconfirmed exactly which conditions are tested) |
| The selection closes by itself after 30 seconds | todo | Value in the game's tables (`src/InfoConstants.h`) |
| A scribble closes the selection | todo | See [scribble.md](scribble.md) |
| Drawing an R repeats the last miracle called up, if within 40 seconds | todo | |
| A key opens the miracle selection, and another repeats the last miracle, as the gestures do | todo | See ../interface/ for key bindings |
| A miracle called up only becomes active in the hand after a short delay (1.5 seconds) | todo | Value in the game's tables; see ../miracles/ |
| While the selection is open the gestures that can be drawn are shown as icons on the screen | todo | (unconfirmed how they are laid out) |
| The tutorial counts when the selection is opened and when a miracle is called up by gesture, to know the player has learnt it | todo | Needs the help events in the script engine (see ../story/); the shipped lesson is Khazar's, in [Impress Village](../story/gold_scrolls/impress_village.md#the-gesture-hand-demo) |

## Other rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle's gestures only work inside the player's influence | partial | Only the scribble that drops a miracle tests influence; the selection, where the test belongs, is not ported |
| Every recognised gesture starts a force-feedback effect on supporting mice | n/a | Force-feedback mice are not supported |
| The game counts the gestures each player has drawn for its statistics | todo | No game statistics yet; see [../interface/statistics_counted.md](../interface/statistics_counted.md) |
| Rival gods controlled by the computer call up miracles by gesture or from their icons too | todo | No computer players (see ../story/) |
