# Lethys has taken our Creature!

The kidnapping that ends Land 2's story: after Khazar's death, once Lethys is down to his last town (or the player
holds two of his three), Lethys opens a vortex by his temple, mesmerises the player's creature with a beam and walks it
into the vortex; the leash doesn't work. The advisors argue, and for a few seconds a scroll over the vortex lets the
player follow at once; miss it and the vortex closes, leaving the player to destroy Lethys's temple to open it again.
It records a gold (quest) entry in the challenge log, filed under the good advisor's line "Lethys has taken our
Creature! What are we going to do?". The land as a whole is in [../land_2.md](../land_2.md), Lethys in
[../../rival_gods/lethys.md](../../rival_gods/lethys.md#losing-the-second-land), the vortex itself in
[../portals.md](../portals.md#opening-the-exit-per-land).

**Land:** 2 · **Giver:** none (a story event at Lethys's temple; Lethys speaks) · **Script:** LethysVortex · **Reward:** none (following through the vortex takes the player to Land 3 straight away) · **Repeatable:** no

Sources: the quest's script source (`LethysVortex.txt`) and the land's control script (`LandControl2.txt`), checked
against the compiled form in the shipped `challenge.chl` (which also shows the log entry is filed under its own
challenge number); the game's English text table; and the executable (read-only) for stopping scripts by name and for
the challenge log. openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 56 script functions
this film needs, 38 still only log "not implemented" in `src/CHLApi.cpp` (among them the moving camera, dialogue, the
advisors, the computer player's hand, the vortex's fade, the beam effect, the creature-in-temple switch, the camera
path and the challenge log), and openblack never runs Land 2's control script at all (see [../land_2.md](../land_2.md)
row 1), so the kidnapping never happens; every row below is todo unless the notes say otherwise.

**Progress: 0/52 done, 9 partial — 9%**

## Is it a gold scroll?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Yes, as a story step: it records a quest (gold) entry in the challenge log, like Khazar's death, but has no gold scroll of its own | todo | `Snapshot` is a stub |
| The scroll it puts over the vortex is a silver (challenge) highlight, used only as a "follow now" button: no advisor nags about it and it gives nothing | todo | `CreateHighlight` is a stub |
| Its log entry is filed under its own challenge number, kept apart from the land's exit through the temple vortex (which shares the script file's challenge) | todo | the script sets its challenge just before recording |

## When it happens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds, but only after Khazar has died ([nemesis_no.md](nemesis_no.md)), so never on the same check as his death | todo | the control script never runs in openblack |
| It starts when the player (or anyone) holds at least two of Lethys's three starting towns (the near Celtic town, the far one and his home town), or Lethys has one town or none in all | todo | town owners and totals are stubs |
| It happens once; the control script waits for it to finish | todo | |
| If Lethys has no towns left and his temple is nearly destroyed, it still comes first, and the exit through the temple ([leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md)) is checked straight after it on the same loop | todo | |

## The kidnapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's taunt script and the rival's confront-the-player script are stopped by name, and the land is told Lethys is gone | partial | `StopScript` works in `src/CHLApi.cpp` but only for a single exact name; the game splits the name on spaces, commas and tabs and stops every script named, so openblack would stop neither here (the script passes both names in one string) |
| A film starts and Nemesis's music plays; the camera's position and aim are remembered | partial | `StartMusic`, reading the camera work |
| Lethys's computer player is paused, and his hand flies (speed 200) to above his temple | todo | pausing and moving a computer player are stubs |
| The camera is put behind his temple looking out over it; the time of day is set to 20:00 and stopped | partial | setting the camera and `SetGameTime`, `GameTimeOnOff` work |
| Lethys's hand slowly sinks to 20 above the vortex spot in front of his temple; Lethys: "You are truly a powerful god but you failed to defeat me." | todo | `RunText` is a stub |
| The camera cuts to a side view; the vortex opens there | todo | see [../portals.md](../portals.md) |
| The player's creature is moved to a spot below the temple, facing the vortex, wherever it was | partial | `SetPosition` works; `SetFocus` is a stub |
| The camera swings up over 11 seconds (its aim over 7); after 2 seconds Lethys: "Let's see how powerful you are without your Creature." | todo | |
| A magic beam runs from Lethys's hand to the creature for 60 seconds | todo | `SpecialEffectObject`, `AddSpotVisualTargetObject` are stubs |
| The creature starts walking to the vortex; its leash is switched off | partial | `SetLeashWorks` works; `MoveGameThing` is a stub |
| The creature is kept from appearing in the creature cave in the temple while it is taken | todo | `SetCreatureInTemple` is a stub; Land 3 switches it back on when the creature is freed |
| The camera cuts to look at the creature from behind and follows down over 6 seconds | todo | |
| The challenge log records a quest entry titled "Lethys has taken our Creature! What are we going to do?", success 1, alignment 0, its picture the view of the walking creature, no reminder line | todo | `Snapshot` is a stub; the source comment: "Snapshot here because it's a big story bit. No reminder though." |
| The evil advisor appears: "Look! He's heading for the Vortex! He's mesmerised!"; the good advisor appears: "The Creature! Get him on a Leash quickly!"; both disappear | todo | `SpiritAppear`, `SpiritDisappear` are stubs |
| The creature is made unable to die, so being hurt can't send it home to the temple halfway | partial | developer function 9 works (`DevFunction` sets `canDie` in `src/CHLApi.cpp`); the land never gets here |
| The film ends, handing control back while the creature walks | todo | |

## What the player can do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has control while the creature walks to the vortex; nothing they do stops it | todo | |
| If the player puts the creature on a leash, the good advisor says once: "The Leash isn't working." | partial | `IsLeashed` and the leash switch work; dialogue is a stub |
| The walk ends when the creature is within 5 of the vortex, or no longer exists | todo | |
| The creature can die again from then on | partial | developer function 8 works |

## Following

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 1.5 seconds Lethys's hand sinks into the vortex (to the ground, speed 20); after 2 more a second film starts | todo | |
| The camera looks at the vortex from the south-west and swings round over 36 seconds (its aim, 20 above the vortex, over 20) | todo | |
| The evil advisor appears: "NO! He's gone through! And so has Lethys!"; the good advisor appears: "Lethys has taken our Creature! What are we going to do?"; evil: "We're going to get him back. Let's go through!"; good: "Yes! No! Let's think about this. It could be a trap."; the music stops | todo | |
| The film ends but the dialogue goes on, and the silver scroll appears 20 above the vortex | todo | |
| Evil: "No! There isn't time to think! The Vortex is closing!"; good: "But we don't know what's on the other side!"; evil: "Our Creature is on the other side! That's all we need to know. Let's go!"; both disappear | todo | |
| The player has 10 seconds after these lines (plus however long the three lines take) to click the scroll or the vortex | todo | `GameThingClicked` is a stub |
| Clicking in time: the land is marked as being left; the camera flies to the start of the exit camera path over 4 seconds, runs the path, and after 2 seconds fades to black over 2 seconds | todo | `RunCameraPath` is a stub; the same path as the temple-vortex exit; see [../portals.md](../portals.md#going-through) |
| Game time starts again, the land's control loop ends and the story moves on to Land 3 | todo | the story's top script then stops every other script and loads Land 3; `LoadMap` does nothing in openblack |

## Missing the chance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the player hasn't clicked within the 10 seconds, the chance is marked missed: the scroll stops waiting, the vortex fades out and game time starts again | todo | `VortexFadeOut` is a stub |
| The evil advisor appears: "All we gotta do is destroy his Temple to eradicate all belief in Lethys!", goes home and disappears | todo | |
| The scroll is not deleted by the script | todo | not determined whether a scroll left switched on still shows |
| From then on the only way out is to take every one of Lethys's towns and wreck his temple to a tenth of its health ([leave_through_the_vortex_land_2.md](leave_through_the_vortex_land_2.md)) | todo | how temples take damage: [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-takes-damage) |
| Lethys's computer player stays paused for the rest of the land: no land 2 script resumes it | todo | so he can't win back towns while the player finishes him off |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has no creature for the rest of Land 2 and arrives in Land 3 without it, whichever way they leave | todo | Land 3: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| Lethys's taunts end (his taunt script stops when it sees he is gone) | todo | see ../../rival_gods/lethys.md |
| Once both gods are gone, the land's town-ownership watcher, which made them comment on towns changing hands, stops | todo | |
| It can't fail; missing the click only means the longer way out. It can't soft-lock unless Lethys's temple can't be brought down | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors appear and disappear in place rather than stepping out of their corners | todo | the script uses "appear"/"disappear", not "eject"/"send home"; `SpiritAppear` is a stub |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays from the start of the first film until the end of the second | partial | `StartMusic`, `StopMusic` work; the film never runs |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is moved, made to walk into the vortex, can't be led, can't die while walking, and can't appear in the temple | todo | see the rows above |
| What the vortex does with the creature when it arrives is not done by this script | todo | see [../portals.md](../portals.md#being-sucked-in) |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script stops "LethysTauntAI, ComputerPlayerConfrontPlayer" as one string; the game splits it, so both stop, but the script that actually voices Lethys's taunts (run by the taunt script, and in the background by the town-ownership reactions) is a third one, which is not named | todo | the confront-the-player script named is Khazar's lesson helper on this land; not determined whether a taunt already playing is cut off with the taunt script that called it |
| The vortex here is 12 away from the one the temple exit opens, though the source comment says to keep them the same; both use the same recorded camera path | todo | |
| The creature's walk is checked in a loop with no pause | todo | |
| The window to follow is not a fixed 10 seconds: it also covers the three closing lines, however long the player takes to read them | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script that would ring Lethys's temple with five lightning storms (time 80, no wind, heavy forks) is written but its start is commented out | n/a | never started by the game |
| A high camera spot is set up but never used; the good advisor's "No!" is in the text table but unused | n/a | |
| Pointing the advisors at the creature is commented out | n/a | |
