# Impress Village

Khazar's second lesson on the second land, logged as the gold scroll "Impress Village": twenty minutes after the
worship lesson Khazar lays a scroll by the village and teaches casting by gesture (a spiral to open the miracle
selection, then the miracle's own gesture), then takes the player to the nearest Norse village, empties its store and
has the player fill it with food while he gives it wood. When that village is won, Khazar comes back for a lesson on
influence, which is not a scroll of its own but is covered here. The land as a whole is in [../land_2.md](../land_2.md),
the land's script in [../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Khazar (the friendly god's hand), at a scroll west of the player's village · **Script:** LearnGestures (then LearnInfluence) · **Reward:** none; the store-filling wins the Norse village over, and winning it brings the lesson on influence and the land's last scroll · **Repeatable:** no

Sources: the quest's script sources (`LearnGestures.txt`, `LearnInfluence.txt`), the land's control script
(`LandControl2.txt`, its expansion and influence watchers), the shared Land 2 scroll notifier (`SetupLand2.txt`) and the
hand demo scripts (`HandDemos.txt`), checked against the compiled scripts in the shipped `challenge.chl`; the game's
English text table (`InfoScript2.txt`) and hand demo recordings (`Data/HandDemo`). openblack's state is judged on the
physics work tree (`ob-wt-physics`): of the 69 script functions the two scripts and the scripts they run need, 52 still
only log "not implemented" in `src/CHLApi.cpp` (among them scrolls, dialogue, advisors, Khazar's hand and actions, hand
demos, gesture event counts, worship site power, the town store, the challenge log, the moving camera and camera tracks).
openblack never runs Land 2's control script at all (see
[../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never appears; every row is todo unless
the notes say otherwise.

**Progress: 0/66 done, 8 partial — 6%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When Khazar's worship lesson is done ([worship_site.md](worship_site.md)), the control script starts an expansion watcher alongside The Workshop | todo | the land control script never runs in openblack |
| The watcher sets a 20-minute timer and checks every 3 seconds; once the time is up and the player still owns the home village, the quest starts, once | todo | `CreateTimer`, `GetTimerTimeRemaining` stubs |
| Starting it also marks that the gestures lesson has been triggered, which changes Khazar's idle remarks from "build more" ones to ones that include "take over the land" | todo | Khazar's idle remarks are in the land's computer player script |
| The quest's village is the nearest Norse village to the player (the one the land script also uses for the idol scroll), and its store is the storage pit within 20 of a fixed spot in it | todo | `GetTownWithId` is a stub |
| Its target is twice the cost of a grain miracle, used to keep the worship site able to pay for one | todo | `GetManaForSpell` stub |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Norse village's store is emptied of all food and wood straight away, before the scroll appears | todo | `RemoveResource` stub; see [../../town/storehouse.md](../../town/storehouse.md) |
| Khazar's hand flies (speed 200, fixed height) to a spot west of the player's village; a gold scroll appears there on the ground; 2 seconds later his hand is let go | todo | `MoveComputerPlayerPosition`, `CreateHighlight` stubs |
| The good advisor nags it while the camera is within 100 and it is on screen, at most every 30 seconds: "Look. One of Khazar's Scrolls. Let's click on it." | todo | the Land 2 scroll notifier; also stops if Khazar is gone |
| The quest waits for the scroll (or its spot) to be clicked | todo | `GameThingClicked` stub |

## Khazar's introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's own computer player is paused for the lesson | todo | `EnableDisableComputerPlayer2` stub |
| A widescreen film with Khazar's music; the camera pulls up over the village and his hand flies in (speed 300), then comes to 20 in front of the camera | partial | `SetWidescreen`, `StartMusic` work; the moving camera and Khazar's hand don't |
| Khazar: "You have become well established." · "Our combined strength is becoming greater." · "As you near Lethys it will become more essential to have your Miracle skills in top form." (the camera drifts and his hand moves slowly aside) · "For this you must experience the benefit of gestures." | todo | `RunText` stub |
| The interface is limited to moving the hand and the film ends | partial | `SetInterfaceInteraction` works in openblack; never reached |

## The gesture hand demo

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any miracle the player is charging is cancelled; a second film with Khazar's music turns the camera west, Khazar's hand drifts in | todo | `ClearPlayerSpellCharging` stub |
| Khazar: "You can charge Miracles without having to return to your Temple to select them" | todo | |
| The worship site's power is raised to twice a grain miracle's cost if it has less | todo | `GetMana`, `GameSetMana` stubs |
| The "gestures" hand demo plays, pausing at its marks: "Move your hand in a spiral like this." · "Now select the Miracle of your choice. " · "Grain is selected like this." | todo | `PlayHandDemo`, `HandDemoTrigger` stubs; recording `Data/HandDemo/Gestures.hnd`; see [../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md) |
| The dialogue closes and the log entry "Impress Village" is recorded at 0; tapping the entry in the log plays this whole gesture demo again, instead of an advisor's reminder | todo | `Snapshot` stub |
| When the demo ends, Khazar (one line, the player may act): "All your Gestures are summarised at the bottom right of the screen." | todo | |
| The music stops and the film ends into dialogue; the player's counts of spirals drawn and miracles picked by gesture so far are noted | partial | `StopMusic` works; `GetTotalEvents` stub |
| Khazar (one line): "You try it now." | todo | |

## Practising the gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze info sign on gestures appears nearby (power-ups, the spiral, leash and creature miracle gestures, repeating the last miracle) | todo | the info sign script |
| The player has 5 minutes to draw the spiral and pick a miracle by its gesture; both counts must have gone up | todo | `GetTotalEvents`, `CreateTimer` stubs; the miracle selection isn't ported ([../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md)) |
| Every second while waiting, any miracle being charged is cancelled and the worship site's power is topped up to twice a grain miracle's cost, so the player can always afford the miracle | todo | |
| When the 5 minutes run out, Khazar (one line): "Well, it doesn't matter if you can't do it now." and the lesson moves on without the praise | todo | |
| The interface goes back to normal | partial | `SetInterfaceInteraction` works; never reached |
| If the player did it, a film: 1.5 seconds, then Khazar: "Superb. " | todo | |

## Going to the Norse village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The village's store is emptied of food and wood again and the scroll moves to the Norse village | todo | |
| If the village still exists: Khazar: "You should practice your gestures. Go and impress this town over here." as the camera flies there and his hand goes to above the store | todo | |
| The log entry is recorded at 0.5 with the good advisor's reminder "You need to fill up the Village Store with food." | todo | |
| Khazar: "They look like they're in a bad way. No food or wood." | todo | |
| If the store exists: Khazar: "Why don't you create some food for their Village Store, while I supply them with wood." (the player may act), then "Make sure you have people worshipping at your Worship Site." | todo | |
| His hand is let go and the film ends | todo | |
| If Khazar's temple has a wood miracle, it is filled with enough power for six wood miracles and he is told to cast wood on the village four times | todo | `GetSpellIconInTemple`, `QueueComputerPlayerAction` stubs |

## Filling the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 seconds the store's food is read and the log entry is updated to 0.5 plus half the share of 2,800 food it holds | todo | `GetResource`, `UpdateSnapshot` stubs |
| The player fills it by casting grain on the store or dropping food in it (any way of adding food counts) | todo | the script only reads the store |
| When it holds more than 2,800 food (the wood is not checked; a wood check is commented out) Khazar is let go and, if he is still alive, a film: his hand is put by the store and the camera sweeps round it | todo | |
| If the village is not yet the player's, Khazar: "Excellent, we have filled the Village Store and the people are easily impressed." then "The town needs more impressing before it becomes yours. But I have matters to attend to and shall return later."; this is remembered for the influence lesson | todo | |
| If it already is the player's, only the first line, as a one-line dialogue | todo | |
| The step also ends, without the film, if the village or its store no longer exists | todo | |
| The log entry "Impress Village" is recorded at 1 (complete), now with the evil advisor's reminder "You need to impress this Village." | todo | the entry is complete before the village is won |
| The quest then waits until the village is the player's (or no longer exists), and only then marks the gestures lesson done | todo | see [../../town/belief_and_conversion.md](../../town/belief_and_conversion.md) |

## Khazar's lesson on influence

Not a scroll: no highlight and no log entry. The control script's influence watcher, started with The Workshop, waits
for the gestures lesson to be done and the Norse village to be the player's, then runs it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The influence watcher checks for both conditions over and over without any pause | todo | script quirk |
| If Khazar is alive: a widescreen film with Khazar's music; his hand flies (speed 300) towards the Indian village | partial | `SetWidescreen`, `StartMusic` work |
| If the store was filled before the village was won: the camera flies a recorded track and Khazar says "Congratulations." then "You are expanding well. Lethys had better beware."; otherwise the camera is put on a fixed view | todo | `RunCameraPath` and the camera library are stubs; see [../../camera/camera_paths.md](../../camera/camera_paths.md) |
| Khazar: "Time is not kind, you need to gain influence in the direction of our common enemy." | todo | |
| If the Indian village (the one where The Sea and The Plague happen) is not yet the player's, the camera turns to it and Khazar: "I strongly encourage you to gain this town."; either way, Khazar: "Before I leave to defend my realm I must tell you one more thing." | todo | |
| The screen fades to black over 2 seconds, the camera is put high above the edge of the player's influence and fades back in | partial | the fades and setting the camera work (`SetFade`, `SetFadeIn`, `SetCameraPosition`); never reached |
| Khazar: "You will notice you cannot do anything, even cast Miracles outside of your influence." · "However, there is a way using the power of belief." as the camera drops to the edge | todo | |
| The "Influence" hand demo plays, pausing once: "You can reach outside the edge of your realm for short periods of time" · "But your influence is soon lost until your hand is returned to your realm." | todo | `Data/HandDemo/Influence.hnd`; virtual influence itself is in [../../worship/influence.md](../../worship/influence.md); the land switches it on for the player at its start (`SetVirtualInfluence` stub) |
| Fade to black, the camera is put back on the view before the demo, fade in, the music stops and the film ends; Khazar is let go | partial | fades work; the rest is stubbed |
| A bronze info sign on influence appears (the faded hand, virtual influence, what can be done outside influence) | todo | |
| If Khazar is already dead, only the info sign appears | todo | |
| When the lesson ends, the watcher starts the land's last scroll ([destroy_it.md](destroy_it.md)) unless Khazar's death has already started it | todo | |

## Failing and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't be failed: the gesture practice gives up after 5 minutes and the store has no time limit | todo | |
| If the player loses the home village before the 20 minutes are up, the quest waits until it is theirs again | todo | |
| If Khazar is dead when the scroll is clicked, his lessons are skipped and the quest at once marks itself done without logging anything; the scroll is never removed | todo | his hand is still sent to lay the scroll first; what it does once he is gone is undetermined |
| If the Norse village is destroyed, the store step ends and the lesson is marked done, but the influence lesson never comes, since it waits for that village to be won | todo | the land's last scroll is then started only by Khazar's death; no soft-lock, since the land ends on Lethys's towns |
| Winning the Norse village before the scroll is clicked does not skip the lesson; the store must still be filled | todo | |

## Advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is nagged by the good advisor; the log reminders are the good advisor's (fill the store), then the evil advisor's (impress the village) | todo | |
| Khazar's theme plays through each of his films | partial | `StartMusic`/`StopMusic` work; never reached |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part; it can help fill the store, since only the store's food is read | todo | follows from the script |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the 5 minutes run out, both noted counts are pushed on by one each second until neither matches; if the player had done just one of the two steps, a count can match again and "Well, it doesn't matter…" is said twice | todo | |
| The log entry is updated before the 2,800 check, so it can show just over complete for a moment | todo | |
| Khazar's wood miracle is given power for six casts but he is told to cast four | todo | the script's own comment says three |
| Khazar's computer player is paused at the start of the lesson and no Land 2 script un-pauses it; whether letting his hand go lifts the pause is undetermined | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two spare influence demo scripts exist in the hand demo file; one has its demo commented out, the other plays a second influence recording (`influence2.hnd`); nothing in the game runs them | n/a | never in the shipped game |
| An earlier place for the 0 log entry, after the gesture demo, is commented out | n/a | |
| A wood check on the store (2,800 wood as well as food) is commented out | n/a | |
| A note to clear the repeat-miracle icon and remove the leash "for confusion" is left as a to-do | n/a | |
