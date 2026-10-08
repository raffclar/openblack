# Destroy it!

Land 2's last gold scroll: once Khazar has taught the player about influence (or has been killed by Nemesis), a gold
scroll appears on the shore west of the Indian town by the bay. Clicking it flies the camera over Lethys's three towns
(or, if they are all lost to him already, his temple) while the advisors say what must be done: win his towns and
destroy his temple. The scroll has no title of its own in the game's text; the challenge log files it under the good
advisor's line "Destroy it!", and its success is the share of Lethys's three towns he no longer holds. The land as a
whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), Lethys in
[../../rival_gods/lethys.md](../../rival_gods/lethys.md).

**Land:** 2 · **Giver:** the evil advisor, at a gold scroll on the shore west of the Indian town by the bay (Town2) · **Script:** Land2FinalScroll · **Reward:** none (finishing what it asks opens the way out: [leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md)) · **Repeatable:** no (the log entry's reminder replays the fly-over)

Sources: the quest's script source (`FinalGoldScroll.txt`) and the land's control script (`LandControl2.txt`), checked
against the compiled form in the shipped `challenge.chl`; the land's map script (`Land2.txt`) for which towns are
whose; the game's English text table; and the executable (read-only) for how the challenge log stores and updates an
entry. openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 36 script functions this quest
and the scripts it runs need, 23 still only log "not implemented" in `src/CHLApi.cpp` (among them the scroll, clicking,
town look-ups and ownership, the advisors, dialogue, the moving camera and the challenge log), and openblack never runs
Land 2's control script at all (the land-loading command does nothing, and the story always starts with Land 1's
control script, which stops long before its end; see [../land_2.md](../land_2.md) row 1), so the scroll never appears;
every row below is todo unless the notes say otherwise.

**Progress: 0/48 done, 2 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script can start this scroll in two places, and starts it only once (a land-wide "final scroll started" flag) | todo | the control script never runs in openblack (`LoadMap` does nothing; see ../../scripts/land2_script.md) |
| First place: after Khazar's gesture lesson ("Impress Village") is finished and the nearest Indian town (Town1, west of the player's home) belongs to the player, Khazar's lesson on influence plays, and as soon as it ends the scroll is started | todo | see [impress_village.md](impress_village.md) for the lesson on influence; the watcher checks continuously with no pause between checks |
| Second place: straight after Khazar's death ([nemesis_no.md](nemesis_no.md)) ends, if the scroll hasn't been started yet | todo | so the scroll comes with whichever of the two happens first |
| If the land's exit has already begun (the player has won the land and Lethys's temple vortex is open), the scroll is never made | todo | the script checks the "exit vortex done" flag before anything else |
| A gold scroll (a story highlight) appears on the ground at a fixed spot on the shore, about 190 west of the Indian town by the bay, on the way west towards Lethys's lands | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and the scroll is on screen, the evil advisor steps out, points at it and says "There's a Gold Story Scroll down here, Boss.", at most once every 30 seconds and only when no film is playing | todo | the land's own copy of the shared notify loop; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| The nagging and the wait end when the scroll (or its spot) is clicked, or when the land's exit begins; the scroll is then switched on | todo | `GameThingClicked`, `SetActive` are stubs |
| Before the film, the script counts which of Lethys's three starting towns he still holds: the near Celtic town (the one nearest the player, mid-land), the far Celtic town on the left of the fly-over's view and the one on the right (his home town, by his temple) | todo | `GetTownWithId` and the town's owner are stubs; in the map script these are towns 4, 3 and 5, all Lethys's at the start |
| If the exit began while the player was looking for the scroll, the film and the log are skipped | todo | |

## The fly-over (Lethys still holds a town)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film starts; the camera's position and aim are remembered | partial | reading the camera works (`GetCameraPosition`, `GetCameraFocus`); `StartCameraControl` and the film state are stubs |
| The camera flies over 3 seconds from near the scroll to look west across the water | todo | `MoveCameraPosition`, `MoveCameraFocus` are stubs |
| The evil advisor steps out: "All we gotta do is destroy his Temple to eradicate all belief in Lethys!", and goes home | todo | `SpiritEject`, `RunText`, `SpiritHome` are stubs |
| The camera flies over 5 seconds to look down on the near Celtic town, whoever owns it now | todo | the source comment: "Do fly over the first town irrespective of ownership" |
| After 2 seconds the good advisor steps out: "Let's move in and enlighten these poor people.", goes home, and the dialogue closes | todo | |
| Half a second later the challenge log entry is recorded from this view of the near town: a quest (gold) entry titled "Destroy it!", success = the share of the three towns Lethys no longer holds (0, ⅓ or ⅔ here), alignment 0, its reminder the fly-over itself | todo | `Snapshot` is a stub |
| If the town on the left is still Lethys's: the camera flies over 5 seconds to it, waits 3 seconds, and one of five lines is picked at random; whichever advisor owns it steps out and says it (spoken so the player can click through it), then both go home | todo | the five: evil "We've gotta get these people on our side.", evil "We'd better get this settlement believing in us.", good "Let's go for these Villagers' hearts and minds!", good "I say we drum up some faith from these people.", evil "We got to take this place, Boss. I can't stand the waiting." |
| If the town on the right (Lethys's home town) is still his: the camera flies over 5 seconds to it, waits 3 seconds, and another of the same five lines is picked, never the same one as for the left town; its advisor steps out and says it, and the good advisor goes home | todo | |
| If the last line picked was a good advisor's, the evil advisor butts in: "I want this Village! Now!" and goes home | todo | |
| The dialogue closes; the screen fades to black over 2 seconds; the camera is put back where it was; the film ends and the screen fades back in over 1 second | partial | `SetFade`, `SetFadeIn`, `FadeFinished` and setting the camera work in `src/CHLApi.cpp`; the film itself never runs |

## The fly-over (Lethys holds none of the three)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the first 3-second move, the evil advisor steps out and the camera flies straight over 5 seconds to Lethys's temple | todo | |
| After 2 seconds the evil advisor says "All we gotta do is destroy his Temple to eradicate all belief in Lethys!" | todo | he is not sent home in this branch; the end of the film puts him away (not determined how he leaves) |
| No log entry is recorded during this film; the entry first appears with the first update 5 seconds later (see below) | todo | the game makes a new entry when an update finds none, so the log does get one, with success 1; what picture it shows is not determined, since no camera view is passed |
| The same fade to black, camera reset and fade back in end it | todo | |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 5 seconds the script recounts Lethys's three towns and updates the log entry's success to the share he has lost (0, ⅓, ⅔ or 1), alignment 0, same title and reminder | todo | `UpdateSnapshot` is a stub |
| The updates go on until the land's exit begins; nothing else ends them | todo | |
| Success counts only the three towns Lethys started with; towns he wins later and his temple are not counted | todo | |
| What actually ends the land is checked by the land's control script, not this one: Lethys must hold no towns at all and his temple must be down to a tenth of its health | todo | see [leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md); how temples take damage: [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-takes-damage) |
| Along the way, once Khazar is dead and the player has taken two of the three towns (or Lethys is down to one town), Lethys steals the creature | todo | see [lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md) |
| Taking any town, or holding six, is also what makes Nemesis kill Khazar, if he hasn't already | todo | see [nemesis_no.md](nemesis_no.md) |
| Winning a town from Lethys brings a taunt from Lethys (and, while Khazar lives, praise from Khazar), from the land's town-ownership watcher | todo | not part of this quest; the watcher stops once both gods are gone. `Land2TownTakeOver` lines: see ../../rival_gods/lethys.md |

## Success, failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The log entry reaches full success when all three of Lethys's starting towns are no longer his | todo | |
| There is no failure: the scroll can't be lost, timed out or turned down | todo | |
| If the player leaves through Lethys's vortex when he steals the creature, the land ends with this scroll unfinished; the scroll's script is simply stopped with the rest of the land | todo | the story's top script stops every script but its own before loading Land 3 (`StopAllScriptsExcluding` works in openblack, but nothing gets there) |
| Losing the land (the player's temple destroyed) is the game's general game over, not part of this quest | todo | see [../losing_and_game_over.md](../losing_and_game_over.md) |

## Reward and aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no reward; the scroll only points the way | todo | |
| The scroll itself is never deleted by the script; it stays switched on until the land ends | todo | not determined whether a clicked, switched-on scroll still shows |
| Story order: it comes after Khazar's lessons (or his death) and ends with the land's exit through Lethys's temple vortex, or is cut short by following the creature through Lethys's vortex | todo | Land 3 begins with [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| It can't soft-lock on its own: the scroll needs only a click, and the land's end is checked separately; the land is stuck only if the player can't take Lethys's towns and wreck his temple | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor gives the scroll and nags about it; both advisors speak in the fly-over | todo | `SpiritEject`, `SpiritHome` are stubs |
| Whether the evil advisor's "I want this Village! Now!" plays depends on which advisor owned the last random line | todo | the "does the good advisor speak this line" check is a stub (`SpiritSpeaks`) |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script plays no music or sound of its own; only the advisors' voices | todo | |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature takes no part in this scroll | n/a | nothing in the script touches it |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The log title is an advisor's line ("Destroy it!", the good advisor's), not a scroll title; the scroll has none | todo | the title text of a log entry is just a text id; this one belongs to a line in the general advisor set |
| The log's success counts towns but the land's end also needs the temple, so the entry can read fully done while the land goes on | todo | |
| Replaying the entry from the log runs the whole fly-over again, which records the entry again with a new picture and the success of the last count | todo | the reminder is the fly-over itself, and it contains the record command |
| When Lethys has none of the three towns at the click, the evil advisor's line is the same one the town branch opens with, and no entry is recorded until the first update | todo | |
| When neither the left nor the right town is still Lethys's, the "last line" checked for the evil advisor's butt-in is still empty; what the check does then is not determined | todo | |
| The scroll's spot is checked as both the scroll and its location in the notify loop (the script passes the scroll twice) | todo | |
| The script's first record command (straight after the film) is commented out in the source; recording happens inside the film | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two flags meant to say whether the left and right towns are needed are set but read only by the fly-over itself | n/a | nothing else uses them |
