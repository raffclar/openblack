# Khazar's Fireball Challenge

A land 2 gold scroll and Khazar's lesson in attack: three empty huts on the shore are the targets, three fireball
seeds are laid out a long throw away, and however well (or badly) the player throws them, Khazar ends by giving the
Fireball miracle to the player's village. This file also covers the "Miracle Challenge" scroll that Khazar leaves by
the worship site after the [Worship Site](worship_site.md) lesson, which starts this challenge and
[Khazar's Shield Challenge](khazars_shield_challenge.md) together. The land as a whole is in
[../land_2.md](../land_2.md), the land's control script in [../../scripts/land2_script.md](../../scripts/land2_script.md),
the script program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), the miracle itself in
[../../miracles/fireball.md](../../miracles/fireball.md).

**Land:** 2 · **Giver:** Khazar (the friendly god), through a scroll by the player's worship site and then a scroll on the target huts · **Script:** MiracleChallenges, FireBallChallenge · **Reward:** the Fireball miracle (first level) at the player's home village's worship site, given whatever the result · **Repeatable:** no

Sources: the quest's script source (`Land2FireballChallenge.txt`), the land's scroll-notify helper (`SetupLand2.txt`),
the hand demo (`HandDemos.txt`), the lesson that places the first scroll (`LearnWorshipping.txt`) and the land's
control script, checked against the compiled `challenge.chl`; the game's English text table; and the land's map
script (`Scripts/Land2.txt`) for the huts. openblack's state is judged on the physics work tree (`ob-wt-physics`): of
the 59 script functions the two scrolls (and the shield challenge they start) need, 42 still only log "not
implemented" in `src/CHLApi.cpp` (scrolls, dialogue, advisors, Khazar's hand, the moving camera, timers, the held
object, object properties, hand demos, the challenge log), and the land's control script never runs in openblack (the
land-loading function does nothing and the story always begins with Land 1's script), so the quest never appears;
every row below is todo unless the notes say otherwise.

**Progress: 0/72 done, 6 partial — 4%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the Worship Site lesson ends, Khazar's hand flies to a spot just south of the player's worship site (about 75 m from the altar) and the "Miracle Challenge" scroll is started there; the lesson does not wait for it | todo | see [worship_site.md](worship_site.md); `MoveComputerPlayerPosition` is a stub |
| Nothing else in the land waits for either miracle challenge to be finished: no flag is set on completion, and the land's later quests (The Workshop, Impress Village, the last scroll) are started by other conditions | todo | the land's control script only watches the Worship Site lesson's end flag |
| What it unlocks: only the Fireball miracle at the home village's worship site; the same miracle also comes with Town10 (an Indian town that starts with a fire miracle) when it is won | todo | `Land2.txt` gives town 10 a fire miracle |
| An unfinished challenge holds Khazar "busy" for as long as it runs; the land's control script will not start Khazar's death while he is busy, so leaving this challenge half done holds back the end of the land (see Script quirks) | todo | |

## The Miracle Challenge scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears 6 m up in the air at the spot Khazar's hand went to, just south of the worship site | todo | `CreateHighlight` and the height property are stubs |
| A two-minute timer starts as soon as the scroll is placed; it decides Khazar's first line later | todo | `CreateTimer` is a stub |
| While the camera is within 100 of the scroll and the scroll is on screen, the evil advisor steps out, points at it and says "Khazar's left us a Miracle Challenge, to expand our knowledge!", at most once every 30 seconds and only when no film is playing | todo | the land's own scroll-notify loop; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| The loop ends when the scroll or the spot under it is clicked, or when Khazar has died; either way the scroll is then switched to its active look | todo | `GameThingClicked` and `SetActive` are stubs |
| If Khazar has already died when the loop ends, nothing more happens: neither challenge is started and the two miracles are never taught this way | todo | |
| The scroll is never deleted by the script; it goes when the scroll's script ends (scripts' own objects are cleared with them; inferred, as for the other scrolls) | todo | |

## Khazar's tour (introduction)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking it starts both challenges at once, each with its own scroll (this one on the target huts, the other on the island hut), and then plays a film | todo | |
| Khazar's music starts | partial | `StartMusic` works in openblack (`src/CHLApi.cpp`); never reached |
| Khazar's hand flies fast back to the worship site; the camera rises above it over 5 seconds looking west, then pans slowly east over 12 seconds | todo | `MoveCameraPosition`/`MoveCameraFocus` are stubs |
| Khazar speaks: if the scroll was clicked within two minutes of being placed, "You are keen to learn the secret of casting Miracles. I appreciate your enthusiasm."; otherwise "You are ready to learn the secret of using Miracles." | todo | `GetTimerTimeRemaining` is a stub |
| Khazar: "Come with me. I have a set up an area where you can learn more." (the text table's own wording); two seconds later the camera lifts over the temple and his hand drifts off east | todo | |
| Scroll drawing is switched on, so the two new challenge scrolls show (inferred from where it is switched) | todo | `SetDrawHighlight` is a stub |
| His hand flies to the island hut of the shield challenge; the camera moves in over 7 seconds and he says "Here you can learn about defence." | todo | |
| His hand flies on to the middle target hut of this challenge; the camera follows over 7 seconds and he says "And here you can learn to attack." | todo | |
| The camera pulls back over 4 seconds to look over the huts from the west and Khazar says "Activate the Scroll of your choice." | todo | |
| Khazar's hand goes home to his own temple, the music stops, the film ends and Khazar is released | todo | `ReleaseComputerPlayer` is a stub |

## How the fireball scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The targets are the three houses of a small empty Norse village on the south-east shore, a neutral village the map makes uninhabitable for the challenge | partial | the houses are made by openblack's map loading (`AbodeArchetype`); the uninhabitable flag is not (see [../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| As soon as the challenge is started (at the tour, before its scroll is clicked) an anti-influence ring of radius 50 is laid round the first hut, so the player's hand can't reach in and must throw | todo | `InfluencePosition` is a stub; openblack has anti-influence areas only for its own miracles |
| A gold scroll appears on the middle hut | todo | `CreateHighlight` is a stub |
| Notify: while the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points and says "We gotta have a go at this Fireballing. Boss. We gotta!", at most once every 30 seconds and only outside films | todo | same notify loop as above |
| The quest waits for the scroll or the middle hut to be clicked; if Khazar has died first the scroll is made active and nothing more happens | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click, a ring of influence of radius 40 is made where the seeds will lie, so the player can pick them up | todo | `InfluencePosition` is a stub |
| All three huts are mended to full health | todo | setting a building's health is a stub (`SetProperty`) |
| The player's control is cut down to the hand only for the whole challenge | partial | `SetInterfaceInteraction` works in openblack (`src/CHLApi.cpp`); never reached |
| A film starts; Khazar's music plays and Khazar counts as busy | partial | `StartMusic` works; never reached |
| The camera swings over 4 seconds to look along the shore at the huts while Khazar's hand flies fast to them | todo | |
| Khazar: "Behold three huts. We shall use these as our targets." | todo | `RunText` is a stub |
| The log entry "Khazar's Fireball Challenge" is recorded at 0 with the reminder "Try and burn one of these huts with the Fireball Miracle seeds." | todo | `Snapshot` is a stub |
| Khazar's hand flies slowly back inland and three Fireball one-shot seeds appear on a hillside about 270 m from the huts, 2 m apart in a diagonal line | todo | creating one-shot seeds is not done by `CreateScriptObject` (it makes scenery and rocks only) |
| The camera moves over 4 seconds to just behind the seeds, looking out to the huts | todo | |
| Khazar: "Here are some fireballs to use." while his hand drifts down to hover by the seeds | todo | |
| Khazar's fireball hand demo plays (a fire seed is put in the hand): "You hold the Action Button down. Like this.", pausing for the hand; "You then move the Hand to put momentum on the fireball.", pausing again; "You release the Action Button to cast the fireball."; then "Devastating! Fire spreads and causes serious damage." | todo | `PlayHandDemo`, `HandDemoTrigger` are stubs; openblack has no hand demos |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| First the quest waits for the player to hold a fire miracle: until then, every 30 seconds (the first at once) the good advisor says "Tap the Fireball Miracle." | todo | `GetObjectHeld`, `IsOfType`, timers are stubs |
| The check looks for a fire spell seed in the hand, not the one-shot globe itself (undetermined: whether picking up the globe alone counts, or only once tapped) | todo | |
| The first wait also ends if all three seeds are gone without one being seen in the hand (for instance thrown at once); then the advisor's throwing tip is skipped | todo | |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 15 m from the first seed's spot fades out and is made again at its own spot | todo | the other two seeds are measured from the first seed's spot too |
| Once a fire miracle is held, the evil advisor steps out: "Now throw it at a hut like you'd throw a rock." | todo | |
| Any hut that drops below full health counts as hit, once; hits add up over the whole challenge | todo | building health reads are stubs |
| A round ends at once if all three huts are hit; otherwise when all the seeds are used up: the quest waits 1 second, then until the hand is empty, then 6 seconds more, and counts the huts one last time | todo | |
| The throw itself (fireball flight, bouncing, setting the huts alight and fire spreading) is the miracle's normal behaviour | partial | the fireball miracle works in openblack ([../../miracles/fireball.md](../../miracles/fireball.md), fire in ../../physics/fire.md); the challenge never runs |

## Results and retries

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After each round a short film shows the huts from the shore, the camera drifting closer over 10 seconds (8 after a miss), then snaps back to where the player was | todo | |
| All three huts hit: the log entry is set to 1 and the evil advisor says "Nice aiming, Boss. You damaged all three huts." and "Fire spreads, you see. A Village can be torched in minutes." | todo | |
| Two huts hit: "Pretty nifty. Two of the three huts are damaged." then the same fire-spreads line; log 1 | todo | |
| One hut hit: "Oh yeah! You hit one of the huts." then the fire-spreads line; log 1 | todo | |
| Any hit ends the challenge; there is no reward difference between one hut and three | todo | |
| No hut hit: the log entry's success goes up by a third, and the evil advisor says "Oh man. None of the huts got toasted." after the first miss, "You still haven't managed to hit anything." after the second and third | todo | |
| After a miss three new seeds appear at the same spots; Khazar says "Have some more practice. Here are more Fireballs." the first time and "Try again." the second | todo | |
| The player gets three rounds at most; after the third miss the challenge ends as if passed | todo | |
| There is no failure: the challenge always ends with the reward | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The music stops, the log entry is set to 1 and the player's full control comes back | todo | |
| In a dialogue (not a film) the Fireball miracle (first level) is given to the player's home village, so its worship site can pray for it | todo | the command that adds a miracle to a town is a stub; the worship site's miracles: see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Khazar: "My Fireball Challenge is complete.", "I have given the knowledge of the Fireball Miracle to your Village.", "From now on your people can worship for this Miracle at your Worship Site." | todo | |
| The evil advisor: "Wow. We can create these now. Bet they'll impress non-believers!" | todo | |
| Khazar is released and no longer busy; any seeds left fade away | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The huts stay as damaged as the player left them (burning huts burn on); nothing rebuilds them | todo | |
| The fireball miracle stays at the home village's worship site for the rest of the land | todo | |
| The influence ring by the seeds and the anti-influence ring round the huts are never removed by the script (undetermined: whether they go when the script ends) | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor owns the notifications and the results; the good advisor owns the log reminder and the tap tip | todo | `SpiritEject`, `RunText` stubs |
| Clicking the log entry speaks its reminder "Try and burn one of these huts with the Fireball Miracle seeds." through the good advisor, who steps out to say it | todo | the shared reminder script |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays through the tour, and again from the challenge's click until the reward | partial | `StartMusic`/`StopMusic` work in openblack; never reached |
| The tour stops the music at its end; the challenge stops it only after the last round | todo | |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts stops it from fetching or throwing the seeds (undetermined what happens if it does) | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar counts as busy from the click until the reward; Khazar's death only starts while he is not busy, so a challenge clicked and then ignored (seeds never used) holds back Khazar's death, and with it Lethys's vortex and the end of the land, until it is finished | todo | the land's control script checks the busy flag every 9 seconds |
| The busy flag is shared: the shield challenge (or any other Khazar lesson) finishing clears it even while this one is still running, so Khazar's death can then start in the middle of this challenge | todo | |
| The first hut is looked up at the village's position, about 5 m from that house's own position (undetermined: whether the lookup finds it; if not, that hut can't count as hit) | todo | the house is at a slightly different spot in `Land2.txt` |
| A round with seeds left never ends while the player holds something: the wait for an empty hand has no time limit | todo | |
| The three hit results all set the log to 1 before the last line; a miss raises it by a third, so after three misses it reaches 0.99 before the final 1 | todo | |
| The two "no hut hit" lines after the second and third misses are the same line | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Unused lines in the text table: Khazar's "Here are some Fireball Miracle seeds." and "Using these fireballs.", the evil advisor's "Oh Boss. You didn't hit anything at all.", and two entries marked "NOT USED" | n/a | in the text table but never spoken by the shipped script |
