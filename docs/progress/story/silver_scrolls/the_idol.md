# The Idol

A land 2 silver scroll: a man of the nearest town (an Indian one) has carved a huge idol out in the hills and kneels before it,
and the advisors want it gone. The idol cannot be broken; it has to be kept burning for about half a minute, after which
it explodes and the town is given the stronger fireball miracles. The man and the worshippers who drift out to join him
can be spared or killed, which only changes the alignment the challenge log records. The land as a whole is in
[../land_2.md](../land_2.md), the land's control script in [../../scripts/land2_script.md](../../scripts/land2_script.md).

**Land:** 2 · **Giver:** the good advisor, at a silver scroll over the idol in the hills west of the nearest town (Town1, Indian, id 12) · **Script:** IdolPyre · **Reward:** the fireball miracle's first and second power levels at that town's worship site · **Repeatable:** no

Sources: the quest's script source (`IdolPyre.txt`, checked line by line against the PC game's compiled `challenge.chl`,
including how each condition compiles), the land's control script for the trigger, the shared notify and reminder
helpers, the game's English text table for every line, and the progress notes on the fire model and town artefacts.
openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 64 script functions this quest and the
notify helper need, 43 still only log "not implemented" in `src/CHLApi.cpp` (among them the scroll, dialogue, advisors,
camera moves, flocks, properties such as health and scale, hits, deleting and the miracle reward). The fire question
and setting something on fire do work (`IsFireNear`, `SetOnFire`), as do the no-move and no-pick-up flags, but the land
2 control script stops at its first unwritten command, so the quest never starts; every row is todo unless the notes
say otherwise.

**Progress: 0/108 done, 4 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds; the quest starts once the player owns Town1 (town id 12, the nearest town and the one used for learning gestures; the control script's comment calls it Norse, but the map script makes it Indian) and owns more than five towns in all | todo | the land 2 control loop never gets this far (see [../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| It starts once only (a done flag is set as it starts) and runs in the background, so the rest of the land carries on | todo | |
| If Town1 is later lost, nothing stops the quest once it has started; if Town1 is lost before the six-town mark, the quest waits until both hold again | todo | the condition is rechecked each 9-second pass until it first holds |
| A separate one-line test script that just runs the quest exists in the sources but is not in the shipped program | n/a | test scaffolding only (not compiled into `challenge.chl`) |
| As it starts, the idol appears straight away out in the hills about 250 m west-north-west of the town centre: the game's idol object, scaled to four times its size, facing far to the south | todo | `CREATE` makes mobile statics, so the idol itself would be made (`CreateScriptObject` in `src/CHLApi.cpp`), but the script never runs; the scale is a property (`SetProperty`, stub) |
| The idol can't be moved or picked up by the hand | partial | `SetIdMoveable` and `SetIdPickupable` work in `src/CHLApi.cpp`, but there is no idol to apply them to |
| Because the script holds it, the idol can never become a town artefact (and it can't be picked up to place anyway) | todo | see [../../town/artefacts.md](../../town/artefacts.md) (the idol's artefact value exists in the tables but scripted objects never qualify) and [../../nature/one_shot_features.md](../../nature/one_shot_features.md) |
| A silver scroll is raised at the idol | todo | `CreateHighlight` is a stub; see [../../interface/scrolls_and_signs.md](../../interface/scrolls_and_signs.md) |
| Nobody stands at the idol yet: its maker only appears once the scroll is clicked | todo | he is made inside the introduction |
| While the scroll waits, whenever the camera is within 100 of it and it is on screen, at most every 30 seconds and only when no other scene holds the screen, the good advisor steps out, points at it and says "Look. Something for you to do here." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll or the idol's spot starts the challenge and the scroll shows it is active | todo | `GameThingClicked`, `SetActive` are stubs |
| Anything done to the idol before the scroll is clicked (fire, blows) counts for nothing: the fire watch only begins after the introduction | todo | |
| Leaving land 2 stops the quest wherever it has got to, with every other script | todo | the all-lands control stops all scripts before loading the next land |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts (widescreen, the player's control taken) with the Gregorian chant music | todo | `StartMusic` works in openblack, but the scene commands around it are stubs |
| The screen fades to black over 2 seconds and stays black for 3 | todo | `SetFade` works; nothing reaches it |
| The idol's maker appears beside it, about 12 m in front of it: an Indian trader villager (not the special idol-builder villager, which the game uses on land 5 instead), facing the idol and praying 23 times over | todo | creating a villager from a script does nothing (`CreateScriptObject` only makes fixed objects); `SetFocus`, the play-animation state are stubs |
| The camera is placed low beside him looking up at the idol, its lens at 80 widening to 87 over 7 seconds, while the picture fades in over 3 seconds | todo | `MoveCameraLens`, camera placement in a scene; see [../../camera/script_camera.md](../../camera/script_camera.md) |
| After 4 seconds the lens closes to 70 over 9 seconds as the camera rises and turns down onto the maker (7 seconds and 4 seconds) | todo | `MoveCameraPosition`, `MoveCameraFocus` are stubs |
| Both advisors step out | todo | `SpiritEject` |
| Evil advisor: "Wait up! What the heck is this?" | todo | the source's comment marks this as a good-advisor greeting; the text table gives it to the evil advisor |
| Evil advisor: "Outrageous! He's built an idol to worship. Punish him!", then a 2-second pause | todo | |
| The camera drops again and swings round past the idol (7 seconds and 11 seconds) | todo | |
| Good advisor: "This could start a new religion. We really should remove this idol." | todo | |
| Evil advisor: "Yeah, destroy it and the people! Ungrateful peasants!", then a 2-second pause | todo | |
| The camera pulls back and up to look at the idol (7 and 6 seconds); both advisors go home and the dialogue closes | todo | `SpiritHome`, `GameCloseDialogue` |
| When the camera has arrived, the challenge is entered in the player's challenge log: the snapshot command records the challenge with its title "The Idol", a picture of the view, a success of 0 and an alignment of 0, and the reminder line the scroll will repeat | todo | `Snapshot` is a stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| The reminder is the good advisor's "This could start a new religion. We really should remove this idol.", spoken by the advisor who owns the line, who steps out to say it | todo | the shared reminder script |
| The music stops and the scene ends | todo | `StopMusic` works; nothing reaches it |
| The idol then chants: a Gregorian piece fixed to it is heard around it as the player nears | todo | `AttachMusic` is a stub; see [../../audio/music.md](../../audio/music.md) |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keep fire burning within 30 m of the idol for long enough; nothing else finishes the challenge | partial | the fire question itself works (`IsFireNear` in `src/CHLApi.cpp`, `FireSystem::IsFireNear`; see [../../physics/fire.md](../../physics/fire.md) and [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md)), but the quest never runs |
| Any fire counts, not the idol's own: a burning tree, hut, villager or hot rock within 30 m of the idol's spot all do, and so do the maker and worshippers set alight, since they stand inside that radius | todo | the check asks only "is anything on fire near this place" |
| The fire can come from the fireball miracle, lightning, a hot rock or the creature: the script doesn't care who lit it | todo | |
| The idol can't be broken: whenever its health falls below 1 it is put back to 1, until the fire has done its work | todo | `GetProperty`/`SetProperty` (health) are stubs |
| Hitting it (throwing things at it, a creature's blows) does nothing to it beyond, after enough blows, an advisor's hint | todo | |

## Rules, timers and counts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script makes one pass of its watch each game turn (a tenth of a second) | partial | openblack's script machine yields on each loop back as the game does (`LHVM` jump opcodes), and `ClearHitObject` works, but the watch is never reached |
| Every 21 passes (about 2.1 seconds) it looks for fire within 30 of the idol's spot | todo | |
| Each look that finds fire adds one to a count; once the count passes 15 (16 looks in a row, so fire for about half a minute, at least 31.5 seconds from the first look that saw it) the idol is done for | todo | |
| A look that finds no fire resets the count to 0 | todo | so a fire that flickers out and is relit must start over |
| If the fire had been seen at two or more looks in a row and then goes out, the evil advisor steps out: "We gotta keep the fire going for longer than that, Boss." and goes home | todo | no limit: said every time a run of two or more looks breaks |
| A fire seen at only one look and then gone resets silently | todo | |
| On a look with no fire (and no fire run to comment on), if the idol has been hit since the last look, the hit is counted and the hit record cleared | todo | `GameThingHit` is a stub; openblack's physics does keep a last-hit record (`DynamicsSystem::RecordHit`, see [../../physics/impact_damage.md](../../physics/impact_damage.md)) |
| The third counted hit brings the good advisor: "Leader, battering the idol isn't working. There has to be another way." and after that hits are no longer watched | todo | at most one hit is counted per look, so it takes three blows on three separate looks |
| Hits during a fire run, or on the look that comments on a fire going out, aren't counted | todo | the hit test sits only in the no-fire branch |
| Whether a hit on some other object between two looks wipes out an idol hit before it is counted | todo | undetermined: depends on how the game keeps its last-hit record |
| The maker's death is watched every pass until the idol is done for | todo | |
| The town the worshippers come from is the town within 150 of a spot by Town1's centre | todo | `CallNear` is a stub |

## The worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An empty worshipper flock is made at the maker's spot, held within 5 to 20 of its centre | todo | `FlockCreate`, `ChangeInnerOuterProperties` are stubs |
| Every 1801 passes (about 3 minutes) of the watch, while fewer than 8 worshippers are in the flock and fewer than 5 have been killed, more villagers of the town (any not held by another script) set out to worship | todo | `CallIn` ("get villager in town excluding scripted") is a stub |
| The first time two set out at once; after that one at a time | todo | |
| Each walks slowly (speed 0.5) to its own random spot within 8 m either way of the maker | todo | `SetProperty` (speed) and `MoveGameThing` are stubs |
| On arriving each joins the flock, turns to the idol and prays once | todo | `FlockAttach`, `SetFocus`, play-animation state |
| Arrival is tested as standing exactly on the chosen spot (a distance of exactly 0) | todo | the shipped program compiles the "at" test that way; undetermined whether a walker's end position ever matches the spot exactly, and so whether worshippers ever join the flock in the real game |
| A walker that hasn't arrived when the next one is sent is forgotten by the script: it is never added to the flock or counted if it dies | todo | the script keeps one walker slot (two for the first pair) and overwrites it |
| Walkers still on their way count towards the kill tally if they die (each death counted once); flock members count when the flock shrinks | todo | |
| A death of any cause counts as the player's kill: worshippers burnt by the fire lit to destroy the idol count too | todo | the tally only looks at flock size and the walkers' health |
| The first worshipper death brings the good advisor: "Oh dear. You've killed a worshipper." (once) | todo | |
| The fifth death brings the good advisor: "Horrific. You've killed loads of worshippers." (once), costs 0.4 of the challenge's alignment, and stops any more worshippers being sent | todo | the alignment is only written to the log when the idol burns, or with the maker's death |

## The idol's maker

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He stays at the idol for the whole challenge; the script never moves him until the end | todo | |
| If he dies before the idol is done for, by any cause, both advisors step out | todo | |
| Good advisor: "It's rather sad that we lost a follower. And he lost his life." | todo | the source comment has the good advisor saying "You've killed the Idol Builder."; the line used is the sad one |
| Evil advisor: "We still gotta get rid of the idol. It'll attract people!" | todo | |
| This costs 0.6 of the challenge's alignment and the log is updated to a success of 0.5 with that alignment (so killing him is logged as half the job) | todo | `UpdateSnapshot` is a stub |
| The advisors aren't sent home by the script after these two lines; the dialogue just ends | todo | undetermined whether ending the dialogue sends them back |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once the fire has lasted, there is a further 1-second pause, then the idol is set burning for good | partial | `SetOnFire` works (`FireSystem::SetOnFire`), but nothing reaches it |
| The script then waits until the idol's spot is on screen with the camera within 150 of it, however long that takes | todo | `PosFieldOfView` is a stub |
| A scene starts with the Gregorian chant again; the camera moves in beside the idol (4 and 3 seconds) | todo | |
| When it arrives, the challenge's alignment gains 0.2 and the challenge log is retaken with a success of 1 and the alignment so far (a fresh snapshot, so a new picture of the burning idol) | todo | `Snapshot` |
| The camera slowly drifts (20 seconds); the good advisor: "The idol has caught fire. It seems to be working!", then goes home | todo | |
| The camera moves close in (3 seconds); after 3 seconds a bang bursts at the idol | todo | `SpecialEffectPosition` is a stub |
| 0.3 seconds later a flash 3 m above it; 0.2 seconds later the idol is removed | todo | `ObjectDelete` is a stub |
| A second later another flash and the guardian-stone explosion sound; 0.6 seconds later a bang, and 0.25 seconds after that another | todo | `PlaySoundEffect` is a stub; see [../../audio/sound_effects.md](../../audio/sound_effects.md) |
| The last flash and two bangs are placed at an idol that has already been removed | todo | undetermined where the game puts an effect at a deleted object (probably its last place) |
| After 3 seconds, if the maker is alive the camera goes to him (from 14 m off and 10 up, 4 and 3 seconds); he turns to the camera and prays once | todo | |
| If no worshippers are in the flock, the maker: "Please don't kill me. I'll bow down before you." (the camera drifts while he finishes praying) | todo | spoken by the man's voice |
| If any are, they too turn to the camera and pray, and the maker: "Please spare us! We'll worship you loads!" | todo | |
| A 2-second pause after his line | todo | |
| If the maker is dead, the worshippers (if any) turn to the camera and pray, with no line | todo | |
| The camera returns to where the player had it (3 and 2 seconds); the worshippers are released to their town life and the scroll is removed | todo | `ReleaseFromScript`, deleting the highlight |
| If the maker lives he walks back to his house in the town; evil advisor: "So you didn't feel our idol-maker was worth punishing? Never mind. You're the boss, I suppose." | todo | the source comment calls this a good-advisor line; it is the evil advisor's |
| If he is dead, the good advisor repeats "It's rather sad that we lost a follower. And he lost his life.", the alignment loses another 0.6 and the log is updated (success 1, that alignment) | todo | see the quirks below |

## Alignment and success values

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll: logged at success 0, alignment 0 | todo | |
| The maker killed while the idol stands: success 0.5, alignment −0.6 (−1.0 if five worshippers had already died) | todo | |
| Burning the idol and sparing everyone: success 1, alignment +0.2 | todo | |
| Burning the idol after killing five or more worshippers, maker alive: success 1, alignment −0.2 | todo | |
| Maker killed before the idol burns: −0.6, +0.2, then −0.6 again at the end: success 1, alignment −1.0 (−1.4 with five worshippers dead) | todo | |
| Maker dying after the idol is done for but before the closing scene checks (for example burnt by the idol's own fire while the script waits to be seen): no lines then, +0.2 −0.6: success 1, alignment −0.4 (−0.8 with five worshippers) | todo | |
| Leaving the idol alone: the challenge stays in the log at success 0 (or 0.5 if the maker died) and never ends | todo | |
| How the logged alignment then moves the player's own alignment | todo | see [../../worship/alignment.md](../../worship/alignment.md) and [../challenges_and_rewards.md](../challenges_and_rewards.md) |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no failure: no timer, no limit on attempts, no loss of the town ends it | todo | |
| The quest can be left forever; the idol keeps chanting and worshippers keep coming (up to 8 in the flock) | todo | |
| Killing the maker doesn't end it either: the idol must still burn | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera moves over the town near its centre (8 seconds) | todo | |
| The town is given the fireball miracle's first and second power levels, so its worship site can cast the stronger fireballs (three balls at the first power-up, eight at the second) | todo | `SetMagicInObject` is a stub; see [../../miracles/fireball.md](../../miracles/fireball.md) and [../rewards.md](../rewards.md) |
| Good advisor: "Ooh. By destroying the idol we've gained a more powerful Fireball Miracle." | todo | |
| Evil advisor: "It looks better than the normal Fireball, that's certain." | todo | the advisors aren't sent home by the script before the scene ends |
| The scene ends | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The idol is gone for good and its chant with it | todo | |
| The maker, if alive, lives on in the town as an ordinary villager; the worshippers go back to town life | todo | |
| The script never stops the Gregorian music it started for the closing scene | todo | undetermined whether the scene's end or the game stops it |
| Nothing else in the land reads the quest's outcome (its finished flag is used only inside the script) | todo | the flag is global but no other script reads it |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part in the script: it can start the fire (a fireball, a burning object) or kill the maker and worshippers, and all of it counts the same as the player's doing | todo | the script tests only fire, deaths and hits, never who caused them |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Killing the maker before the idol burns costs the alignment twice (0.6 when he dies and 0.6 again in the closing scene) and the sad line is heard twice | todo | |
| The closing line for a dead maker carries a developer note asking whether the dialogue is wanted at all ("Is this dialogue wanted? The snapshot challenge probably is."); its comment expects a gleeful "You killed the Idol maker too. Ideal.", which matches the unused evil line "Ha, you destroyed the idol, and its maker. That rocks!" rather than the sad line used | todo | |
| Source comments name the wrong advisor for three lines (the greeting, the maker's death line, the "worth punishing" line) | n/a | comments only; the game plays what the text table gives |
| The second walker's death test checks whether the walker itself is "2" instead of its killed flag, a slip that has no effect because the walker slot is never 2 | todo | |
| "Fire near the idol" is a place test, so the fire that kills the maker or worshippers can be what completes the challenge | todo | |
| The flock size counts as kills any worshipper who leaves the flock in any way, not only deaths | todo | undetermined whether flock members can leave other than by dying |
| A worshipper "arrived" test needing an exact match of positions may never be met | todo | see the worshippers section |
| The idol's scale is read twice in the compiled program for a single "four times bigger" (a compiler quirk with no visible effect) | n/a | no effect a player can see |

## Unused and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction once ended with the evil advisor's "Ha, you destroyed the idol, and its maker. That rocks!" and "We still gotta get rid of the idol. It'll attract people!" (marked "Kill him" and "Kill him and Idol"), then both advisors going home; it is commented out, and the first line is used nowhere | todo | the second line is reused for the maker's death |
| The challenge log entry was first taken in the middle of the introduction, before the advisors spoke; that line is commented out and the entry is taken after the camera settles instead | n/a | cut |
| A further camera move at the end of the introduction is commented out | n/a | cut |
| The idol was to push out an "anti-influence" of radius 50 (keeping the player's influence away from it), removed when it exploded; all three lines are commented out | todo | the anti-influence command; see [../../worship/influence.md](../../worship/influence.md) |
| A developer's temporary test ("gj temp") that blew the idol up 4 seconds after it was made, with the same bangs and flashes as the final scene, is commented out | n/a | cut test code |
| The idol's text lines are numbered 01 to 20 but 07, 08, 14, 15 and 16 do not exist in the text table; of those that do, only 05 is never used | n/a | text table checked |
| The special idol-builder villager kind is not used here; this maker is an Indian trader | n/a | the idol-builder villager appears on land 5 |
