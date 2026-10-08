# The Magic Dragon

A silver scroll on the fifth land: five worn-out crusaders camped by a cave want to fight the dragon inside. The player
heals them, then lights their pyre so its smoke fills the cave, then follows the unseen fight by listening at the
cave's five blowholes. Three crusaders stagger out of the far exit with the dragon's treasure: a flying-flock miracle
dispenser. The land as a whole is in [../land_5.md](../land_5.md), dispensers in
[../rewards.md](../rewards.md) and [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md).

**Land:** 5 · **Giver:** the crusaders' leader, at their camp by the cave mouth near the Tibetan town · **Script:**
Crusaders · **Reward:** a flying-flock miracle dispenser (doves or bats) by the cave's exit, refilling every 5 minutes,
and an influence ring round it · **Repeatable:** no

Sources: the quest's script source (checked against the decompiled shipped `challenge.chl`, where it is compiled), the
land's control script, the shared notify, reminder and dispenser-reward scripts, the game's text table
(`InfoScript2.txt`). openblack's state is judged on the physics work tree (`ob-wt-physics`): openblack starts the
story's top script, which always begins with Land 1's control script, and the land-loading function does nothing, so
Land 5's control script never runs and this quest never starts. Of the commands it needs, only the object flags, the
camera cuts, starting and stopping music and the "is there fire near" test do anything in `src/CHLApi.cpp`; creating
villagers, stores, highlights, effects, timers and dispensers does nothing (`CreateScriptObject` only makes mobile
statics and rocks). Every row is todo unless the notes say otherwise.

**Progress: 0/67 done, 14 partial — 10%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 5's control script starts the quest in the background right after the land's opening (Nemesis's curse on the creature), together with the land's other first quests | todo | the land's control script never runs in openblack (see [../../scripts/land5_script.md](../../scripts/land5_script.md)) |
| Nothing has to be done first; the camp is there from the start of the land | todo | |
| The quest's title is "The Magic Dragon" (title text 113) | todo | `UpdateSnapshotPicture` and the other snapshot commands are stubs |
| No other script waits for it or reads its outcome | todo | |
| The crusaders come back in the game's ending: its first credits shot ("Black & White", "Designed and Created by Lionhead Studios Ltd.") shows three crusaders sitting and talking at this cave's exit, whether or not the quest was done | todo | the ending sequence in the land's control script; see [../ending.md](../ending.md) |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cave's entrance and exit and the five blowhole craters along the hill between them are made indestructible | partial | `SetIndestructable` works, but finding the land's features by position (`GET FEATURE`) is not implemented |
| A small cauldron (a fifth of normal size) is put in the camp | todo | `CreateWithAngleAndScale` does not make mobile objects of this kind |
| Five crusaders (the land's special crusader villagers) are made round the camp and put in one group that keeps within 10 to 25 of its centre | todo | creating villagers and groups (`FlockCreate`, `FlockAttach`) does nothing |
| Each crusader is indestructible, cannot be picked up, cannot be hurt by fire and cannot be set on fire | partial | the flag commands work (`SetIndestructable`, `SetIdPickupable`, `SetHurtByFire`, `SetSetOnFire`), but the crusaders are never made |
| All five sit down for good and face the camp; the group's life is set to 0.6 (worn out from "our last adventure") | todo | `SetProperty` (health) is a stub |
| A wood pile (a wood store holding 9000 wood) is the pyre, just outside the cave mouth; for now it is indestructible, cannot be picked up, cannot burn and cannot be set on fire | partial | the flags work; making a store does nothing |
| Five small helper scripts, one per crusader, wait for the charge into the cave | todo | |

## How it appears (the advisor nag)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver challenge scroll hangs 20 above the camp | todo | `CreateHighlight` is a stub |
| While it waits, whenever the camera is within 100 of it and it is on screen, the evil advisor steps out, points at it and says "Hey. There's a job for you. Wanna do it?", at most once every 30 seconds and only when no other scene is playing | todo | the shared notify script; `SpiritEject`, `SpiritPointGameThing`, `RunText` are stubs |
| The quest waits for this: it goes on only when the scroll, or the camp spot, is clicked; the scroll then takes its opened look | todo | `GameThingClicked`, `PositionClicked` are stubs |

## The introduction: "heal us"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader is drawn in high detail; the camera swings in to the camp (position over 3 seconds, focus over 2), then creeps closer over 20 seconds | partial | `StartMusic` works; `StartCameraControl`, `MoveCameraPosition`/`Focus` and `SetHighGraphicsDetail` are stubs |
| One crusader whittles a stick throughout; the leader gets up and looks exhausted; two others get up, one sinking back to sit twice, one once | todo | animation commands are stubs |
| The scroll is recorded with progress 0 and the reminder "These brave followers are in need of healing, Leader." (spoken by the good advisor when asked for) | todo | the shared reminder script |
| The leader faces the camera (man's voice): "Holiest One, we worship and adore you." | todo | |
| The leader: "In your name we'll fight the dragon in this cave, once we've recovered from our last adventure." The camera cuts to look into the cave mouth, a dragon's roar sounds from it, and the camera drifts over 6 seconds | partial | the cut works (`SetCameraPosition`/`Focus`); `PlaySoundEffect` and the camera moves are stubs |
| The leader turns; the camera cuts back to the camp: "If you'd be so good as to heal us, the dragon would stand no chance." while he looks exhausted twice | todo | |
| The music stops and the scene ends; the leader and the two others sit back down, a quarter of a second apart | partial | `StopMusic` works |

## Healing the crusaders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must bring all five crusaders to full life (each at 1 or more); the heal miracle is the way to do it | partial | the heal miracle heals people to full life ([../../miracles/heal.md](../../miracles/heal.md)), but the crusaders don't exist and the life reads are stubs |
| There is no time limit and nothing can go wrong: they cannot die, burn or be carried off | todo | |

## The second scene: "light the pyre"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader stands up in high detail and faces the camera, which swings round (position over 3 seconds, focus over 2) and then drifts over 30 seconds | partial | `StartMusic` works |
| The leader: "Thank you, Holiest. We're nearly ready to take on the pesky dragon." | todo | |
| The camera's focus turns to the cave mouth over 6 seconds; the leader walks to the pyre, faces the cave, and the camera cuts to look at the mouth, drifting over 8 seconds | partial | the cut works |
| The leader prods the pyre like a campfire; the scroll's progress becomes 0.33 with the reminder "Light the pyre and the caped crusaders can get on with it." | todo | |
| The leader: "Our plan is to fill the cave with smoke from this pyre." He turns back to the camp | todo | |
| From now on the pyre can burn: it loses its indestructibility and can be hurt by fire and set alight | partial | `SetIndestructable`, `SetHurtByFire`, `SetSetOnFire` work, but the pyre isn't made |
| The crusaders liven up (gesturing, gossiping, looking impressed); the leader: "The dragon will be unable to see and we'll storm in and dispatch him." and dances | todo | |
| The camera moves back (2 seconds); the leader walks towards it: "There's one problem. We have no means of lighting the pyre." shrugging twice, then "Can you help us with this?" | todo | |
| The leader walks back to his place, the music stops, the scene ends and all five sit down one after another | partial | `StopMusic` works |

## Lighting the pyre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits until there is fire within 10 of the cave's doorway; the pyre is about 3 from it, so setting it alight (a fireball, a burning object thrown on it) is enough, and any other fire that close counts too | partial | the test works (`IsFireNear` in `src/CHLApi.cpp`, `FireSystem::IsFireNear`), but the pyre is never made and the script never runs |
| No time limit; nothing reminds the player beyond the scroll's reminder | todo | |

## Smoking out the dragon (the charge)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts (no music): thick smoke (eight times normal size) pours from the pyre for 30 seconds; 3 seconds later the camera moves in above it (3 seconds), holds 4 seconds | todo | `SpecialEffectPosition` and `SetProperty` (scale) are stubs |
| The camera rises high above the cave mouth and pans along the hill from the first blowhole to the fifth over 28 seconds while smoke bursts out of each blowhole in turn, 3 seconds apart (each eight times size, for 30 seconds) | todo | |
| The pyre is emptied and removed | todo | `AddResource`, `ObjectDelete` are stubs |
| The camera cuts to look across the camp to the cave and pulls back slowly over 25 seconds; the scroll's progress becomes 0.66 with the reminder "Let's listen at the blowholes to follow their valiant progress." | partial | the cut works |
| The charge: the leader walks to the rally point at the mouth, looks into the cave and back, points and cries "Charge!" | todo | `GamePlaySaySoundEffect` is a stub |
| The other four get up (three of them stand from sitting first) and gather round the rally point; once all five are there the leader shouts "Run forward quickly!" and all run into the cave at 0.7 speed (one a second later), fading away as they reach the doorway | todo | |
| The scene ends once the last crusader has gone | todo | |

## Listening at the blowholes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 10 seconds the fight is followed blowhole by blowhole, strictly in order from the cave mouth's end: each one plays only when the camera is near it (within 50 for the first, within 30 for the others) and the blowhole is on screen | todo | `CallNear`/camera-near and on-screen tests are stubs |
| The voices are heard as spoken sound lines in crusaders' voices rather than as dialogue boxes | todo | undetermined whether their subtitles show; `GamePlaySaySoundEffect` is a stub |
| First blowhole: smoke (10 seconds); "Cough! Cough!", 2 seconds later "Who's' that?" (the stray apostrophe is in the game's text), 4 seconds later "Get off! That's me!", 2 seconds later "Can you see him?" | todo | |
| Second: smoke; "Cough! Cough!", then "What's that golden shining, oh Alan, why didn't you go before?", 4 seconds later the dragon roars, then "Go for his head, Barry!" | todo | |
| Third: smoke; "Tally-Ho!", 2 seconds later "At 'em, lads!", then the dragon roars | todo | |
| Fourth: smoke; the dragon-fight sound, then "Ralph's dead!" and "Albert's copped it. Flee!" and the camera shakes (within 50, amplitude 0.5, 2 seconds) | todo | `ShakeCamera` is a stub |
| The dragon is never seen: there is no dragon in the cave or anywhere, only its roars and the fight sound | todo | |
| There is no limit on how long the player takes; nothing reminds them which blowhole is next beyond the scroll's reminder | todo | |

## Out of the exit: the treasure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reaching the fifth blowhole starts a scene: the camera moves to look over it and the gate (3 seconds); smoke, a roar, the camera shakes (within 100, amplitude 0.25, 1 second); 4 seconds later "We've got the treasure, Scarper to the exit, lads!" | todo | |
| The camera moves to the cave's exit (4 seconds); smoke (half the pyre's size) bursts from it for 20 seconds | todo | |
| Three crusaders (two did not make it) come out of the exit in high detail, indestructible, unpickable and unhurt by fire (this time not protected from catching fire), at life 0.4, and run out with the running-while-on-fire animation at speeds 0.7, 0.6 and 0.55; one shouts "Run forward quickly!" | todo | creating villagers does nothing |
| They stop in front of the exit: the leader faces the camera and pants, one pants, one sits down | todo | |
| A sparkle marks the spot and a flying-flock miracle dispenser appears there, refilling every 5 minutes | todo | dispensers exist in openblack (`MagicSystem::CreateDispenser`), but the script command that places them is a stub; the flying flock miracle is in [../../miracles/flocks.md](../../miracles/flocks.md) |
| The player gains an influence ring of radius 50 round the spot, which is never taken away | todo | `InfluencePosition` is a stub |
| The leader: "Victory! We bring you the treasure we found inside the cave.", "Take this with our compliments.", "We will leave it here as a holy shrine to you. It might be useful, too." | todo | |
| The two standing crusaders sit down; the scroll's progress becomes 1 (complete) and the scroll is removed | todo | |
| The dispenser's own presentation runs too: the reward sting, the camera flies to a view 21.5 to its side and 14 up (4 seconds), and the evil advisor, pointing, says "Nice. Another of those cool Miracle Dispensers." (or, if it were the first dispenser of the game, explains dispensers and puts up a signpost), then the dispenser's own help lines | todo | the shared dispenser-reward script; it asks for its own scene while this one is still playing; undetermined exactly how the two scenes are ordered |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three survivors wait up to 2 minutes for the player to heal them to full life | todo | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining` are stubs |
| Healing them or not makes no difference: the script notes which happened but never uses it (no alignment change, no extra reward) | todo | quirk |
| They then walk back into the cave's exit one after another and fade away | todo | |
| What stays: the dispenser and the influence ring at the exit; the cave features stay indestructible; the cauldron stays at the old camp | todo | |

## Script quirks and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One of the four followers (the second) is never told to stand up before walking to the rally point, unlike the other three | todo | |
| Cut: each crusader was to idle by looking for something and looking exhausted in turn through each stage; the code is commented out (two followers keep an empty wait loop from it) | n/a | not in the compiled script |
| Cut: a base-camp object, a wait for the camera to be away before making the camp, a dragon murmur and a camera shake at the fourth blowhole in the leader's own spot are commented out | n/a | not in the compiled script |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The two camp scenes play the creature guide's theme and stop it at their end; the smoke, blowhole and treasure parts start no music | partial | `StartMusic`/`StopMusic` work, but the script never runs |
| Sounds: the dragon's roar (four times), the dragon-fight sound, the reward sting (from the dispenser) | todo | `PlaySoundEffect` is a stub |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part to play; it could cast heal or set the pyre alight on the player's behalf, which the script cannot tell apart from the player doing it | todo | the script checks only the crusaders' life and fire near the cave |
