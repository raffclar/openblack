# The Singing Stones (land 2)

A silver scroll on Khazar's land: a priest shows the player a horseshoe of nine singing stones that "hold the spirits of
the ancients". Tapping the stones in the shape of three hidden melodies (Twinkle Twinkle Little Star, Chopin's funeral
march and White Christmas) brings on night with bats and mist, raises the dead inside the ring, or makes it snow. Two
whistling wanderers about the land give away the first two tunes. There is no reward object; each first-time tune adds a
third to the challenge's success.

A cut stone-circle quest, never compiled, is [The Miracle Stones](./the_miracle_stones.md).

**Land:** 2 · **Giver:** a priest from the hut above the stones (the scroll stands on the hut) · **Script:**
SingingStonesSongs · **Reward:** none given; the tunes' effects (night with bats, the dead raised, a snowfall) are the
prize · **Repeatable:** yes (every tune can be played again and again; the challenge never closes)

Land 1 has a different quest with the same title (the hippy's stone circle, `SingingStoneCircle`, documented by its own
file); it is not described here. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the stones as a puzzle in
[../minigames.md](../minigames.md#singing-stones-lands-1-and-2), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

Sources: the quest's original script source (`SingingStonesSongs.txt`, 15 scripts, checked against the PC game's compiled
`challenge.chl` for argument orders and object types), the land's control script (`LandControl2.txt`), the shared notify
script (`ChallengeNotify.txt`) and the game's English text table. openblack is judged on the physics work tree
(`ob-wt-physics`): the quest and the scripts it runs use 77 script functions, of which 21 do something in
`src/CHLApi.cpp` (creating objects only for mobile statics and rocks, the move and pick-up flags, indestructible and fire
flags, game time, music start and stop, widescreen, positions and distances) and 56 only log "not implemented" (among them
highlights, dialogue, advisors, the camera moves, clicks, sounds, snapshots, flocks, mist, weather, timers and setting
properties). The land's control script stops at its first unwritten function long before it starts this quest (see
[../land_2.md](../land_2.md)), so the quest never appears; every row is todo unless the notes say otherwise.

**Progress: 0/85 done, 2 partial — 1%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest at once, in the background, right after the land's set-up and the first did-you-know signposts and before the land's entry scene; no town has to be won and there is no wait | todo | the land's control script never gets this far in openblack (`run background script` would work, but earlier natives are stubs) |
| The nine stones, their bases, both whistlers and the stone-tap listener are made before the scroll appears, so the stones already sing when tapped before the quest is taken (those early taps are wiped when the quest starts) | todo | |
| A silver challenge scroll is put on the hut above the stones (a house at about 2135, 3011; no height is given, so the game's default for a challenge scroll) | todo | `CreateHighlight` is a stub |
| While the scroll waits, whenever the camera is within 100 of it and it is in view, the good advisor steps out, points at it and says "Look. Something for you to do here.", at most once every 30 seconds and only when widescreen is ready | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll or the hut itself starts the quest; the scroll is then switched to active | todo | `GameThingClicked` works, `SetActive` is a stub; the script never removes the scroll (undetermined whether it stays on screen afterwards) |
| A priest is then made at the hut to give the introduction | todo | villagers are not made by `CreateScriptObject` |

## The stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine singing stones, numbered 1 to 9, stand in a horseshoe of about 10 radius around the circle's centre (about 2105, 3000), open towards the west; each sits on its own singing-stone base | todo | the script creates them as mobile statics (singing stone and its base), which openblack's script object creation does make (`Create`, `CreateWithAngleAndScale`, `CreateScriptObject` in `src/CHLApi.cpp`); the script is never reached |
| The stones are made at three-quarter size, each turned to face the centre (turned by -10, -27, -44, -70, -100, -130, -140, -160 and -180 degrees from stone 1 to stone 9) | todo | as above |
| The stones cannot be picked up or moved by the hand; the bases are not locked by the script | todo | `SetIdPickupable`, `SetIdMoveable` work but have no stone to act on; openblack's physics already treats the singing stone as heavy and its base as unmovable (`src/ECS/PhysicsClasses.cpp`), which helps only if they exist |
| Tapping a stone (clicking it) plays that stone's own note at the stone, flashes a short success sparkle on it (a tenth of a second) and adds the stone's number to the tune being remembered | todo | `GetObjectClicked`, `PlaySoundEffect`, `SpecialEffectObject`, `ClearClickedObject` are stubs |
| By day the stones play one set of nine notes (the "twinkle" samples); at night they play a second, darker set (the "funeral" samples) | todo | the stone-tap samples are script sound effects; nothing in openblack plays them |
| Only the player's own clicks count; the script never looks at the creature or anything else touching the stones | todo | |
| The stones remember the last 14 taps in a ring: each tap goes in the next of 14 slots, the 15th tap overwriting the first | todo | rules summary in [../minigames.md](../minigames.md#singing-stones-lands-1-and-2) |
| Each tune is listened for all the time by its own checker, which looks for the tune starting at every one of the 14 slots in turn | todo | |
| After any tune is recognised, and whenever the stones change between day and night, all 14 slots and the tap count are wiped | todo | |

## Day and night

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the hour of the day is after 22:00 or before 5:20, the nine stones fade away and nine tombstones (three-quarter size, same places and turns) take their place; they too cannot be picked up or moved | todo | `GetGameTime` works; the swap needs `ObjectDelete` (stub) and creating a feature (not made by `CreateScriptObject`) |
| Outside those hours the tombstones fade away and the singing stones come back, unless the dead are still being raised, in which case the tombstones stay until the five minutes are over | todo | |
| The funeral march can only be recognised at night, while the tombstones stand; the other two tunes are recognised by day or night, on stones or tombstones (at night they sound in the darker notes) | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene (widescreen, camera under script control) starts with the fourth generic script theme | todo | `StartCameraControl` and friends are stubs; `StartMusic` works and the theme's bank is listed in `src/Audio/GameMusic.cpp`, but the scene never runs |
| The priest is shown in high detail; the camera closes on the hut as he walks slowly out (speed 0.2) | todo | `SetHighGraphicsDetail`, `SetProperty` are stubs |
| After 2.5 seconds he goes on down the path to the stones on his own (speed 0.3, then 0.5 for the last stretch) while the camera follows him in three moves and then settles beside the stones | todo | `MoveGameThing` is a stub |
| Facing the camera, gossiping with his hands: "These stones hold the spirits of the ancients. Each has its own voice." | todo | `RunText`, `SetFocus` stubs; he plays a gossip gesture once, then a second one looping |
| "It is said that when a correct tune is played, special powers are unleashed." | todo | |
| "When the stones are tapped the spirit sings." as the camera drifts closer over seven seconds | todo | |
| "Few people know the melodies. You may meet them, or discover the melodies for yourself." | todo | the hint that the whistlers carry two of the tunes |
| The dialogue closes, the camera pulls back over the stones, the priest goes back to normal detail | todo | |
| The challenge is entered in the player's challenge log (the snapshot records a picture, the title "The Singing Stones" and the reminder script) with success 0 and alignment 0; the music stops | todo | `Snapshot` is a stub |

## The priest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the priest walks slowly (speed 0.2) and wanders around the circle's centre within 6 of it, with two further wander settings of 4 and 30 (undetermined: their exact meaning in the wander state) | todo | `SetScriptState`, `SetScriptStatePos`, `SetScriptFloat`, `SetScriptUlong` are stubs |
| He is not protected: he can be picked up, thrown or killed | todo | the script sets no flags on him |
| The first time he is found dead, the evil advisor steps out: "Ah… Now that's what I call music." (once only) | todo | `GetProperty` (health), `SpiritEject` stubs |

## The whistlers (hints)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A whistling wanderer (a pied-piper villager) is made north of the stones (about 2011, 3201) with the whistled Twinkle Twinkle Little Star fixed to him, so it is heard near him | todo | `AttachMusic` stub; the whistled tune's bank is listed in `src/Audio/GameMusic.cpp` |
| He walks a loop of 31 points through the north of the land and back, waiting at each until within 5 of it or 60 seconds pass; at the fifth point he sits down for two minutes | todo | |
| A second whistler is made far to the south (about 2387, 1840) with the whistled funeral march fixed to him; he walks a loop of 24 points, sitting for two minutes at a rest spot on a rise | todo | |
| Both whistlers cannot be killed, cannot be hurt by fire and cannot be set alight | partial | `SetIndestructable`, `SetHurtByFire`, `SetSetOnFire` work in `src/CHLApi.cpp`, but the whistlers are never made |
| Both are made as soon as the land starts the quest, before its scroll is clicked, and walk forever | todo | |
| No one whistles White Christmas: that tune must be found by the player | todo | |

## The melodies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Twinkle Twinkle Little Star is 14 taps on fixed stones: 1, 1, 8, 8, 9, 9, 8, 6, 6, 5, 5, 3, 3, 1 | todo | it fills the whole 14-tap memory, so it has to be played with no stray tap inside it |
| The funeral march is 11 taps judged by the steps between stones, not the stones themselves, so it can start on any stone: the same stone four times, three stones up, one down, the same, two down, the same, one down, one up (for example 4, 4, 4, 4, 7, 6, 6, 4, 4, 3, 4) | todo | only listened for at night |
| White Christmas is 8 taps judged by steps, starting anywhere: one up, one down, one down, one up, one up, one up, one up (for example 3, 4, 3, 2, 3, 4, 5, 6) | todo | |
| The step shapes match the real tunes if neighbouring stones are a semitone apart (the funeral march's B-flat opening and White Christmas's chromatic climb); with that reading Twinkle's stones give C, D, E, F, G but stone 9 would be a semitone below the tune's A (undetermined: the stones' real pitches would need the sound samples) | todo | |
| The tunes can be played in any order; Twinkle's night is the intended way to reach the funeral march, which needs night | todo | |
| If more than one tune is waiting, the main loop deals with Twinkle first, then the funeral march, then White Christmas | todo | |

## Twinkle Twinkle Little Star: night falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene starts with the Twinkle theme | todo | `StartMusic` works and the bank is listed; the scene never runs |
| The first time only, a flock of 10 bats is made at the circle's centre (inner radius 15, outer 30); they stay for good | todo | `FlockCreate`, `PopulateContainer`, `ChangeInnerOuterProperties` stubs |
| Every time, a white mist is made at the centre (size 0.8, faint, low: height ratio 0.6) | todo | `CreateMist` stub; this is the only mist the shipped scripts make |
| The time of day moves to 23:00 over 10 seconds and the clock is started again | partial | `MoveGameTime` and `GameTimeOnOff` work (the sky goes to night), but only if the scene is reached |
| The camera starts low by the stones, rises straight up over 12 seconds, then swings out wide | todo | `MoveCameraPosition`, `MoveCameraFocus` stubs |
| At the end the mist grows from size 1 to 2 and fades from 50 to 0 over 10 seconds, and the music stops; the mist is never deleted (undetermined whether 0 there means fully gone) | todo | `SetMistFade` stub |
| Because it is now night, the stones turn into tombstones straight after | todo | |

## The funeral march: the dead rise

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene starts with the funeral theme; the camera moves in low beside the stones | todo | |
| If the priest is alive he walks into the ring, stops, and faces the camera; the scene waits until he gets there with no time limit | todo | quirk: if he cannot reach the spot the scene would never end (no time-out, unlike the whistlers' 60 seconds) |
| After 10 seconds the raising begins, and a second later the priest says: "Behold, the dead will rise again when placed within the ring." | todo | the source's comment gives an older line, "The Dead shall live." |
| The camera pulls back over the stones and the music stops | todo | |
| A healing glow (the singing stones' heal effect, 13.5 times its size) covers the ring for five minutes | todo | `SpecialEffectPosition`, `CreateTimer` stubs |
| For those five minutes, any dead body within 10 of the centre is found (about every 0.3 seconds, one at a time), puffs steam, plays a spell sound and is brought back to full health | todo | `GetDeadLiving` stub; the sound is number 18 of the spell sound bank (undetermined which sound that is) |
| A dead villager or child is made again in its place as the same kind with the same age, but as a skeleton, and joins the town with id 11 (the Indian town by Lethys's snow edge) if it still exists, whichever town it came from | todo | `SetSkeleton`, `GetTownWithId`, `FlockAttach` stubs; the script calls it the "nearest town", but it is always the same town |
| A dead animal is made again in its place as the same kind | todo | |
| Each revived thing is held by the script for 3 seconds, given full health again and let go to live its own life | todo | `ReleaseFromScript` stub; at most about one revival every 3.3 seconds |
| Playing the march again while the raising is running replays the scene and the priest's line but starts no second raising | todo | |
| The tombstones stay, even past dawn, until the five minutes end; then the glow goes | todo | |

## White Christmas: snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A snowstorm is made at the centre: 300 seconds long fading over 5, snow only (no rain, no lightning, temperature 0, fall speed 1), 10 clouds, shade 1, height 70, inner radius 200, outer 500, not blown by the wind | todo | `ChangeWeatherProperties`, `ChangeCloudProperties`, `ChangeLightningProperties`, `ChangeTimeFadeProperties`, `ChangeInnerOuterProperties`, `SetAffectedByWind` stubs; creating a weather thing isn't made by `CreateScriptObject` |
| A cinema scene with the Christmas theme sweeps the camera round the stones in four moves (about 30 seconds) | todo | |
| The scene never stops its music (the other two scenes do) | todo | undetermined whether ending the cinema stops it |

## Success and the challenge log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first time each tune is played, the challenge log entry is updated: success 0.33 for the first tune found, 0.66 for the second, 0.99 for the third, in whatever order they were played | todo | `UpdateSnapshot` stub |
| Once all three have been played, success becomes 1.0 at once | todo | quirk: the update to 1.0 is then repeated on every pass of the main loop for the rest of the game |
| Alignment is 0 at every step (the snapshot and every update): nothing in the quest is good or evil | todo | |
| Every entry keeps the title "The Singing Stones" and the reminder script | todo | land 1's stone circle uses the same title text, so both lands' entries read "The Singing Stones" |
| Playing a tune again replays its effects every time but adds no more success | todo | |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no way to fail and no time limit; the quest's main loop never ends | todo | |
| Killing the priest does not end it: only the evil advisor's remark changes, and the funeral scene skips his walk and his line | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No reward object is given at any point; the quest's effects (night with bats and mist, the dead raised, the snowstorm) are its only payoff | todo | see [../rewards.md](../rewards.md) for the rewards other scrolls give |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The bats from the first Twinkle stay in the land | todo | |
| Villagers raised by the march stay skeletons, living in town 11 | todo | |
| The stones keep switching to tombstones every night and back every morning for the rest of the land | todo | |
| The whistlers keep walking their loops forever | todo | |

## Advisors' comments

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Asking for a reminder from the challenge log: the good advisor steps out: "Create melodies on these stones to awaken the spirits of the ancients." then the evil advisor: "Few people know the melodies. But who knows? We may meet them, Boss." | todo | the quest's own reminder script; `SpiritEject`, `RunText` stubs |
| The good advisor's scroll notice (see How it appears) and the evil advisor's remark on the priest's death are the only other advisor lines | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five script themes: the fourth generic theme (introduction), Twinkle, the funeral march, Christmas, and the two whistled versions fixed to the whistlers | todo | all six banks are listed in `src/Audio/GameMusic.cpp`; `StartMusic`/`StopMusic` work, `AttachMusic` is a stub |
| Eighteen stone notes: nine day ("twinkle") and nine night ("funeral") samples, one per stone, played at the stone tapped | todo | `PlaySoundEffect` stub |
| A spell sound at each revival | todo | |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part: no line, no action, and it cannot tap the stones for the player (only the player's clicks are read) | todo | undetermined whether the creature can learn anything from watching |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Because the checkers read the 14 slots as a ring, a tune may run across the point where the newest tap sits next to the oldest: e.g. White Christmas's last notes, then six other taps, then its first notes, is accepted; and any rotation of Twinkle (such as starting from its third note and ending with its first two) is accepted too | todo | |
| An empty slot counts as a stone numbered 0, so after a wipe a 13-tap run whose 11th to 13th taps and 1st to 4th taps happen to fit White Christmas around the still-empty 14th slot is accepted | todo | |
| The checkers loop without pausing, testing all 14 starting slots continuously | todo | |
| The reminder's evil line repeats the priest's own "Few people know the melodies" almost word for word | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All six of the quest's own lines are used; no unused lines of this quest were found in the text table | n/a | checked the text table for the quest's line prefix and for tune names |
| A small starter script that just runs this quest exists in the sources but is not in the land's challenge list (a test launcher) | n/a | not shipped as a quest |
| A cut stone-circle quest on this land, "The Miracle Stones" (eight stones, three hidden around the land, healing a boy), sits in the sources unlisted; its script is named "more singing stones", the very prefix this quest's own lines use, suggesting this quest replaced it | n/a | belongs to the shared-and-unused quests' file |
