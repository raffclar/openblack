# Other players' hands

Every god in a game has a hand of its own: the rival gods of the story and of skirmishes move theirs about the land to
pick things up, throw, cast and work their creatures, and in network games each player sees the others' hands. The rules
of network play are in [../multiplayer/](../multiplayer/).

**Progress: 2/28 done, 4 partial — 14%**

## Each player's hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player has a hand of their own, with what it holds, where it is, its speed, and the camera it looks from | partial | openblack has two hand entities for the local player only (`HandSystem`, left and right); other players have none |
| A hand holds one thing (or one handful) at a time; a full hand takes nothing more | partial | Done for the player's hand (`hand_grab::PassesGate`); other players' hands don't exist yet |
| Each player's hand remembers the last thing it picked up and the last it dropped, forgetting them when they are destroyed | todo | |
| Things a hand holds that are destroyed leave it at the next turn | todo | |
| Each hand has its own leash | partial | `LeashSystem` records each leash's player, but only the local player's hand uses one |
| A hand's interaction locked onto something carries on each turn only while that thing says so and the hand stays inside its player's influence | todo | see [clicking_and_activating.md](clicking_and_activating.md) |
| A hand's place is reported as a point on the map, which miracles and scripts use | partial | the local hand's place is known (`HandSystem::GetPlayerHandPositions`); the game's reported place is not kept separately |
| Only the local player's hand plays the guidance heartbeat and changes the local interface | todo | |

## The rival gods' hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god's hand is drawn moving over the land | todo | (unconfirmed how it is drawn) |
| It moves to a place, to an object or to its creature's head, and can hold over an object | todo | |
| It moves inside its own influence, keeping its height over the land as it goes | todo | |
| It picks up villagers, rocks, trees, food and wood, and drops them on a town, a worship site, a building site or a store | todo | |
| It throws things at a place, at another god's town or creature | todo | |
| It gives things to its creature | todo | |
| It strokes and slaps its creature, and puts its leash on things | todo | see [creature_contact.md](creature_contact.md) |
| It raises and lowers village totems | todo | see [totem.md](totem.md) |
| It draws gestures and casts miracles from icons or gestures | todo | see [../miracles/](../miracles/) |
| It combines scaffolds and makes disciples | todo | |
| It takes food and wood out of stores to use | todo | |
| How long each of these takes is estimated before it is started | todo | |
| Another god's fireball can be caught by the player's hand | done | `MagicSystem.cpp` (catch); see [picking_up.md](picking_up.md) |
| Another god's creature can be held and stroked or slapped by the player's hand | done | `creature_hand::MayHold` |

## Network play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What a hand does is sent to the other players as messages and applied at the start of the next turn on every machine | todo | see [../multiplayer/](../multiplayer/) |
| The hand's and camera's movements are sent to the other players, no more often than needed | todo | |
| Other players' hands are drawn where their messages put them | todo | |
| Locking onto an object is started and ended by message, so all machines agree | todo | |
| A hand's gesture is sent as its recognised result, not as its mouse movements | todo | see [../gesture/](../gesture/) |
| The hand's held miracle sends messages every turn while it needs them | todo | |
