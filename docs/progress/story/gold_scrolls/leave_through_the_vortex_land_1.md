# Leave Through the Vortex (Land 1)

The first land's last gold scroll: after Nemesis's storm kills the guide, a golden light marks a vortex that opens near
the coast, swallows a shepherd and throws him back out, and the advisors tell the player to send people and supplies
through and then follow. Clicking the scroll over the vortex dives the camera into it and ends the land. The quest's
flow is here; how vortices work (what they suck in, what crosses, the arrival) is in [../portals.md](../portals.md).

**Land:** 1 · **Giver:** the two advisors, at the vortex near the coast · **Script:** LeaveThroughVortexL1 ·
**Reward:** the way to the second land · **Repeatable:** no

Sources: the land's control script and the quest's script (the original source text, which matches the shipped
`challenge.chl`), the shared reminder script, the game's text table and the executable. openblack is judged on the
physics work tree (`ob-wt-physics`): the land's control script stops long before the storm, and most commands the quest
needs only log "not implemented" in `src/CHLApi.cpp`.

**Progress: 0/41 done, 2 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background straight after the guide's fight lesson, together with The Ogre, and then runs the storm | todo | see [../creature_guide.md](../creature_guide.md) and [../silver_scrolls/the_ogre.md](../silver_scrolls/the_ogre.md) |
| It waits until the storm opens the way: 30 seconds after the storm turns on the player's village (about two minutes after the guide dies) | todo | the storm's part: [../creature_guide.md](../creature_guide.md#after-the-storm) |
| A yellow "see this" beam is made where the vortex will open, lasting until the vortex appears | todo | `SpecialEffectPosition` is a stub |
| The good advisor steps out: "This is hopeless. Wait. What's this golden light?" and, half a second in, the evil advisor points at the spot (without having been called out first; how that shows is undetermined) | todo | `SpiritEject`, `SpiritPointPos` are stubs |
| The quest then waits until the player brings the camera within 200 of the spot; there is no reminder meanwhile | todo | the camera-near question is a stub |
| A new game that skips the creature guide (or keeps the old creature) opens the vortex and its scroll at once, with no scenes | todo | the skip questions are stubs; see [../creature_guide.md](../creature_guide.md) |

## The vortex opens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature is forbidden to go through the vortex for now, and its kind is recorded "for setting up the next land", though no shipped script ever reads it back (quirk) | todo | `SetProperty` and `GameSubType` are stubs; how the creature is remade on arrival: [../portals.md](../portals.md#arriving) |
| A cinema: the camera moves in over 7 seconds to look at the vortex's place | todo | `MoveCameraPosition`, `MoveCameraFocus` stubs |
| 4 seconds in, a Norse shepherd is made beside it, faces it and looks about (the looking-for-something animation, twice) | todo | creating villagers is not done (`CreateScriptObject` only makes mobile statics and rocks) |
| The vortex opens (an "in" vortex, the kind that takes things to the next land) and the yellow beam is removed; the shepherd is pulled in | todo | vortex creation and its pull: [../portals.md](../portals.md) |
| A second later both advisors step out; the good advisor looks back over the land; evil advisor: "What's this? Some trick? Some new Miracle?" | todo | |
| The camera moves onto the vortex (8 seconds); good advisor: "It looks like a portal of some kind." | todo | |
| The camera goes to the vortex's edge; evil advisor: "It looks like a trap to me." then "Look. It's sucking stuff in." | todo | |
| Good advisor: "Try throwing something into it." as the camera swings to a wide view of the vortex; then "I bet you anything it'll come out wherever this Vortex leads to." | todo | |
| Both advisors go home and the cinema ends | todo | |

## Testing the vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A 60-second timer starts; the shepherd comes back when it runs out, or as soon as the player picks something up and lets go of it (or has held it for 60 seconds) | todo | `CreateTimer` and the held-object questions are stubs (`GetObjectHeld` logs "not implemented") |
| 4 seconds later, the quest waits until the vortex is in view and the camera is within 150 of it | todo | |
| What the player throws in really goes through to the next land | todo | what crosses: [../portals.md](../portals.md#what-crosses-and-what-is-left) |

## The shepherd comes back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema: the camera turns to the vortex (4 seconds); 3 seconds in, a new shepherd is made at its middle and flung out towards the camera at speed 16, the camera following him | todo | the original shepherd is never seen again; the returning one is a new villager |
| The shepherd: "Cor. There's a whole new world through there." | todo | |
| The camera pulls back to the wide view; both advisors step out and the camera looks up | todo | |
| Good advisor: "That's it. I'm going through for a look.", pointing high and then down into the vortex as he "goes through" | todo | `SpiritPointPos` is a stub |
| Evil advisor: "He's braver than I thought. He's gone through."; 2 seconds later the good advisor points high again, as if back | todo | |
| Good advisor: "The Villager was right. There's a new land through there." then "And we should send plenty of food, wood and followers into the Vortex as well." (a line shared with the fourth land's vortex) | todo | |
| Evil advisor: "You really aren't afraid of this thing, are you?" | todo | |
| Good advisor: "I say we throw things through and then go through it ourselves. Our Creature will follow. Try clicking the Action Button on the Scroll." — a second in, the gold scroll appears 20 above the vortex and he points at it | todo | `CreateHighlight`, `HighlightProperties` stubs |
| Evil advisor: "What the hell. Do we want to live forever?"; good advisor: "Well actually…" and the cinema ends | todo | |

## The scroll and leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is not recorded in the scroll log: no title, progress mark or reminder text is set for it (unlike the guide's and trainer's scrolls) | todo | nothing to show; undetermined what title, if any, the scroll shows on hover |
| Straight away and then every 30 minutes until the scroll (or its spot) is clicked, the reminder "There's a Gold Story Scroll ya gotta click on, Boss." is said | todo | the shared gold-scroll reminder; `RunText` is a stub |
| The reminder's advisor never steps out or points: the quest passes the evil advisor's voice where the reminder expects an advisor, so neither advisor matches (quirk) | todo | the reminder only calls out the good or the evil advisor; the value given is the narrator number, not an advisor |
| The prepared "Click the Action Button on the Scroll and we can go through the Vortex." line is never said (the reminder's own first notice is commented out) | n/a | cut use of an existing line |
| Once the scroll is clicked it is switched on and the creature is allowed through the vortex | todo | |
| A cinema: the camera climbs to 100 above the vortex over 6 seconds looking down, then dives into it over 8 | todo | `MoveCameraPosition` is a stub; see [../portals.md](../portals.md#going-through) |
| 5 seconds into the dive the screen fades to black over 2 seconds | partial | the fade works (`SetFade` in `src/CHLApi.cpp`, `CinematicDirectorSystem`); the quest is never reached |
| When the camera arrives, every script of the guide's story (the storm with it) is stopped and the land is told to end | partial | stopping scripts by file works (`StopScriptsInFiles`); the quest is never reached |
| With the storm's script stopped the land's control script finishes; the story stops every other script and loads the second land | todo | `LoadMap` does nothing in openblack (its body is commented out); the arrival: [../portals.md](../portals.md#arrival-scenes) and [../land_2.md](../land_2.md) |
| The creature follows the player through: in the second land it is made again at the arrival vortex | todo | see [../portals.md](../portals.md#arriving) |

## Failure, soft locks and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot fail: nothing in it reacts to the shepherd dying, the vortex being ignored or the scroll waiting; the player can stay in the first land as long as they like | todo | |
| If the storm stops existing before the scroll is clicked, the storm's script ends, the land's control script finishes and the story loads the second land at once, skipping the rest of this quest (undetermined whether the storm can end by itself) | todo | the storm loops only while it exists; whether a scripted storm ever dies on its own was not found |
| In a skipped-guide game the creature's permission to go through is set on nothing (the creature is only looked up in the scene the skip leaves out), so it is never given here (quirk) | todo | whether the creature can then still cross is decided by the vortex's own rules ([../portals.md](../portals.md)) |
| Next in the story: the second land, Khazar's ([../land_2.md](../land_2.md)) | todo | |
| No music is started by the quest; whatever the storm left playing carries on | todo | |
