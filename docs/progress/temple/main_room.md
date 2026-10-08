# Main room

The temple's central hall: a pool with the island in relief floating over it as a map, five buttons choosing what the
map shows, a scroll of the world's statistics, and doors to every other room.

**Progress: 14/23 done, 5 partial — 72%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room is drawn with its floor reflecting it | done | `src/3D/Implementations/TempleInterior.cpp`, `RenderingSystemTemple` |
| The pool shimmers, drawn twice over itself, turned | done | `TempleInterior::GetPoolTime` |
| The temple is lit by the alignment of the realm the player came in from: red pulse for evil, slow rainbow for good | done | `src/3D/TempleLight.cpp`; test `test_temple_light` |
| The doors swing open as the camera walks through them, with door sounds | done | `src/3D/TempleDoors.cpp`; test `test_temple_doors` |
| Signs over the doors name the rooms and light up for the door under the hand | done | `src/3D/TempleSigns.cpp` |
| Tooltips say what the hand is over and what clicking does | partial | `src/3D/TempleToolTips.cpp`; test `test_tool_tips`; pictures and replay tooltips todo |
| Moving about the room | done | see ../camera/temple_camera.md |

## The map in the pool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The island floats over the pool in relief, from the land's heights and brightness, fading out at the coast | done | `src/3D/TempleMap.cpp`; test `test_temple_map` |
| The map's land texture is drawn afresh each visit | done | `TempleInterior::GetVisits` |
| Markers turn over the map for the temples and the creatures, in their player's colour | partial | `TempleInterior::UpdateMapMarkers`; some lands give players others' colours (TODO in `TempleMap.cpp`) |
| Markers for the challenges not done, miracles being cast and the players' influence | todo | TODO in `TempleInterior.cpp` |
| A double click on the map leaves the temple for that place | done | `TempleInterior::LeaveForMapPoint`; see ../camera/temple_camera.md |
| The hand feels the click on the map | todo | TODO in `TempleInterior.cpp` |

## The five buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Buttons choose what the map shows: temples, creatures, miracles being cast, influence and challenges | done | `src/3D/TempleToggles.cpp`; test `test_temple_toggles` |
| A button is pressed in or out, turning over with a click | done | `TempleToggles` |
| Each button plays its own pitch of click | partial | one pitch for all (TODO in `TempleToggles.cpp`) |
| Turning influence on recolours the map's land by owner | todo | TODO in `TempleToggles.cpp` |
| The buttons' settings are kept with the saved game | todo | see ../engine/saving_and_loading.md |

## The world's statistics scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scroll of the world: believers, men and women, births and deaths, sacrifices, buildings, wonders and disciples | partial | the scroll's facts are made up (`TempleScrolls::Facts::Mock`) until openblack keeps the game's statistics ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |
| The scroll turns up and down as the mouse drags it | done | `src/3D/TempleScroll.cpp`; test `test_temple_scroll` |
| Clicking a scroll brings the camera close and its text is drawn in front of it | done | `TempleScrolls::SetFocus`; see ../camera/temple_camera.md |
| Scrolls squeak as they turn | done | `TempleScrolls.cpp` |
| Scroll text is written onto parchment in the game's font | partial | `TempleScroll.cpp`; glyph heights and pictures todo |
