# The Explorers

A silver scroll of the first land: three sailors (the script and the text call them missionaries) camp on a beach
beside an unfinished ark and want to sail away. Each time they need something they sing a verse about it, to an
accordion: wood to finish the boat, then grain, then meat. Once they have everything they sail off and leave a Water
miracle dispenser behind. Killing them, or leaving them waiting for three and a half hours, ends the challenge with an
evil mark. If they do sail, they come back on the fifth land ("The Explorers Again", see [../land_5.md](../land_5.md)).

**Land:** 1 · **Giver:** three sailors at a campfire beside an ark in dry dock, on a beach the good advisor places
"behind those huge Gates" · **Script:** TheMissionaries · **Reward:** a Water miracle dispenser on the beach ·
**Repeatable:** no

Sources: the challenge's script source (`TheMissionaries.txt`, checked against the PC game's compiled `challenge.chl`),
the shared helper scripts it runs (the scroll notice, the dispenser reward), the game's text table and the executable.
openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 81 script functions the challenge and
the helper scripts it runs need, 58 still only log "not implemented" in `src/CHLApi.cpp`. The working ones are camera
set and read, distance and position, the thing-valid check, making mobile statics, the fire, moveable and pick-up
flags, script music, the interaction level, stopping scripts, game time, random numbers and widescreen. On top of
that, the land's control script stops long before it would start this challenge (see
[../../scripts/land1_script.md](../../scripts/land1_script.md)). Every row is todo unless the notes say otherwise.
The script program as a whole is in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/89 done, 7 partial — 4%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the challenge in the background once the creature has been chosen (or straight away when a new game skips to the creature choice), alongside the creature breeder | todo | see [../land_1.md](../land_1.md); the land's control script never gets this far in openblack |
| The challenge script also waits until the creature choice is finished before putting up the camp and the scroll, which matters only when the opening is skipped | todo | |
| The ark is a dry-dock building made at the start, already a fifth built, and it can't be destroyed, hurt by fire or set alight | todo | making a feature from a script does nothing (`CreateScriptObject` only makes mobile statics and rocks); the three flags work (`SetIndestructable`, `SetHurtByFire`, `SetSetOnFire`) but have nothing to act on |
| A ring of the player's influence, radius 150, is put round the ark, so the hand can reach the beach; it is never taken away, not even when the ark sails | todo | `InfluencePosition` is a stub |
| A campfire (a bonfire object) is placed between the sailors' three seats, can't be moved or picked up and is set burning | partial | making the mobile static, the moveable and pick-up flags and setting it alight all work (`Create`, `SetIdMoveable`, `SetIdPickupable`, `SetOnFire` in `src/CHLApi.cpp`), but the script never reaches it |
| The guide's first lesson suggests this scroll first when the player hasn't opened it (or was already sent here and hasn't finished it): "You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it.", flying the camera to the beach | todo | see [../creature_guide.md](../creature_guide.md); the check counts the challenge as started once its scroll is clicked, and as finished when the main script ends (sailed or all sailors killed) |
| When the challenge ends by the time running out, the main script is stopped before it can mark itself finished, so the guide would keep suggesting it | todo | quirk of the scripts: the timeout stops every script in the file but the sailor check |
| If the ark sails, a flag is set that the fifth land reads to bring the sailors back ("The Explorers Again"); killing them or letting the time run out loses that challenge | todo | that challenge belongs to the fifth land: [../land_5.md](../land_5.md) |

## The scroll and the advisors' hints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears 16 above the ark | todo | `CreateHighlight` is a stub |
| While it isn't clicked, every 30 seconds that the camera is within 100 of the scroll with the scroll on screen and the widescreen free, the evil advisor comes out, points at it in the world and says "A Silver Reward Scroll. Let's see what it's all about." | todo | the shared scroll-notice script; `SpiritEject`, `SpiritPointPos`, `RunText`, `GameThingFieldOfView` are stubs |
| Clicking the scroll or the ark itself starts the challenge, and the scroll is switched to its active look | todo | `GameThingClicked` and `SetActive` are stubs |
| A second hint runs alongside until the scroll is clicked: when the camera comes within 100 of a point near the gates, the good advisor comes out, points at the scroll and says "There's a Silver Reward Scroll on the beach behind those huge Gates."; after that it waits 250 seconds before it can say it again | todo | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining` are stubs |

## The sailors arrive and sing the first verse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click, three sailors come out of the ark's door: two plain sailors and an accordion player, who carries the challenge's background accordion tune with him | todo | creating villagers is not done (`CreateScriptObject`); attaching music to a thing is a stub (`AttachMusic`) |
| A three-and-a-half-hour time limit starts from the click | todo | `CreateTimer` stub |
| The first verse starts at once, as long as no sailor is in the hand or the creature's hand (the verse scene always checks this and is skipped otherwise) | todo | `GetObjectHeld`, `InCreatureHand` are stubs |
| The idle and building animations are stopped; the sailors walk at a slow 0.4 to their three seats round the fire (a sailor more than 15 away who isn't flying is put straight onto his seat; a flying one is waited for until he lands) and they are drawn in high detail | todo | `MoveGameThing`, `SetHighGraphicsDetail` stubs; `SetPosition` works |
| The camera glides high over the beach, the first verse's music starts, then the camera drops low to the fire | partial | the camera moves are stubs (`MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived`); the verse's music (`StartMusic` with the first verse bank, `src/Audio/GameMusic.cpp`) works |
| The challenge is recorded with its title "The Explorers", no success and no alignment yet, and its reminder is this first verse scene, so clicking the scroll again replays the song | todo | `Snapshot` stub |
| The accordion tune is moved to the third seat's sailor, who plays the accordion for the rest of the challenge; the second sits down and sways to the song; the first beckons, looks lost, then gossips | todo | `DetachMusic`, `AttachMusic`, the animation commands are stubs |
| The camera cuts between close shots of the singers every 3 seconds, then the second sailor stands and despairs, and the camera sweeps along the ark and back to the fire | todo | `SetCameraPosition`/`SetCameraFocus` work, the moves don't |
| The scene ends only when the song's words have finished; then the first two sailors go back to idling | todo | |
| The idle loop, at random: walk to the fire and prod it, whittle a stick, shrug, look puzzled or despair, then sit down, sway to a sailing song for 4 to 8 loops and stand up; it ends when the sailor dies | todo | `Played` and the animation commands are stubs |
| After the first verse, the evil advisor: "How dare they leave? After all we've done for them?"; the good advisor: "What have we done for them, exactly? Perhaps we should help them out. It might be nice." | todo | `StartDialogue`, `RunText`, `TextRead` stubs |

## The three verses (words on screen)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each verse's twelve lines are shown one at a time, each when the music reaches that line, or after 10 seconds if the music gives no cue; the last waits until read | todo | the music-line wait is a stub (`LastMusicLine`); see ../../audio/voices_and_speech.md |
| Verse one (wood): "Ooooh, we've got this notion" / "That we'd quite like to sail the ocean" / "So we're buildin' a big boat to leave here for good." / "We're not keen on sinkin'" / "So we're all sittin' here a thinkin'" / "Cos we built it too big and we've run out of wood." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave til we get some more wood." / "Oooh, we're not keen on sinkin'" / "so that's why we're sittin' thinkin'" / "cos we simply can't leave til we get some more wood." | todo | sung in the sailors' (man's) voice |
| Verse two (grain): "Ooooh, the boat is now finished" / "But there's still somethin' on the wish-list" / "To keep us all goin' through the wind and the rain." / "There's no food on the table" / "And we can't sail unless we're able" / "So we ain't goin' nowhere 'til we get some grain." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave until we get some grain." / "Therrrre's no food on the table" / "And we can't sail unless we're able" / "So we ain't goin' nowhere 'til we get some grain." | todo | |
| Verse three (meat): "Ooooh, We're not complainin'" / "but there's still one more thing remaining" / "Cos bread is quite borin' if that's all you eat." / "We need some flavour" / "So do us all a little favour" / "Cos, we ain't goin' nowhere 'til we get some meat." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave until we've got some meat." / "Sooooo, Do us a favour" / "find somethin' with a little flavour" / "Cos we're goin' nowhere 'til we've got some meat." | todo | |
| The songs are shown as known dialogue, so they don't wait for the player to click on through them | todo | |

## Wood: finishing the boat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gifts are only looked for while all three sailors are alive | todo | |
| Every 3 seconds the script looks for a wood store (a pile of wood dropped by the hand) within 40 of the ark | todo | `CallNear` stub |
| If one is there and no sailor is in either hand, its wood is counted and the pile fades away | todo | `GetResource`, `ObjectDelete` stubs |
| Each 1,500 wood builds a whole ark's worth; the ark starts a fifth built, so 1,200 wood in all finishes it; the target is capped at fully built | todo | |
| The building scene: the sailors walk at 0.4 to three spots along the ark and hammer, look overworked, saw, carve or swing a sledgehammer at random (1 to 5 loops, then a 4 to 10 second pause) until the ark is fully built | todo | |
| A sailor says "Thank you for this wood."; the camera glides to the ark, cuts, then pulls back over it | todo | |
| The ark grows on screen by 0.03 every half second, with a random small woodpile sound each step, until it reaches the new target | todo | the built amount (`SetProperty`) and `PlaySoundEffect` are stubs |
| If the ark still isn't finished: "Can we have some more wood please?" | todo | |
| The sailors stay at work by the ark until it is finished, not by the fire | todo | |
| A tree dropped within 40 of the ark (checked every 3.4 seconds) brings the good advisor out once in the whole challenge: "A tree is no good. These people require prepared wood." | todo | `SpiritAppear`, `SpiritDisappear` stubs |

## Verse two: grain

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the ark is fully built, verse two plays (if no sailor is held) and grain becomes the next need | todo | undetermined: the script checks for the ark being exactly fully built, and its 0.03 steps from a fifth overshoot to 1.01, so the challenge relies on the engine capping the built amount at 1; the cap wasn't traced in the engine |
| Sailors that are flying are waited for, the others are put on their seats; all are drawn in high detail | todo | |
| The camera glides to the start of a recorded camera path and follows it while the second verse's music plays | partial | the music works (`StartMusic`); `RunCameraPath` and the camera conversions are stubs |
| The first sailor sits and sways, the accordion keeps playing; at the fourth line the second sailor is put on the ark's deck, walks along it, faces the camera and looks unimpressed, then is put back beside the ark and walks back to his seat to gossip | todo | |
| The challenge is recorded again: success 0.4, alignment 0.2, with verse two as the reminder | todo | `Snapshot` stub |
| The camera closes in on the fire with cuts every 3 seconds; when the words end, the first two sailors go back to idling | todo | |
| Every 3 seconds the script looks for a food store within 40 of the ark; its food is added up and the pile fades away | todo | `GetResource` stub |
| While the player answers, the interface is limited to moving the hand | partial | the interaction level works (`SetInterfaceInteraction`), the dialogue around it doesn't |
| Under 300 food in all: "That's good grain. Please can we have more?"; 300 or more: "We're grateful for the grain, Holy One." | todo | |

## Verse three: meat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At 300 food, verse three plays (if no sailor is held) and meat becomes the last need | todo | |
| The sailors are put back on their seats as in the first verse; the third verse's music starts and the words follow | partial | the music works (`StartMusic`), nothing else |
| The first sits and sways, the accordion plays, the second gossips; the record is updated to success 0.5, alignment 0.1, with verse three as the reminder | todo | `UpdateSnapshot` stub |
| The camera glides round the fire with cuts every 3 seconds, then cuts on the seventh, ninth and tenth lines of the song, ending in two quick moves | todo | |
| Every 3 seconds the script looks within 40 of the ark for a cow, a sheep, a horse or a pig, in that order, and takes one per check, fading it away | todo | `CallNear`, `ObjectDelete` stubs |
| A cow: "Great. Cattle to eat."; a sheep: "A sheep. Lovely. Sheep have many uses. And the voyage is long."; a pig: "A pig. Yeah, we'll have that."; after the first animal: "We need more meat, though." | todo | |
| Two animals are needed; the scroll is then taken away and a sailor says "Thanks for all this meat." | todo | |
| The interface is limited to moving the hand while they talk | partial | as for the grain; only the interaction level works |
| Quirk: a horse gets no line of its own; as the first animal it is answered "Thanks for all this meat." and then "We need more meat, though." | todo | the horse branch says the closing line by mistake |

## Extra hands: villagers, a woman and a galley boy

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3.3 seconds, while the ark is on screen and the camera within 200, the script looks for a villager within 40 of the ark who isn't held by a script | todo | `GameThingFieldOfView`, `CallNear` stubs |
| Before the boat is built, with no sailor dead: "Some more company is nice but we need to get the boat finished." | todo | |
| Once the boat is built, a man is taken aboard: "Hurrah! Another shipmate!"; he walks to the ark's door and vanishes, lost to his village | todo | `SexIsMale`, `SetScriptState` stubs |
| A woman is taken too: "This young lady will keep us all in check during the voyage." / "Go inside. You have your own cabin." | todo | the women taken are counted but the count is never used |
| While a sailor is dead, a man is taken as a replacement: "Great! Another sailor. Just what we need!" / "Go inside and pick out a uniform, mate." | todo | |
| While a sailor is dead, a woman is turned away: "It'd be lovely to have you along, but we need a man to steer the big heavy rudder." | todo | |
| After a refusal the script waits until that villager is more than 40 from the ark, or 20 seconds | todo | |
| Each man taken while a sailor is dead brings a new sailor out of the door, who walks to the dead one's seat and starts idling; the death count goes back down | todo | the new sailor is always a plain sailor, even when the accordion player died |
| Once the boat is built, the first child brought within 40 (checked every 3.1 seconds) becomes the galley boy: "Great! Someone to scrub the decks! Get inside!" | todo | |
| The galley boy is put on the ark's deck scrubbing it for good, facing the camera, which closes on him and pulls back with a lens change over 6 seconds, then returns to where it was | todo | `SetCameraLens`, `MoveCameraLens` stubs |
| Only one galley boy is taken; he leaves with the ark | todo | |

## Hurting the sailors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every pass of the main loop checks each sailor for having died or vanished, whatever killed him (hand, creature, miracle) | todo | `GetProperty` (health) is a stub; `ThingValid` works |
| The first seat's sailor dying: "Oh my god. You killed Kenneth!"; the others: "You killed my shipmate! Outrageous!" (not said for the last of the three) | todo | |
| A death records success at the dead share (a third per sailor) and alignment -0.2, then at once 0.3 and -0.4 if the second sailor still lives, or -0.6 if only the third does | todo | the first record is overwritten straight away |
| If the accordion player dies, the tune moves: a surviving sailor is swapped for an accordion player in the same seat, who plays on | todo | |
| With a sailor dead, the reminder becomes "They haven't got enough shipmates." (good advisor) and no gifts are accepted until a man replaces him | todo | |
| All three dead: the evil advisor: "You killed all the explorers!" / "But I can't get their stupid song out of my brain."; the record is set to success 1, alignment -1, and the challenge ends with no reward | todo | the ark, the campfire and the scroll stay |

## Running out of time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After three and a half hours the sailors walk to the ark's door; the record is set to success 1, alignment -0.6 | todo | |
| The good advisor's voice: "The Missionaries have given up waiting for you, Leader."; the sailors fade away | todo | |
| Every script of the challenge is stopped; the ark, the campfire and the scroll stay | todo | `StopScriptsInFilesExcluding` works, the rest doesn't |

## Setting sail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When wood, grain and meat are all given, all three sailors are alive, no verse is playing and no sailor is held, the campfire and the galley boy are taken away and the departure starts | todo | |
| The sailors and the ark are removed and a hand-made film of the ark sailing away starts (its own two boat animations from the game's misc data) | todo | `PlayJcSpecial` is a stub; the effect's name is in `src/ScriptHeaders/ScriptEnums.h` only |
| A heave sound, then two cheers 3.5 seconds apart as the camera glides along the boat | todo | `PlaySoundEffect` stub |
| The camera turns to the sea and a sailor says "Thank you, highness. We won't forget you, you know." as the third epic theme starts | partial | the music works (`StartMusic`), the line and camera don't |
| The dialogue box closes; a cheer, a close shot of the boat, then shots far out to sea to the west | todo | `GameCloseDialogue` stub |
| The record is set to success 1, alignment 0.2, with a picture taken | todo | `UpdateSnapshotPicture` stub |
| Back over the beach, the good advisor: "I just hate goodbyes."; the evil advisor: "I just hate good." | todo | |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Water miracle dispenser is built on the beach by the old camp, facing angle 0, and switched on | todo | dispensers exist (`MagicSystem::CreateDispenser`), but making one from a script (`CreateScriptObject` has no dispenser) and `SetMagicProperties` are stubs; see ../../miracles/dispensers_and_seeds.md and ../../miracles/water.md |
| Its refill time is left at the dispenser's own (the script asks for 0 seconds, which the engine ignores for a non-timer object) | todo | quirk: the helper's check meant "if a period was given" tests the game time instead |
| The reward sting plays and the camera glides to a view 21.5 to the side and 14 up from the dispenser over 4 seconds | todo | see [../rewards.md](../rewards.md) |
| If it is the player's first dispenser: the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles."; a signpost is put beside it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), "Click on the signpost for more info.", and a "did you know" scroll about miracles hidden around the land is placed ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") | todo | the first-dispenser lines are shared with every dispenser reward ([../rewards.md](../rewards.md)) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | todo | |
| The dispenser's own help lines are then spoken | todo | `GetFirstHelp`, `GetLastHelp` stubs |
| A storm reward falling from the sky was the original reward and is left commented out | n/a | not in the compiled scripts |

## Unused and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An advisor line "It looks like the men are getting ready to set sail." (good advisor) is in the text table but no script says it | n/a | not used by the shipped scripts |
| A sailor's line "How are we going to sail the ship now we don't have a full crew ?" is in the text table but no script says it | n/a | not used by the shipped scripts |
| The quest's text calls the sailors missionaries throughout, though the scroll's title is "The Explorers" | n/a | naming only |
