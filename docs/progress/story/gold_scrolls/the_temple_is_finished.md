# The Temple Is Finished (the first gold scroll)

The story step between the opening's walk to the village and Choose Your Creature: once the villagers finish the
temple, the advisors show the player its entrance, make them go in and come out again, then fly the camera to the
creatures' gates and show what a gold story scroll looks like. It has no title of its own in the game's text table and
no scroll the player can click: the gold scroll it shows is a stand-in, removed when the scene ends, at exactly the
place where [Choose Your Creature](choose_your_creature.md)'s real scroll appears next.

**Land:** 1 · **Giver:** the two advisors, at the new temple · **Script:** CitadelGuide · **Reward:** none (it
opens the way to the gold scrolls) · **Repeatable:** no

Sources: the land's control script and the scene's script (the original source text, which matches the shipped
`challenge.chl`), the game's text table and the executable. openblack is judged on the physics work tree
(`ob-wt-physics`): the land's control script stops long before this scene ([../../scripts/land1_script.md](../../scripts/land1_script.md)),
and most of the commands it needs only log "not implemented" in `src/CHLApi.cpp`.

**Progress: 2/34 done, 4 partial — 12%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script runs the scene straight after the opening walk ("follow us", [../tutorial.md](../tutorial.md)), which ends only when the villagers have finished the temple | todo | the control script stops long before (see [../../scripts/land1_script.md](../../scripts/land1_script.md)) |
| A new game that skips to choosing the creature never plays it: the temple is built at once at its spot and marked finished instead | todo | the skip question and building commands are stubs (see [../../scripts/land1_script.md](../../scripts/land1_script.md)) |
| Before the scene, two did-you-know scrolls are placed: one by the village on zooming, rotating and double clicking, one by the temple door on the Space bar's safe camera position | todo | the shared did-you-know script; scrolls are not made (`CreateHighlight` is a stub); their lines: "You can use the mouse wheel to zoom in and out…" and "The Space Bar is a shortcut key to place your camera in a safe position…" |
| The control script waits for the whole scene (including the visit inside the temple) before it places the fish-farm did-you-know, starts Throwing Stones and The Lost Flock, and runs Choose Your Creature | todo | see [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [choose_your_creature.md](choose_your_creature.md) |
| The scene is logged nowhere: no challenge record, no reminder, nothing in the scroll log | todo | nothing to show; noted so the port doesn't invent one |
| The scene is a "temple" kind of script: while the player is inside the temple, only temple scripts may take the camera, so this scene can still run its own camera there | partial | the executable refuses camera control inside the temple to all but the two temple kinds of script; openblack runs every kind always (see ../../engine/script_vm.md) |

## The temple flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema (widescreen bars, the player's control taken) begins with the first epic theme | partial | the widescreen and music commands work (`SetWidescreen`, `StartMusic` in `src/CHLApi.cpp`); the scene is never reached |
| The camera flies to the temple over 6 seconds, then sinks slowly down its face over 23 seconds | todo | `MoveCameraPosition` and `MoveCameraFocus` are stubs |
| Both advisors step out; the good advisor: "At last. The people have finished our Temple." | todo | `SpiritEject` and the say command are stubs |
| A second line, "It's beautiful and it will be extremely useful later.", is in the text table but commented out of the scene | n/a | cut line |
| The camera moves to the entrance over 4 seconds; 2 seconds in, the evil advisor: "This is the entrance. Click the Action Button on it to take you inside." (with the action button's picture) | todo | |
| The evil advisor stops pointing and the cinema ends when the camera arrives | todo | |

## Going in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple is let in for the first time ("enable temple"); until now it could not be entered | todo | `SetInterfaceCitadel` is a stub; openblack lets the player in whenever the temple exists (see ../../temple/temple_exterior.md) |
| Every 20 seconds until the player goes in, the evil advisor steps out and repeats "This is the entrance. Click the Action Button on it to take you inside." | todo | `CreateTimer` and the timer read are stubs |
| Whenever the camera strays more than 100 from the door, or the door is out of view, the camera is pulled back to the entrance over 3 seconds and the good advisor says "No, click on the door." — so the player cannot wander off until they go in | todo | `InsideTemple`, the near and viewed questions and the camera moves are stubs |
| Going in through the door itself works as in the game | done | the entrance and walking in: ../../temple/temple_exterior.md |

## Inside the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 5 seconds after the player goes in, a timer starts: after a minute inside, then every 30 seconds, the good advisor says "Press the Escape key to leave the Temple." | todo | `InsideTemple` is a stub |
| If the player leaves within those first 5 seconds the reminder never runs (quirk) | todo | |
| Escape leaves the temple as in the game | done | ../../temple/temple_hand_and_controls.md |
| The scene waits until the player is outside and no other dialogue is running | todo | `IsDialogueReady` and `InsideTemple` are stubs |

## Coming out: the gold scroll shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cinema begins with the neutral theme; the camera jumps to the entrance and pulls back over 3 seconds, then climbs the temple over 8 | partial | `SetCameraPosition`, `SetCameraFocus`, `StartMusic` work; the camera moves are stubs |
| The camera turns to look at the village (5 seconds), then over it (7 seconds) | todo | |
| The good advisor appears: "Now we have a Temple, let's explore. I'm dying to know what these Signposts are." and, 2 seconds in, points at a signpost in the village | todo | `SpiritAppear` is a stub; the signposts are the did-you-know scrolls |
| Scrolls are made drawable ("enable highlight draw") and a gold scroll is made 14 above the creatures' gates — the same spot and height as Choose Your Creature's scroll | todo | `SetDrawHighlight`, `CreateHighlight` and `HighlightProperties` are stubs |
| The camera drifts into the village (8 seconds); the evil advisor appears: "No, there's so much we gotta do. I say we hunt out the Gold Story Scrolls." | todo | |
| The camera turns to the scroll (8 seconds); evil advisor: "We'll need these if we're gonna progress through this world." | todo | |
| The camera pans slowly up the scroll (14 seconds); evil advisor: "A Gold Story Scroll looks like this.", pointing at it | todo | |
| The good advisor steps out, points at the scroll: "Clicking on Gold Story Scrolls with the Action Button will lead you through your Quest in Eden." | todo | |
| The camera pulls back to a view ready for exploring (6 seconds); the evil advisor steps out and the good one stops pointing: "You can activate the Scroll or you can explore. It's up to you, Leader." | todo | |
| Both advisors go home, the music stops and the cinema ends when the camera arrives | todo | `SpiritHome`, `StopMusic` (works) |
| The stand-in scroll is deleted; it can never be clicked | todo | `ObjectDelete` is a stub |

## Aftermath, failure and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing stays changed in the world except that the temple can now be entered | todo | |
| The scene cannot fail; the only wait the player controls is going into the temple and out again, which the advisors and the camera pull keep pushing them to do (no soft lock while the temple stands) | todo | |
| Next in the story: Choose Your Creature's scroll appears where the stand-in was | todo | see [choose_your_creature.md](choose_your_creature.md) |
| Music: the first epic theme for the flight, the neutral theme for the way out | partial | the music command plays these tracks (`StartMusic`, `src/Audio/GameMusic.cpp`); the scene is never reached |
| No creature is involved (it hasn't been chosen yet) | n/a | |

## Unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Commented-out camera moves straight onto the gold scroll and back to the temple | n/a | cut from the scene |
| An older draft of the temple's opening (rotating the camera, carrying wood and food to the builders, the kinds of scroll) | n/a | not in the shipped program: [../silver_scrolls/see_the_citadel.md](../silver_scrolls/see_the_citadel.md) |
