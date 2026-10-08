# Leave through the vortex (Land 2)

The way out of Land 2 when the player missed following Lethys through his vortex: once Lethys holds no towns and his
temple is all but destroyed, the temple blows up, the vortex hidden under it opens again, the advisors say they must
follow the creature, and a gold scroll over the vortex takes the player through to Land 3. The game gives this scroll
no title and records no challenge log entry for it. The vortex itself (how it looks, sounds and what crosses) is in
[../portals.md](../portals.md#opening-the-exit-per-land); the land as a whole in [../land_2.md](../land_2.md).

**Land:** 2 · **Giver:** the good advisor, at a gold scroll over the vortex where Lethys's temple stood · **Script:** LeaveThroughVortexL2 · **Reward:** none (the way to Land 3) · **Repeatable:** no

Sources: the quest's script source (`LeaveThroughVortexL2.txt`) and the land's control script (`LandControl2.txt`),
checked against the compiled form in the shipped `challenge.chl`; the shared scroll-notify script; the land's map
script (`Land2.txt`); the game's English text table. openblack's state is judged on the physics work tree
(`ob-wt-physics`): of the 33 script functions this quest needs, 23 still only log "not implemented" in
`src/CHLApi.cpp` (among them the scroll, clicking, deleting the temple, the moving camera, the camera path, the
advisors and dialogue), and openblack never runs Land 2's control script at all (see [../land_2.md](../land_2.md) row
1), so the scroll never appears; every row below is todo unless the notes say otherwise.

**Progress: 0/28 done, 2 partial — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds, while the land isn't already being left: Lethys must hold no towns at all and his temple must be at a tenth of its health or less | todo | town totals and the temple's health are stubs (`GetProperty`); how temples take damage: [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-takes-damage) |
| It runs once (an "exit vortex done" flag), and the control script waits for it | todo | |
| In normal play it comes only after Khazar's death and Lethys's theft of the creature, when the player didn't follow him in time | todo | the theft is checked earlier in the same loop and would start first; see [lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md) and the quirk in [nemesis_no.md](nemesis_no.md#script-quirks) |
| Setting the "exit vortex done" flag ends the land's last gold scroll's updates and its nagging | todo | see [destroy_it.md](destroy_it.md) |
| Not determined what the temple-health check reads if the temple has already been destroyed outright | todo | the control script looks the temple up once, when the land starts |

## The temple falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film starts with the camera close in front of Lethys's temple, drifting slowly towards it over 12 seconds | partial | setting the camera works; `MoveCameraPosition`, `StartCameraControl` are stubs |
| Lethys's temple is blown up | todo | deleting a temple with its explosion is a stub (`ObjectDelete`); see [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-is-destroyed) |
| When the camera arrives, it cuts to a high view behind the temple site looking at the vortex spot, and sinks over 10 seconds | todo | |
| The vortex opens in front of where the temple stood; the camera flies over 8 seconds to look out from it over the land | todo | see [../portals.md](../portals.md) |
| Both advisors step out; good: "Look! The Vortex has opened again!"; good: "I see! Lethys' Temple was hiding a Vortex. Let's go through!"; good: "We've got to get our Creature back."; evil: "Yeah. The poor guy. I really miss him."; good, clickable through: "We've no choice. We must follow him through the Vortex." | todo | `SpiritEject`, `RunText` are stubs; the source comment for the second line reads "Now that Lethys belief in this land is no longer protecting the vortex." |
| Both go home and the film ends | todo | |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears 20 above the vortex | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of it and it is on screen, the good advisor steps out, points at it and says "Action Button the Scroll and we'll get through the waiting Vortex.", at most once every 30 seconds and only when no film is playing | todo | the shared notify loop |
| The player can wait as long as they like; the land goes on meanwhile | todo | |
| Clicking the scroll or the vortex switches the scroll on and leaves | todo | `GameThingClicked`, `SetActive` are stubs |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the camera flies to the start of the exit camera path over 4 seconds, runs the path, and after 2 seconds the screen fades to black over 2 seconds | partial | `SetFade` works; `RunCameraPath` is a stub; same path as following Lethys straight away; see [../portals.md](../portals.md#going-through) |
| The vortex is marked closed and the land is marked as being left; the land's control loop ends | todo | |
| The story's top script stops every other script and loads Land 3, which begins with the arrival through the vortex | todo | `StopAllScriptsExcluding` works, `LoadMap` does nothing in openblack; Land 3: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| No villagers are sent through by this script; who follows the player is the vortex's business | todo | see [../portals.md](../portals.md#what-crosses-and-what-is-left) |

## Success, failure and aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No challenge log entry is recorded for this scroll | todo | the script has no record command |
| It can't fail; it waits for the click for ever | todo | |
| It can soft-lock only before it starts: if Lethys keeps a town or his temple can't be brought below a tenth of its health, the land never ends | todo | Lethys's computer player is left paused after the theft, so he shouldn't win towns back |
| What it unlocks: Land 3 | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor does most of the talking and gives the scroll; the evil advisor has one line | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script plays no music or sound of its own | todo | the temple's explosion and the vortex have their own sounds; see ../portals.md |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has no creature here: Lethys has taken it | n/a | the script doesn't touch a creature |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is never deleted; the land ends with it | todo | |
| The vortex is 12 away from the one Lethys opened, though a source comment asks for them to be kept the same | todo | |
| A source comment on the second line describes a different line ("Now that Lethys belief in this land is no longer protecting the vortex") from the one spoken | todo | |
