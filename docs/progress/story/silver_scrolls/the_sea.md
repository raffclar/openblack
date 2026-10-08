# The Sea

A land 2 silver scroll at a hut in the Indian town on the shore: a mother's five sons have swum out into the bay and
won't come home. The player (in practice the creature, since the bay is shut to the player's hand) fetches them out of
the water, carries the mother down to the beach to scare them home, or kills them; each ending sets its own
alignment. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** a mother (Indian housewife) at her hut in Town2 (the Indian town by the bay) · **Script:** Baywatch · **Reward:** a "Big" creature miracle dispenser, only if all five sons walk home alive · **Repeatable:** no

Sources: the quest's script source (`Baywatch.txt`) and the land's control script, checked line by line against the
compiled form in the shipped `challenge.chl`; the shared helpers it runs (the scroll notify loop, the standard
reminder, the dispenser reward); the game's English text table; and the executable (read-only) for what a challenge
log entry does with its success and alignment, and how the script's random numbers are drawn. openblack's state is
judged on the physics work tree (`ob-wt-physics`): of the 62 script functions this quest and the scripts it starts
need, 42 still only log "not implemented" in `src/CHLApi.cpp` (among them the scroll, dialogue, advisors, villager
animation and state, moving a villager, held-in-hand checks, sounds, the moving camera and the challenge log), and
the land's control script stops at its first unwritten function long before this quest's check is started (see
[../land_2.md](../land_2.md) row 1), so the quest never appears; every row below is todo unless the notes say
otherwise.

**Progress: 0/77 done, 0 partial — 0%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher for this quest along with the land's other challenge watchers | todo | the control script stops before reaching it (`GetTownWithId` and earlier commands are stubs) |
| The watcher waits 4 minutes, then checks whether both Town6 (the Indian town near Lethys's snowy edge, which has no challenge of its own) and Town2 (the Indian town where the scroll appears) belong to the player; if not, it waits another 4 minutes and checks again, for ever | todo | the land script's comment: "Check for take over of Town6 and Town2 every 4 minutes for Baywatch triggered on Town 2"; town ownership reads are stubs |
| Once both towns are the player's, the quest starts once and the watcher ends; nothing can start it a second time | todo | |
| Before the scroll appears, the bay around the swimming spot is ringed with an anti-influence area of radius 30, so the player's hand has no influence there (and so can't reach the children directly) | todo | `InfluencePosition` (the anti flag) is a stub; openblack does have anti-influence areas for the shield miracle (`InfluenceSystem.cpp`), but the script can't make one |
| A silver scroll appears on the ground at the mother's hut, at the hut's position on the cliff top above the bay (about 186 m inland of the swimming spot); its height isn't changed by the script | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and the scroll is on screen, the evil advisor steps out, points at it and says "Hey. There's a job for you. Wanna do it?", at most once every 30 seconds and only when no film is playing | todo | the shared scroll-notify loop; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| The quest waits until the scroll (or the hut spot) is clicked; the scroll is then switched on and the rest begins | todo | `GameThingClicked`, `SetActive` are stubs |
| No mother and no children exist until the scroll is clicked: the mother (an Indian housewife) is made at the hut, and five Indian farmer boys are made at five spots close together in the bay | todo | creating villagers does nothing (`CreateScriptObject` only makes scenery and rocks) |
| A log entry is recorded during the introduction (see below), not when the scroll appears | todo | |

## The children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each son is made as a grown farmer and then set to age 11, so he looks like a child | todo | `SetProperty` (age) is a stub |
| Each son waits a random 1 to 4 seconds, then starts a looping swimming animation and turns to face a spot in the middle of the group | todo | `SetScriptUlong`/`SetScriptState` (animation), `SetFocus` are stubs |
| A count of children in the water starts at 5 | todo | |
| A son lifted out of the water by the player's hand or the creature's hand leaves the in-water count (once, however long he is held) | todo | the held check and `InCreatureHand` are stubs |
| Each son's script waits until he is no longer held by either hand and no longer flying through the air before deciding what happened | todo | |
| Put down (or landing) alive on dry land (ground height above 0): he plays a lost-in-the-crowd animation once and says "All right I'm going home.", then walks back to the hut at half speed | todo | spoken in the boy narrator's voice, each time any son lands on dry land |
| Put down (or landing) alive in the water (ground height 0 or below): he stays where he is, starts swimming again and rejoins the in-water count | todo | a son can therefore be taken out and dropped back many times |
| A son who reaches within 5 of the hut alive counts as home and his script ends; dropping him by the hut counts at once | todo | `MoveGameThing` (the walk home) is a stub |
| A son's death is noticed whenever his health falls to 0 or below, wherever he is (swimming, walking home, after a throw); he counts as killed, leaves the in-water count if he was swimming, and his script ends | todo | whatever kills him counts the same: a throw, a miracle, a rock, the creature |
| The first son to die (only the first) brings out the evil advisor: "That'll teach the kids a lesson." and then the good advisor: "How savage! Killing such innocents!" | todo | |
| Nothing in the script drowns the children or makes them tire: they swim until fetched or killed, and the quest never ends by time | todo | the unused lines below show an earlier plan where they were in trouble in the water |
| Whether a son eaten by the creature counts as killed (health read as 0) or is lost track of is undetermined | todo | depends on what the health read gives for an eaten (removed) villager; not checked |
| Once a son is home, nothing more is watched for him: killing him later does not change the count | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen) starts with the script music "generic 3"; the mother is drawn in high detail | todo | `StartMusic` works in openblack, but the film is never reached; `SetHighGraphicsDetail` is a stub |
| The camera glides over 3 seconds to the hut while the mother walks at half speed to a spot about 12 m from it | todo | `MoveCameraPosition`/`MoveCameraFocus` are stubs |
| She plays a looking-for-something animation as the camera creeps in over 6 seconds | todo | |
| The mother: "My children have gone swimming, and they have not returned." | todo | |
| The dialogue closes and the camera cuts to a low view over the water looking at the children | todo | `SetCameraPosition`/`SetCameraFocus` work in openblack |
| The challenge log entry is recorded here, titled "The Sea", success 0, alignment 0, its picture taken from this view of the swimming boys; its reminder is the good advisor: "This woman's children have not returned from swimming." | todo | `Snapshot` is a stub |
| The mother (over the view of the boys): "I worry for their safety, and I'm too tired to find them." while the camera glides lower over the water for 6 seconds | todo | |
| The camera cuts back to the hut and pulls up and back over 4 seconds (its aim over 3); the mother's speed drops to a fifth | todo | |
| The high detail and the music stop, the film ends, and the mother wanders around her spot (within 6 of it) | todo | `SetScriptStatePos`, `SetScriptFloat`, `SetScriptUlong` are stubs; the two whole-number settings of the wander (4 and 20) are not determined |
| From the end of the film, the children laugh: at the swimming spot, each time the script runs, a random one of seven child-laugh sounds plays with a chance of 1 in (250 ÷ swimmers + 1), so 1 in 51 with five swimming and rarer as fewer swim; the laughing stops for good once no child is in the water, even if one is later put back | todo | `PlaySoundEffect`, `RandomUlong` are stubs; the chance comes from the game's random draw cutting a number from 0 to the limit + 1 down to a whole number |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The main watch starts 5 seconds after the film and runs until one of the endings below | todo | |
| Bring each son out of the water and set him down on dry land so he walks home, or drop him straight by the hut; with the bay shut to the player's influence, the creature is the usual way to fetch them | todo | the unused line "Let's get the Creature over there to scoop them out." shows that intent |
| Or take the mother to the beach: while all five sons are still swimming, the mother within 40 of the swimming spot and held by neither the player nor the creature starts the beach ending | todo | the hut is about 186 m from the spot and she only wanders within 6 of her own spot, so she must be carried or thrown there; as the hand has no influence within 30 of the spot, she has to be dropped (or thrown) to the 30–40 ring |
| The beach ending is only open while every son is in the water: once any son has been taken out (and not put back) or killed, it can no longer happen | todo | |
| Bringing the camera within 20 of the swimming spot with it on screen makes a son call out, once only: "We're having too much fun, we're not going anywhere." | todo | `GameThingFieldOfView` is a stub |

## Endings

Every ending records the challenge log's final state (success and alignment) for "The Sea". Updating an existing log
entry applies the given alignment to the player's alignment (scaled by a per-player factor that is not determined
here); success is kept between 0 and 1 and alignment between -1 and 1.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All five sons home and the mother alive: a film at the hut; the mother: "Thank you for bringing back my loved ones." | todo | |
| …the reward dispenser is put down quietly (no reward sting, no advisor talk, no help) and the camera flies over 5 seconds (its aim over 2) to look at it; the log entry is recorded again with a new picture of the dispenser: success 1, alignment +1, same reminder | todo | the script uses "snapshot challenge" a second time rather than "update snapshot"; the game finds the existing entry and updates it, taking a new picture |
| Some sons home and the rest killed (one to four killed), no child left in the water, the mother alive: a film at the hut; log update success 1, alignment -0.2, no new picture; the mother: "I'll find where the others got to, I'm sure they'll be somewhere." No reward | todo | |
| All five sons killed and the mother alive: a film at the hut; log update success 1, alignment -0.6, no new picture; the mother: "So you couldn't find my loved ones. I'll have to search for myself."; then the evil advisor: "She'll be searching for a long time. Ha!" No reward | todo | |
| All five sons killed and the mother dead: no film, just dialogue; the evil advisor: "A whole family slaughtered. Nice going." The log is updated with the "details" form: success 1, alignment -1, title "The Sea", the current camera view passed but no new picture taken, and no reminder given | todo | `UpdateSnapshotPicture` (with picture-taking off) is a stub; the source comment quotes the line as "A whole family slaughtered, nice going!"; whether the old reminder is cleared by passing none is not determined |
| Every son accounted for (home or killed, at least one home) but the mother dead: a film at the hut; log update success 1, alignment -0.8, no new picture; the good advisor: "Oh how terrible. Orphans." No reward | todo | this includes all five home alive with the mother dead |
| Beach ending, mother alive: a film at the beach; a son: "Quick, run! It's mom!"; the screen fades to black over 2 seconds | todo | `SetFade` and `FadeFinished` work in openblack, but the film is never reached |
| …in the dark the mother is put back at her spot and every son (all are alive, since all five were swimming) is put around her, each in an idle animation (two plain idles, a lost look, an unimpressed look); she turns to face the camera at the hut; fade in over 2 seconds | todo | `SetPosition` works in openblack; the animations are stubs |
| …the mother: "Thank you for bringing back my loved ones."; the log entry is recorded again with a new picture of the family: success 1, alignment +0.8; the camera pulls up and back over 4 seconds | todo | no reward is given on this ending |
| Beach ending, mother dead (her body within 40 of the spot while all five swim): a film at the beach; the woman's voice: "Aaahhh!"; fade to black over 2 seconds | todo | the source comment writes it "AAaahhh !!!!" |
| …her body is put back at her spot lying dead, and the five sons are put around her, each facing her: two inspecting her, one mourning, one overworked, one scared stiff; fade in; the log entry is recorded again with a new picture of this scene: success 1, alignment -0.5; the camera pulls up and back over 4 seconds | todo | no reward |
| The watch ends with the first ending reached; the children's scripts and the laughing are then stopped | todo | `StopScript` works in openblack |
| A quest with sons still in the water simply waits: there is no time limit, no failure ending and no way to abandon it | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reward is a miracle dispenser for the creature miracle "Big", built (as a Norse dispenser building) on the cliff top between the hut and the bay, about 74 m from the hut, turned 180°, and switched on | todo | dispensers exist in openblack (`MagicSystem::CreateDispenser`) but `CreateWithAngleAndScale` makes only scenery and rocks, and `SetMagicProperties`/`SetActive` are stubs; see [../rewards.md](../rewards.md) and [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| The dispenser's refill time is set to 0 seconds (the helper always sets it, because it tests the game time rather than the given refill time) | todo | `SetTimerTime` is a stub; what a 0 refill time means for the dispenser is not determined |
| It is given quietly: no reward sting, no camera fly-to from the helper, no first-dispenser tip or signpost, no help lines, and it does not count towards the land's dispenser-reward tally | todo | the quest's own camera flies to it instead |
| Only the all-home, mother-alive ending gives it; the equally good beach ending (+0.8) gives nothing | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mother and the surviving sons stay in the world | todo | they are never released from the script; whether they go back to normal village life is not determined |
| The anti-influence ring over the bay and the scroll are never removed by the script | todo | whether the game removes them when the challenge ends is not determined |
| The reward dispenser stays where it was put | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor brings the job to the player's notice (the scroll-notify line) | todo | |
| The log's reminder is the good advisor's "This woman's children have not returned from swimming." | todo | the shared reminder script brings out whichever advisor owns the line |
| The first killing: evil "That'll teach the kids a lesson.", good "How savage! Killing such innocents!" | todo | |
| All sons killed, mother alive: evil "She'll be searching for a long time. Ha!"; whole family dead: evil "A whole family slaughtered. Nice going."; orphans: good "Oh how terrible. Orphans." | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the script music "generic 3" and stops it at its end | todo | `StartMusic`/`StopMusic` work in openblack but are never reached |
| The children's laughter (seven samples, from the script sound bank) is heard at the swimming spot while any child swims | todo | `PlaySoundEffect` is a stub |
| The endings have no music or sound of their own | todo | |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A son held in the creature's hand counts the same as one in the player's hand; where the creature puts him down decides whether he walks home or swims again | todo | `InCreatureHand` is a stub |
| The mother in the creature's hand does not start the beach ending until the creature lets go | todo | |
| A son the creature kills counts as killed (whether eating counts is not determined, see above) | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll-notify helper is called with four values (no notify distance), so it uses the shared 100 | todo | the cut Big Fish quest calls it with five |
| The all-home, mother-alive ending and the beach ending record the log with "snapshot challenge" a second time instead of "update snapshot"; the game treats it as an update with a new picture | todo | |
| The beach ending checks each son's health before gathering him, but it can only start with all five swimming, so all five are always alive there | todo | dead checks |
| A son killed with others still swimming is not followed by any ending until every son is home or dead, so a single son left swimming holds the quest open for ever | todo | |
| If the mother dies with all five sons home, the orphans ending is used (alignment -0.8), not the thanks ending | todo | |
| The "Aaahhh!" of the dead-mother beach ending is in the woman's voice although the mother is dead | todo | |
| The main watch and the laughing loop have no wait, so they run every time scripts run | todo | |
| A son dropped on dry land somewhere he can't walk home from never counts as home, which also holds the quest open | todo | the walk home is ordered once and never repeated |
| The dispenser helper's refill check tests the game time instead of its refill argument, so the refill time is always set | todo | shared helper |
| A testing script that sets up land 2 and runs this quest at once exists in the sources but is not in the shipped program | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text table holds seventeen more lines of this quest that no script uses (numbers 4 to 14 and 16 to 21; 1 to 3 don't exist), from an earlier plan where the children were in danger in the water and the woman judged the player | n/a | cut content; listed for completeness |
| Unused, on killing the woman: evil "You killed the woman, Boss. Now that is a cool, calculated action. I approve."; the woman: "You disgraceful, murdering overfiend. No true god would act like that. I'm repulsed."; evil "Oh this is priceless. They can't take a bit of evil."; good "If I wasn't stuck inside your head, I'd leave and become a ghost or something." | n/a | cut content |
| Unused, on success: the woman: "What a stunning performance. I'm so, so grateful. And the kids will be pleased, too."; evil "Huh. Am I wasting my time pushing the evil point of view? Are you gonna listen to me?" | n/a | cut content |
| Unused, on a cold result: the woman: "That was rather cold-hearted."; evil "Ooh, this girl is feisty. I like it."; good "Oh shut up and take your telling off like a man, demon."; evil "Who rattled your cage, you do-gooding nimrod?" | n/a | cut content |
| Unused, on failing: the woman: "Er, lordly one. You're gave it a good go. Thank you for trying." | n/a | cut content (the typo is in the text table) |
| Unused, spotting the children: good "Look! Look down there! Children are in trouble in the water!"; good "We're not too good at this sort of thing. Remember the fisherman we failed to save?"; evil "Great! I get to see more people drown. Pass the popcorn!"; good "We should be able to save them. We saved the fishermen before."; good "Let's get the Creature over there to scoop them out."; evil "You mean, let's get him over there for a salty snack!" | n/a | cut content; the fisherman lines refer back to an earlier fisherman rescue |
| Line 15 of this quest is asked for by the cut Big Fish quest's scroll but is not in the text table | n/a | that quest belongs to the shared/unused list |
