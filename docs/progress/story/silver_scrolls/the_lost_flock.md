# The Lost Flock

A first-land silver scroll: a shepherd has dozed off and nine of his sheep have strayed all over the island. Each sheep
brought back to his field joins his flock; five back earns a large food reward, all ten back earns a sheep the player
can take as their creature. Killing the shepherd, or letting five sheep die in his field, ends it badly.

**Land:** 1 · **Giver:** the Norse shepherd, from the house beside his field next to the player's village · **Script:** TheLostFlock · **Reward:** a large food reward at five sheep; a sheep creature to swap to at ten · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on the physics work tree
(`ob-wt-physics`): of the 81 script functions the quest and the scripts it starts need (including the creature-swap script it hands over to), 65 only log "not implemented"
in `src/CHLApi.cpp`, among them every dialogue, advisor, camera move, flock, highlight, snapshot, timer and reward
command; creating villagers and animals does nothing (`Create` only makes mobile statics). The land's control script
also stops long before this quest is started (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), so
none of it happens; rows are todo unless the notes say otherwise. The land as a whole is in
[../land_1.md](../land_1.md), the script's function coverage in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/80 done, 3 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script is started early in the land, right after the opening (the family leading the player home and the temple), before the creature is chosen | todo | the land's control script never gets that far |
| On starting, it places the nine lost sheep at once, so they are out in the world long before the scroll shows | todo | `Create` makes no animals |
| The lost sheep are: one hidden in a flock of five pigs in another farmer's field, one behind the creature gates, one near the hermit's cottage, one at the sculptor's house, one in a beach gully, one at the throwing-stones range, one by the lost brother's spot, one on a peninsula near where the crowd gathers, and one on the hill above the town | todo | |
| The pigs' flock that hides the sheep keeps within 5 to 8 of its spot | todo | `FlockCreate`, `ChangeInnerOuterProperties`, `PopulateContainer`, `FlockAttach` are stubs |
| Every lost sheep is always drawn, however far away, so it can be spotted from a distance | todo | the always-visible special flag (`ThingJcSpecial`) is a stub |
| Each lost sheep bleats with its own sample (nine different bleats) every 6 to 8 seconds while it is alive and more than 10 from the shepherd's field; it falls silent once there, once dead or once the quest is over | todo | `PlaySoundEffect` (a sound at a thing) is a stub |
| An empty flock is made at the shepherd's house, keeping its members within 5 to 12 | todo | |
| The scroll waits until the creature's trainer has taught the leash and tied the creature to a house; only then does a silver scroll appear over the shepherd's house | todo | the land raises the flag after that lesson; `CreateHighlight` is a stub |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor pops out at most every 30 seconds, points at it and says "Your godly attention is required here, Leader." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll makes it active (opened) and the introduction begins | todo | `GameThingClicked`, `SetActive` stubs |
| Later, the guide's naps suggest unfinished silver scrolls: if the Explorers' scroll has been opened and isn't the outstanding last suggestion, and the flock's scroll hasn't been opened (or the flock was the last suggestion and isn't done), the good advisor says "You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it." and the camera sweeps up and over to the shepherd's field | todo | see [../creature_guide.md](../creature_guide.md); the suggestion order is the Explorers, then this flock, then the Pied Piper |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with the first generic script theme | partial | widescreen and script music work (`SetWidescreen`, `StartMusic`); the scene itself never runs |
| The shepherd (a Norse shepherd villager) appears at his house with one sheep, which is sent to his field | todo | `Create` makes no villagers or animals |
| He walks very slowly out to a spot in front of the house (speed 0.08) with a tired-eyed walk while the camera follows him | todo | `SetProperty` (speed), `MoveGameThing`, `OverrideStateAnimation`, `FocusFollow` are stubs |
| The quest is recorded: title "The Lost Flock", 0 done, alignment 0, reminder "There are still sheep missing!" (good advisor) | todo | `Snapshot` is a stub; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He faces the camera and yawns: "By the great gods, I have been asleep!" | todo | `SetFocus`, `RunText` stubs |
| Throwing up his arms: "I was just counting my sheep when I dozed off." | todo | |
| Looking about: "Please find my straying sheep." | todo | |
| If the sheep on the hill above the town is still there, the camera cuts to it for about four seconds, drifting, and it bleats; then cuts back to the shepherd. Otherwise the camera simply turns back to him | todo | `SetCameraPosition`/`SetCameraFocus` work, but the moves around them (`MoveCameraPosition`) don't |
| The shepherd stands in despair, weeping, then looks over to his field | todo | |
| The camera pulls back to look over the field as he walks to it at speed 0.15; the music stops | todo | |
| He becomes the leader of his flock with his one sheep, and wanders around the field (within about 6) | todo | `FlockAttach`, `SetScriptState`, `SetScriptStatePos`, `SetScriptFloat`, `SetScriptUlong` stubs |
| Both advisors pop out. Evil: "Well, they were going to be slaughtered anyway. I say we waste the wandering mutton." Good: "But if we find and return them, we'll have a dyed-in-the-wool follower." Evil: "Wow. The saintly one made a joke." | todo | `SpiritEject`, `RunText`, `TextRead` stubs |
| Any loose sheep already within 50 of his flock when the scene ends (ones the player brought early) join the flock at once | todo | |

## Bringing the sheep back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player brings the sheep back by any means (carrying them in the hand, throwing them, the creature carrying them); the scripts only count the shepherd's flock | todo | the hand and throwing work in openblack, but nothing counts a flock |
| A returned sheep joins the shepherd's flock through the game's own flocking: an animal on its own looks about itself for a flock of the same kind to join, as long as the joined flock stays within the species' flock size | todo | undetermined: exactly how a sheep joins a flock led by a villager rather than another sheep |
| If the flock has only the shepherd left in it, the script itself adds any sheep within 50, checking every 0.7 seconds | todo | |
| The second, third and fourth sheep in the flock each get "Many thanks, higher power. Keep them coming, please." (once each) | todo | the count includes the shepherd, so these play at flock sizes 3, 4 and 5 |
| If more than three were already there when the introduction ended, these lines start from that count | todo | |

## Five sheep: the first reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the flock reaches the shepherd and five sheep (four found), the script waits until the shepherd is not held by the hand or the creature, puts him back home if he is more than 50 away, and gathers the flock there | todo | |
| A widescreen scene: the shepherd walks to a spot by his house while the camera closes in over 5 seconds | todo | |
| He faces the camera and shifts about; the record becomes 99% done with alignment +0.4 and the reminder "The shepherd seems to think we have returned all his sheep." (good advisor) | todo | `Snapshot` stub |
| "Thank you. Looks like you've returned my flock!" | todo | |
| He points, and a large food reward appears beside him (it appears on the ground, not falling from the sky, and belongs to no town) | todo | `CreateReward` is a stub; the reward itself: [../rewards.md](../rewards.md) |
| "I can't offer much, but take this with my thanks." | todo | |
| He turns to the reward and the camera swings onto it | todo | |
| When the reward is clicked, its own help line is spoken if the dialogue is free | todo | `GetHelp` stub; see [../rewards.md](../rewards.md) |
| He goes back to wandering in his field; the silver scroll is removed | todo | `ObjectDelete` stub |
| For the guide's purposes the quest now counts as done, though it carries on | todo | |

## More sheep, up to all ten

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first sheep after the reward: "Ooh. You found another sheep. I forgot there was more than five." | todo | |
| Each one after that cycles through "Good gracious. Another sheep. Superb work.", "You've found another sheep." and "Another sheep. Well I never. This gets better and better." | todo | |
| A line is only said when the flock is bigger than the biggest it has been since the reward, so a sheep that dies and is replaced earns no new line | todo | |

## Ten sheep: the sheep creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the flock is the shepherd and all ten sheep, the script waits until he is not held and puts him in his field if he is more than 50 from it | todo | |
| A widescreen scene with the fourth epic script theme; the shepherd stands by his house as the camera moves in over 4 seconds | partial | the music command works (`StartMusic`); the scene never runs |
| "All my sheep are back! Thank you!" | todo | |
| A white success sparkle shows in the field for 3 seconds and a sheep creature appears in it | todo | `SpecialEffectPosition` stub; creating a creature from a script does nothing |
| "Behold! A sheep. You may use him as your Creature." as he looks to the field and bows, then turns to the camera and prays | todo | |
| The record becomes fully done with alignment +0.6, keeping the reminder "There are still sheep missing!" | todo | |
| The camera pulls back over the field; the shepherd leaves his flock, the flock is let go, and he joins the Norse village as an ordinary villager | todo | `FlockDetach`, `ReleaseFromScript` stubs, joining a town is a stub |
| The sheep creature is then offered to swap for (see below) | todo | |

## Swapping to the sheep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sheep creature can't be picked up and keeps looking at the camera, with a silver scroll just above its head | todo | `SetIdPickupable` works; the rest is stubs; the swap: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| Whenever the camera is within 100 of it and it is on screen, the evil advisor pops out at most once a minute, points and says "Swap your Creature with this one if you want." | todo | |
| Clicking the scroll or the sheep with the player's creature more than 50 away: evil advisor: "You'll need to bring our Creature, Boss." and the scroll comes back | todo | |
| With the creature within 50, the scroll goes, the creature's leash is let go and it walks to within 15 of the sheep (given at most 5 seconds); the two creatures look each other over | todo | `DetachObjectLeash` works; `MoveGameThing`, `CreatureDoAction` stubs |
| Every 30 seconds the good advisor asks "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel." | todo | |
| Clicking the sheep confirms; clicking the player's own creature, or waiting 110 seconds, cancels and the scroll comes back | todo | |
| On confirming, a two-creature camera frames them, both sparkle, point at each other and swap with the creature-swap sound; the player's creature becomes the sheep | todo | `StartDualCamera`, `SwapCreature`, `Played` stubs |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." | todo | |
| The old creature stays there, can't be picked up, and is offered in the same way, so the player can swap back and forth as often as they like | todo | the offer never closes: the script's own time limit (60 to 120 seconds) is made but never checked |
| The offer lasts until the land's scripts are stopped (leaving for the next land) | todo | |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 2 seconds the script looks for a dead sheep within 30 of the field that is neither held nor flying; it is removed with a fade and counted | todo | `CreateTimer`, `GetTimerTimeRemaining`, `SetTimerTime`, `ObjectDelete` stubs |
| The first dead sheep: "Oh dear. One of my sheep has died. Sheep happens, I suppose." | todo | |
| The second to fourth: "Oh drat. Another sheep dead." | todo | |
| The fifth: "Too many sheep have died for me to make a living. I'm going to quit." then "You're a powerful being, Holy One, but you're not the best shepherd ever."; the record is closed fully done with alignment -0.6 and the quest ends | todo | |
| Any dead sheep counts, including ones that died elsewhere and were dropped in the field | todo | |
| If the shepherd dies (and is not in flight), the evil advisor pops out and points at him: "I never liked him anyway. He gave me the creeps."; the good advisor looks at him: "I thought nothing gave you the creeps."; evil: "You do, you big creep."; the record is closed fully done with alignment -1 | todo | `SpiritPointGameThing`, `LookGameThing`, `StopPointing` stubs |
| When the guide's storm begins, an unfinished, started flock quest is closed (fully done, alignment 0) and its script stopped | todo | see [../creature_guide.md](../creature_guide.md) |
| A flock quest whose scroll was never clicked is simply left; the sheep stay where they are | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After completion the shepherd lives on as one of the Norse village's people, and his ten sheep roam free | todo | |
| The large food reward stays where it appeared until opened | todo | |
| Nothing else in the land's later story depends on this quest beyond the guide's scroll suggestions | todo | |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every line of the shepherd is the "man" narrator voice; the advisor lines are the good and evil advisors' own voices | todo | the voices are read past; see ../../scripts/info_scripts.md |
| Clicking the active scroll again speaks its reminder through the advisor who owns it (the good advisor here) | todo | the shared reminder script; advisors are stubs |
| Music: the first generic script theme for the introduction, the fourth epic theme for the all-sheep scene; the five-sheep scene has none | partial | the music command plays these tracks (`StartMusic`, `src/Audio/GameMusic.cpp`); the scripts never reach it |
| Sounds: nine different sheep bleats for the lost sheep, and the creature-swap sound | todo | `PlaySoundEffect` stub |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The five-sheep reward only fires when the flock is exactly the shepherd and five sheep; if two sheep join at the same moment and the count jumps past it, no reward or later line ever comes and the quest can't be finished (only the storm closes it) | todo | follows from the script; undetermined how often the flocking adds two in one pass |
| The ten-sheep ending likewise needs exactly the shepherd and ten sheep; one more sheep at the same moment skips it and only the "another sheep" lines play | todo | |
| Since only ten sheep exist for the quest, one dead sheep makes the ten-sheep ending impossible unless some other sheep wanders into the flock | todo | |
| Dead sheep are counted only within 30 of the field, so sheep killed elsewhere never count | todo | |
| The developers' comments call the line "You're a powerful being... not the best shepherd ever" a much ruder one; the shipped text is the polite version | n/a | text only |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the sheep, the shepherd, his flock count and the record | todo | openblack has no saved games |
| A test script starts the quest on its own; an unshipped trade-show control script started it after the Pied Piper | n/a | not started by the game |
| The quest exists only in the first land of the story | n/a | nothing to do beyond the story itself |
