# The Sacrifice

A second-land silver scroll: an Indian priest, his wife and their small son come to the player's new worship site, where
the priest offers his firstborn to the altar. Anything sacrificed there counts towards a total of 4000 of prayer power;
who (or what) the player gives up, or spares, sets the outcome's alignment. Reaching 4000 with the boy alive earns a
heal miracle reward.

**Land:** 2 · **Giver:** an Indian priest and priestess from the Indian village the control script calls Town6 (town id 11), at the player's worship site · **Script:** Sacrifice · **Reward:** a heal miracle chest from the sky (only for reaching 4000 without killing the family) · **Repeatable:** no

Sources: the quest's script source (`Sacrifice.txt`, with the land control script `LandControl2.txt` and the shared
helpers `ChallengeNotify.txt`, `StandardReminder.txt`, `Reward.txt`), checked line by line against the PC game's
compiled `challenge.chl`; the game's text table; the land file `Land2.txt` for which town sits where; and the
executable for what reading a worship site's sacrifice total returns. openblack is judged on the physics work tree
(`ob-wt-physics`): of the 58 script functions the quest and the scripts it runs need, 40 only log "not implemented" in
`src/CHLApi.cpp`, among them every dialogue, advisor, camera glide, highlight, snapshot, timer, sacrifice-total and
reward command; creating villagers does nothing (`Create` only makes scenery and rocks). The land's control script also
stops long before it starts this quest (see [../land_2.md](../land_2.md) and
[../../scripts/land2_script.md](../../scripts/land2_script.md)), so none of it happens; rows are todo unless the notes
say otherwise. The script's function coverage is in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md); how sacrifice gives prayer power in
[../../worship/prayer_power.md](../../worship/prayer_power.md).

**Progress: 0/71 done, 4 partial — 3%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher for the quest as soon as the land is set up | todo | `LandControl2.txt` never gets that far in openblack |
| Every 11 seconds the watcher looks for a worship site within 15 of a fixed altar spot by the player's home village (the site the player is taught to build in this land; the plague's watcher uses the same spot) | todo | `GetProperty` (built) and the worship-site search (`Call`) are stubs |
| The quest starts only once that worship site is fully built (built exactly 1) and the Indian village with town id 11 belongs to the player; the village is checked only after the site is built, and the watcher then stops | todo | the plague's watcher tests "built at least 1" for the same site; this one tests "exactly 1" |
| The scroll is a silver scroll (a challenge highlight) beside a house about 100 north of that Indian village's centre, far west of the altar | todo | `CreateHighlight` is a stub |
| Until the scroll (or the spot it marks) is clicked, whenever the camera is within 100 of it and it is on screen, the evil advisor pops out at most every 30 seconds, points at it and says "These people look like they're doing something interesting, Boss." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `GameThingFieldOfView`, `RunText` are stubs |
| Clicking it makes the scroll active (opened) and the quest begins at once; there is no time limit on clicking it | todo | `GameThingClicked`, `SetActive` stubs |
| Only on the click are the family made at the house: a 6-year-old Indian boy (an Indian farmer villager made a child by age), a priest and a priestess, all three joining the Indian village | todo | `Create` makes no villagers; `FlockAttach`, `SetProperty` (age) stubs |
| After the introduction the scroll is moved to beside the altar (about 20 from it) and stays there; the script never removes it | todo | `SetPosition` works on things openblack can make, but the highlight is never made |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene under the Gregorian chant script theme | partial | widescreen and the music command work (`SetWidescreen`, `StartMusic`, the track is in `src/Audio/GameMusic.cpp`); the scene itself never runs |
| The three are drawn in high detail; the priest and priestess walk a few paces from the house while the boy shuffles along very slowly (speed 0.1) | todo | `SetHighGraphicsDetail`, `MoveGameThing`, `SetProperty` (speed) stubs |
| The camera glides in on them over 5 seconds, then closer over 6 | todo | `MoveCameraPosition`/`MoveCameraFocus`, `HasCameraArrived` stubs |
| After 5 seconds the priest, gossiping, says "Greetings, almighty presence." while the boy stands scared stiff and the priestess idles | todo | animations: `SetScriptState`/play stubs |
| The camera eases round; the priest: "As a gesture of gratitude from our people, we have a gift for you." and the dialogue closes | todo | `RunText`, `TextRead`, `GameCloseDialogue` stubs |
| The boy (now at speed 0.2) and his parents walk off to a spot about 50 south; the camera pulls back and up over about 8 seconds, rising to 80 high | todo | |
| The screen fades to black over 2 seconds; the camera is cut to the player's altar and the family is put down beside it, each then taking one step | partial | the fade and setting the camera work (`SetFade`, `SetCameraPosition`, `SetCameraFocus`, `SetPosition`); the family isn't made |
| The screen fades back in over 2 seconds; the parents face the camera, and the boy mourns, looking away towards one spot | todo | `SetFocus` stub |
| The camera drifts over 8 seconds; the priest: "We are a spiritual tribe, with ancient customs." | todo | the developers' comment calls them a tribe with "mad customs" |
| The quest is recorded: title "The Sacrifice", 0 done, alignment 0, reminder "These people await a sacrifice from you." (evil advisor), with the current view as its picture. The snapshot records the challenge in the player's challenge log with its title and reminder | todo | `Snapshot` is a stub; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| The parents turn to the boy; the priest: "If you place anything on this altar, its life-force is added to our stored prayer power." | todo | |
| The priest: "As a gesture of my belief, I offer my firstborn son to the altar." (waits for the player to click on) | todo | |
| The text clears and the camera pulls back over 3 seconds to take in the altar | todo | `GameClearDialogue` stub |
| Both advisors pop out. Evil: "Now that's what I call a gesture of belief!" The evil advisor then points at the altar while the good advisor answers: "But human sacrifice is wrong! Why not sacrifice something else? Like a plant." (waits for a click) | todo | `SpiritEject`, `SpiritPointPos` stubs |
| The family go back to normal detail, the music stops and the scene ends | partial | `StopMusic` works; the rest is stubs |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must sacrifice 4000 worth of prayer power at the worship site, by whatever means, or choose to sacrifice or kill one of the family | todo | the hand can't hold villagers or animals yet (see [../../worship/prayer_power.md](../../worship/prayer_power.md)) |
| What counts is the site's own running sacrifice total, read once after the introduction and then continually; only what is added after the introduction counts | todo | `GetSacrificeTotal` is a stub; in the game it reads a total kept on the worship site |
| The script only reads the total, so anything that adds to it counts (trees, animals, villagers, the boy himself); it does not look at what was sacrificed | todo | |
| The developers' notes give the sacrifice values they planned around: a tree 250 to 1000, a decent animal 700 to 2000, a human 10,000 to 20,000, "so required is about 4K" | n/a | source comment only |
| There is no time limit and nothing ends the quest except one of the outcomes below | todo | |

## Rules while it runs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the boy is alive he cries every 4 seconds, a random one of eleven child-crying samples played at him, until the quest ends | todo | `CreateTimer`, `GetTimerTimeRemaining`, `PlaySoundEffect` stubs |
| Whenever the total rises, after 2 seconds the priest (if alive) judges the offering by how much it added: under 700 "Hmm. That didn't produce much prayer power."; 700 to 2000 "Yes. Feel the power."; over 2000 "That's a powerful sacrifice. A whole heap of prayer power has been stored from it." | todo | `StartDialogue`, `RunText` stubs |
| The first time an offering adds under 700, the evil advisor first grumbles "Bah, humbug!" (once per quest) | todo | |
| If the priest is dead when the total rises, only the evil advisor's "Bah, humbug!" is said, and only if not said before | todo | this is what happens when the priest himself is sacrificed, just before his ending |
| Offerings made during the 2-second pause and the priest's line are added together and judged as one next time round | todo | |
| The scroll's progress is the amount sacrificed since the start divided by 4000, updated continually with alignment 0 and the same title and reminder; it is capped at 1 once 4000 is reached | todo | `UpdateSnapshot` stub; undetermined how an update with alignment 0 interacts with the 0.5 recorded when the boy is let go |
| Each time round, if no ending has happened, the script notes which of the three (if any) the player is holding in the hand; this is how it later tells "sacrificed" from "killed" | todo | `GetObjectHeld1` stub |
| An ending counts as a sacrifice when that person no longer exists and was the one in the hand the last time the script looked; a person who dies any other way (leaving a body) counts as killed | todo | undetermined: whether the altar removes a villager straight from the hand, which this test relies on |
| The endings are checked in a fixed order: the priest first, then the priestess, then the boy; only if all three live are the boy's release and the 4000 total looked at | todo | |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each death ending is a short scene: the camera is cut to the altar view, the line is said (waiting for a click), the record is closed fully done, and the camera is put back where it was | todo | `SetCameraPosition`/`Focus` work but the scene's other commands are stubs |
| The priest sacrificed: the boy blows a raspberry at his mother, who looks at the camera unimpressed: "It's what he would have wanted." Alignment +1 | todo | `Snapshot` stub |
| The priest killed any other way: the same gestures, the priestess: "You killed my husband! But he was a good priest!" Alignment +0.5 | todo | |
| The priestess sacrificed: the boy blows a raspberry at his father, who looks at the camera unimpressed: "Ah, so you thought my wife'd make a better sacrifice. Interesting," Alignment 0 | todo | |
| The priestess killed any other way: the priest, unimpressed: "My wife! You do work in mysterious ways, don't you?" Alignment -0.2 | todo | |
| The boy sacrificed: the priest looks at the camera impressed, the priestess unimpressed: "Excellent. I knew sacrificing my lad would provide a huge power-boost." Alignment -1 | todo | the boy's sacrifice also raises the total first, so the priest's "That's a powerful sacrifice..." line comes just before |
| The boy killed any other way: the priest, defeated: "Er, you were supposed to sacrifice him on the altar. Such a waste." Alignment -1 | todo | |
| None of the death endings gives a reward, and the quest is over after any of them | todo | |
| Reaching 4000 with all three alive: the camera glides to the altar over 4 seconds; after 2 the boy blows a raspberry at his father | todo | |
| The priest faces the camera: "The power from that sacrifice will remain stored until you need it." while he prays and the priestess cheers; the boy turns to the camera and does a mocking dance | todo | the dance and the priestess's cheer are set to loop without end |
| The priest: "You allowed my son to live. I bow to your will." (waits for a click). The record is closed fully done with alignment +1 | todo | |
| The reward then follows in its own scene (below) | todo | |

## Letting the boy go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the boy is more than 100 from the altar and is neither in the hand nor flying (the player has carried him off and put him down), a scene cuts to the altar view and the priest, waiting impatiently: "We are awaiting a sacrifice." (waits for a click) | todo | `GetProperty` (held, flying) stubs |
| The record is updated: 0 done, alignment +0.5; the boy is let go from the script to live as an ordinary villager; this happens only once | todo | `ReleaseFromScript` stub |
| Unlike the endings, this scene does not put the camera back where it was; it is left on the altar view | todo | |
| Letting him go does not end the quest: the 4000 total is still asked for, and the boy's later death still ends it with his "killed" (or "sacrificed") line and alignment -1 | todo | |
| The parents are never let go by the script in any outcome; only the released boy goes back to normal village life | todo | undetermined what the scripted parents do once the quest is over |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the 4000 ending gives one: a heal miracle chest, dropped from the sky at a spot about 20 in front of the altar | todo | `CreateRewardInTown` stub; see [../rewards.md](../rewards.md) |
| The chest is given "in" the Indian village whose centre is at the given spot (town id 12, the one the control script calls Town1, the nearest village), not the sacrificing family's village | todo | the land file gives that village the heal miracle already |
| What the chest holds follows the reward rules: a town that can't yet cast heal learns it; if the player already has heal, the next heal power-up the player lacks is given (to that village) | todo | [../rewards.md](../rewards.md) "Miracle rewards" |
| The reward scene picks one of two camera shots at random (high, 80 up and 60 back, or low, 15 up and 15 back), glides in over 4 seconds, looks up for the falling chest, then follows it down from 25 up and 30 back | todo | the shared reward-from-sky script; camera glides are stubs |
| Clicking the chest says its help line, if the dialogue is free (the first reward of the game would get the special first-reward lines instead, which can't happen by this land) | todo | `GameThingClicked`, `GetHelp` stubs |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The prayer power given stays in the site's store like any other sacrifice | todo | see [../../worship/prayer_power.md](../../worship/prayer_power.md) |
| On finishing, the script sets a land flag marking the sacrifice as completed; nothing in the shipped scripts reads it | todo | the flag is declared by the land's set-up script |
| Nothing else in the land's story depends on the outcome | todo | |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The priest's lines are the "man" narrator voice, the priestess's the "woman" voice; the advisors use their own voices | todo | the voices are read past; see ../../scripts/info_scripts.md |
| Clicking the active scroll again speaks its reminder, "These people await a sacrifice from you.", through the evil advisor | todo | the shared reminder script; advisors are stubs |
| Music: the Gregorian chant script theme for the introduction only; the endings have none | partial | `StartMusic`/`StopMusic` play and stop it; the scene never runs |
| Sounds: the boy's eleven crying samples, every 4 seconds while he lives | todo | `PlaySoundEffect` stub |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script never mentions the creature; whatever the creature sacrifices at the site adds to the same total, and a family member it kills counts as "killed" (it was not in the player's hand) | todo | undetermined beyond what the script reads |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sacrificing the priest gives the best alignment of all (+1, the same as sparing everyone), better than killing him any other way (+0.5) | todo | as written and compiled |
| The scroll and the family start at the Indian village's house, but the whole quest then takes place at the player's own altar across the land; the family is simply moved there in the fade | todo | |
| The reward is given to a third village (town id 12), neither the family's nor the player's home | todo | |
| The control script's comments disagree with its code: the comment on the village with id 10 says the sacrifice is "at town 2", the comment on the village with id 11 says it has "no challenge", yet the watcher is given the village with id 11, as is the developers' test script | n/a | comments only |
| After the record line in the introduction the script waits for a line to be read though none is showing | todo | harmless |
| A running total of all offerings is kept but never used | n/a | |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every line of the quest's text (21 lines) is used; nothing is cut | n/a | |
| A developers' test script starts the quest on its own with the village with id 11; an unshipped land-one control script (one developer's copy) also started it | n/a | not started by the game |
| A saved game keeps the family, the total read at the start and the record | todo | openblack has no saved games |
