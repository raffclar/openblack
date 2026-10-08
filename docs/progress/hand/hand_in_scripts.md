# Hand in scripts

Every command of the challenge scripts that reads or drives the god hand: where it is and what it is over, what it holds,
clicks and drops, hand demos, highlights, force feedback, locking the creature in an interaction, and how much of the
interface the hand may use. The script language itself is in [../story/](../story/).

**Progress: 5/39 done, 3 partial — 17%**

## Where the hand is and what it is over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the hand's position, to test it near a thing or drop a marker there | done | `GET_HAND_POSITION` in `src/CHLApi.cpp` |
| The position is that of the drawn hand, under the cursor | partial | openblack gives the drawn hand's place; the game gives the position the interface reports, which is the same outside drags (unconfirmed during drags) |
| Scripts read the object the hand is over | todo | `GET_OBJECT_HAND_IS_OVER` stub |
| Scripts read the hand's state: gripping, turning or zooming the land | todo | `GET_HAND_STATE` stub |
| Scripts read whether the player's hand is busy with a miracle charging, or that miracle | todo | `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING` stubs |
| Scripts stop a player's miracle from charging | todo | `CLEAR_PLAYER_SPELL_CHARGING` stub |

## What the hand holds, clicks and drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts read the object the player's hand holds | todo | both `GET_OBJECT_HELD` natives are stubs |
| Scripts test whether an object is in a creature's hand | todo | `IN_CREATURE_HAND` stub |
| Scripts read the last object the hand dropped, and clear it | todo | `GET_OBJECT_DROPPED`, `CLEAR_DROPPED_BY_OBJECT` stubs |
| Scripts test whether an object was clicked, read it, and clear it | todo | `GAME_THING_CLICKED`, `GET_OBJECT_CLICKED`, `CLEAR_CLICKED_OBJECT` stubs |
| Scripts test whether a place was clicked, and clear it | todo | `POSITION_CLICKED`, `CLEAR_CLICKED_POSITION` stubs |
| Scripts make an object one the hand may or may not pick up | done | `SET_ID_PICKUPABLE` (`CHLApi.cpp`) marks it, and `hand_grab::PassesGate` refuses it; test `HandGrabSystemWithWorld.ThingsOutOfTheInfluenceOrHeldByAScriptAreLeft` |
| Scripts read the town's totem, to watch the hand work it | todo | `GET_TOTEM_STATUE` stub |
| The last object picked up and dropped are forgotten when they are destroyed | todo | |

## The interface the hand may use

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts set the interface level, which sets what the camera and hand may do and how far the hand reaches | done | `SET_INTERFACE_INTERACTION`, `CameraHelp::SetInterfaceLevel` |
| The level also makes the tooltip of a held object come from elsewhere | todo | TODO in `SET_INTERFACE_INTERACTION` |
| Scripts let the hand into the temple, or keep it out | todo | `SET_INTERFACE_CITADEL` stub |
| Scripts turn the cinema bars on, which hide the hand and take the interface away | done | `SET_WIDESCREEN`, `CinematicDirectorSystem`; see [placement.md](placement.md) |
| Scripts ask whether the mouse has a wheel and how many buttons it has | todo | `HAS_MOUSE_WHEEL`, `NUM_MOUSE_BUTTONS` stubs |
| Scripts ask whether a key is held | todo | `KEY_DOWN` stub |

## Highlights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts put a highlight (a challenge or silver scroll marker) at a place, for the hand to tap | todo | `CREATE_HIGHLIGHT` stub; see [../story/](../story/) |
| Scripts show or hide a highlight and set how it looks | todo | `SET_DRAW_HIGHLIGHT`, `HIGHLIGHT_PROPERTIES` stubs |
| Tapping a highlight marks it clicked for the script, with its chime | todo | see [clicking_and_activating.md](clicking_and_activating.md) |

## The creature and the leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts tie a creature's leash to the hand, or untie it | done | `ATTACH_OBJECT_LEASH_TO_HAND` |
| Scripts read what the creature is interacting with | todo | `CREATURE_INTERACTING_WITH` stub |
| Scripts read how hard the hand has stroked or slapped the creature | todo | `GET_INTERACTION_MAGNITUDE` stub; openblack keeps the sum (`CreatureHandSystem::GetFeedbackSum`) |
| Scripts test whether the creature is locked in an interaction with the hand | todo | `IS_LOCKED_INTERACTION` stub |
| Scripts light glows on the creature's hands | todo | `SET_CREATURE_CREED_PROPERTIES` stub; see [hand_effects_and_glows.md](hand_effects_and_glows.md) |

## Hand demos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts start a recorded hand demo by name | todo | `PLAY_HAND_DEMO` stub; the demos are in `Data/HandDemo` (drag, casting, gestures, influence, giving wood, fireball …); the throwing demo is played by [Throwing Stones](../story/silver_scrolls/throwing_stones.md); the totem and miracle demos are played by [Worship Site](../story/gold_scrolls/worship_site.md#the-miracle-hand-demo), the gestures demo by [Impress Village](../story/gold_scrolls/impress_village.md#the-gesture-hand-demo) and the influence demo by Khazar's lesson on influence ([impress_village.md](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence)) |
| A demo can be started with a pause on its triggers, or without moving the player's hand | todo | arguments of `PLAY_HAND_DEMO` unused |
| Scripts wait until a demo has played | todo | `IS_PLAYING_HAND_DEMO` stub |
| Scripts wait for a demo's triggers, to talk over each step | todo | `HAND_DEMO_TRIGGER` stub |
| Scripts set which keys a demo shows being pressed | todo | `SET_HAND_DEMO_KEYS` stub |
| While a demo plays, the hand's camera hints come from the demo's recorded keys | todo | |

## Force feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts ask whether a force-feedback mouse is plugged in | partial | `IMMERSION_EXISTS` always answers no; the only quest that needs one: [The Immersion Mushrooms](../story/silver_scrolls/the_immersion_mushrooms.md) |
| Scripts start and stop a force-feedback effect, or stop them all | todo | `START_IMMERSION`, `STOP_IMMERSION`, `STOP_ALL_IMMERSION` stubs |

## Gestures and miracles from scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts draw a gesture with the hand, as the tutorial does | todo | `PLAY_GESTURE` stub; see [../gesture/](../gesture/) |
| Scripts give the player a miracle straight into the hand | partial | `MagicSystem::GiveSeedToHand` exists; see [../miracles/](../miracles/) |
| Scripts set a virtual influence that lets the hand act where the player has none | todo | `SET_VIRTUAL_INFLUENCE` stub; see [../worship/](../worship/) |
