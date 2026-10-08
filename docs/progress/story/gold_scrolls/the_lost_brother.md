# The Lost Brother

The second step of finding the creature-gate stones on the first land: a Norse woman's sick brother has wandered off
from his sick-bed in a fever; if the player finds him and brings him back to her she hands over the ape gate stone kept
in her house. The player can instead heal him, smash her house to get the stone, or kill either of them, and each way
ends the story differently with its own alignment.

**Land:** 1 · **Giver:** the brother's sister, a Norse housewife, at her house, on the edge of the Norse village · **Script:** LostBrother · **Reward:** the ape gate stone (one of the three that open the creatures' gates), set on her doorstep · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's map script (`Land1.txt`) and the game's text table. openblack is judged on the physics
work tree (`ob-wt-physics`): the dialogue, advisor, scroll, record, villager-making, camera move, timer, effect and
property commands the quest needs only log "not implemented" in `src/CHLApi.cpp`; the commands that do work are the
pick-up, moveable, indestructible and fire flags, the camera cuts, fades, widescreen, music and game time. The land's
control script also stops long before this quest is reached (see [../../scripts/land1_script.md](../../scripts/land1_script.md)),
so none of it happens; rows are todo unless the notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md);
the quest that starts it is [Choose Your Creature](choose_your_creature.md).

**Progress: 1/68 done, 7 partial — 7%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold (story) scroll, titled "The Lost Brother" | todo | `CreateHighlight` is a stub |
| It is started by the creature trainer at the creature gates, once the first gate stone (the tiger stone the villagers danced round when the player arrived) is on the plinth: "Excellent. You've found a Gate Stone. You'll need to search for another, now." then "Try looking for a Gold Story Scroll in the Village." and the camera rises to look down towards the village | todo | see [choose_your_creature.md](choose_your_creature.md); the plinth's stone count (`ObjectInfoBits`) is a stub |
| Its stone is the second of the three: the plinth knows the ape stone (this one), the tiger stone and the carved stone apart; with the ape and tiger stones both on it, the trainer sends the player on to [The Sculptor](the_sculptor.md) | todo | see [choose_your_creature.md](choose_your_creature.md) |
| The ape stone stands inside the woman's house from the start of the land (placed by the land's map) | done | openblack loads the land's mobile statics (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| From the end of the opening (the family leading the player home) the ape stone can't be picked up or moved; the quest keeps it locked until the woman gives it away or the quest ends | partial | the pick-up and moveable flags work (`SetIdPickupable`, `SetIdMoveable` in `src/CHLApi.cpp`), but nothing runs them, so in openblack the stone can be taken at any time |
| A separate watcher keeps the ape stone in play until it is on the plinth: lost (or left outside the player's influence) it is made again on the woman's doorstep; left elsewhere for a minute after being dropped it is put back on the doorstep; once more than 100 from the doorstep it sparkles | todo | the gate stone watchers belong to [choose_your_creature.md](choose_your_creature.md) |
| A new game that skips straight to choosing the creature (or keeps the old creature, patch 1.1) deletes the three gate stones and never starts this quest | todo | the skip questions are stubs; see [choose_your_creature.md](choose_your_creature.md) |
| The quest can't leave the story stuck: every ending either hands over the stone or unlocks it where it stands in the house (see "After it ends"), and the watcher remakes it if it is lost | todo | |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears over the woman's house, 6 up | todo | `CreateHighlight`, `SetProperty` (altitude) stubs |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor pops out at most every 30 seconds, points at it and says "Your godly attention is required here, Leader." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` stubs |
| Clicking the scroll or the house makes the scroll active and the introduction begins; nothing else of the quest happens until then | todo | `GameThingClicked`, `SetActive` stubs |
| The sister (a new Norse housewife) is made at her house and the brother (a Norse farmer) at his sick-bed in some trees through a pass beyond the village, with 0.4 health, lying in a dying loop | todo | `Create` makes no villagers; `SetProperty` (health) stub |
| Each of them gets a small patch of the player's influence (radius 5) that moves with them, so they can be picked up wherever they are | todo | `InfluenceObject` stub |
| A sparkle marks the brother for as long as he is lost | todo | `SpecialEffectObject` stub |
| A drizzle is made over the brother: 15 degrees, steady rain, fully overcast, eight clouds of shade 0.8 at height 130, no lightning, reaching 600 to 1000 around him, not blown by the wind, lasting two minutes and fading over 40 seconds | todo | creating a weather object and `ChangeWeatherProperties`, `ChangeCloudProperties`, `ChangeTimeFadeProperties`, `ChangeInnerOuterProperties` are stubs |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen cut-scene with the sad generic script theme; both villagers are drawn in high detail and the sister can't be hurt until it ends | partial | widescreen, music and the indestructible flag work (`SetWidescreen`, `StartMusic`, `SetIndestructable`); high detail (`SetHighGraphicsDetail`) is a stub and the scene never runs |
| The sister walks out in despair, very slowly (speed 0.12), to a spot in front of her house; the camera follows her and closes in over 10 seconds | todo | `SetProperty` (speed), `MoveGameThing`, `OverrideStateAnimation`, `FocusFollow`, `MoveCameraPosition` stubs |
| There she plays her scripted kneel and says "O Holy One, I kneel before you." | todo | `RunText` stub |
| The quest is recorded: "The Lost Brother", 0 done, alignment 0, reminder (good advisor) "The Lost Brother is by some trees. Reunite him with his sister." | todo | `Snapshot` is a stub; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| Turning towards the sick-bed and pointing: "My brother, suffering a fever, has left his sick-bed and become lost." while the camera moves behind her; she stands in despair and the screen fades to black | partial | the fade works (`SetFade`); the rest doesn't |
| The camera runs along a set path over to the brother lying in the trees; he writhes as if poisoned and groans: "He is so weak I'm frightened he'll die." (the sister's voice over the shot) | todo | `RunCameraPath`, `PlaySoundEffect` stubs |
| The camera drops in close to him, he groans again and the screen fades | todo | |
| Back at the weeping sister: "If you find him and bring him home, I'll give you one of the three Gate Stones. It's in my house." She mourns while the camera turns to the stormy mountain above | todo | |
| The game time is put back to what it was when the scene started (an earlier draft jumped the time of day for the brother's shot) | partial | game time works (`GetGameTime`, `SetGameTime`) |
| The sister then wanders around in front of her house | todo | `SetScriptState` and its parameters are stubs |
| The music stops; both advisors come out. The evil advisor points at the house: "I gotta plan. Why don't we trash the house? We can get the Gate Stone that way." Good: "I heartily object! That's just malicious." Evil: "That's kind of the whole point." | todo | `SpiritEject`, `SpiritPointPos`, `RunText` stubs |
| If the player hardly used the zoom while following the family in the opening, the good advisor adds "Leader, you haven't used your zoom ability much.", "You'll need it to find the lost brother." and "Here's a quick reminder." and the zoom hand demonstration plays | todo | the zoom tracking is in the opening ([../tutorial.md](../tutorial.md)); the hand demo command is a stub |

## Finding the brother

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until the brother is first picked up: with the camera within 150 of him, him on screen and the sister not within 30 of him, the evil advisor points at him, "Hey! There's the sick guy!", at most every 30 seconds | todo | |
| Otherwise, with the camera within 200 of the sister, a spot by the pass on screen and the brother still at his sick-bed, the good advisor points at him: "The Lost Brother is in some trees through the pass." at most every 30 seconds | todo | |
| The player carries him in the hand; while he is held within 2 of his living sister (she on land), she turns to him and jumps at the hand | todo | the hand can carry villagers in openblack, but nothing makes the siblings |
| Once his health drops below where it started (hurt by a drop or a throw) while he is not held, the evil advisor says once "No pain, no gain. Let's kill him." | todo | |
| While he is hurt and alive he groans every so often, a random one of five groans | todo | `PlaySoundEffect` stub |
| Nothing else harms him: he never dies of his fever (his 60-second countdown and slow decline were cut) | todo | |

## Ending: reunited

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the brother is within 10 of his sister anywhere, both alive, on land, neither held nor flying, and no other dialogue is running, the reunion plays (once); his sparkle goes | todo | |
| Widescreen, the sad theme, both in high detail; the camera looks down on the brother as the sister runs to him (speed 0.4), and prays three times | partial | music and widescreen work; the rest doesn't |
| A fade; "There you are. I've been so worried!" while, close up, she helps him up | todo | |
| Another fade; close in as they hug, then she gossips to the camera and he looks puzzled | todo | |
| If she still has the stone: "Thank you for your benevolence. Here is the Gate Stone." and the stone is handed over (below), recorded as fully done with alignment +0.9 | todo | |
| If the player had already smashed the house for it, the record closes fully done with alignment -0.3 instead | todo | |
| His health rises by 0.1; both walk home slowly (speed 0.3) and each rejoins the Norse village when home, held or dead | todo | `ReleaseFromScript`, `AttachToGame` stubs |

## Ending: healed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the brother's health rises above where it started (for instance a heal miracle), he sets off home and his sparkle goes | todo | |
| A cut-scene: the record goes to 70% with alignment +0.5; both advisors: good "A life saved and a worshipper on our side forever. You've made me happy, Leader."; evil "No way. You gave in there, my friend. I detest weakness. And niceness." | todo | |
| If she still has the stone, the sister sets off home too and the stone is handed over, fully done with alignment +0.8; the quest ends there, without the reunion scene | todo | |
| If the stone was already taken, the quest goes on and a later reunion closes it (alignment -0.3) | todo | |

## Ending: smashing the house

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once the house is below three quarters of its health while she still has the stone, the break-in scene plays (once) | todo | the house's health (`GetProperty`) is a stub |
| If the sister is alive she stops, faces the camera and waves for attention: "My house! Here, take the Gate Stone!", hands it over (recorded 50% done, alignment -0.3), then "You've got the Gate Stone. Now please find my brother!" and the quest goes on | todo | |
| If she is dead, the stone is handed over at once (fully done, alignment -0.7) and the evil advisor: "All right. Let's take the Gate Stone back to the pedestal." | todo | |

## Ending: deaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the brother dies (from the hand, a throw, a miracle or the creature), the camera follows his body until it lands and closes in; his sparkle goes | todo | `PositionFollow`, `FocusFollow` stubs |
| The record goes to 50% with alignment -0.4; its reminder is the good advisor's "You killed the brother, but his sister doesn't know." if she is alive, else the evil advisor's "Way I see it, we need that Gate Stone. Wreck the house and grab it!" | todo | |
| The evil advisor then points: at the body, "The sick dude's dead. Let's show the body to his sister. Ha!" if she is alive; at the house with "Way I see it, we need that Gate Stone. Wreck the house and grab it!" if she is dead and still has the stone; otherwise the record closes fully done | todo | |
| If his body vanishes while she still has the stone, the record goes to 50% (alignment -0.3) and the evil advisor points at the house: "Now let's smash up the house with a rock to get that Gate Stone." | todo | |
| Five minutes after his death, if the stone has been taken, the record closes fully done (alignment -0.4) | todo | `CreateTimer` stub |
| Laying his body within 10 of his living sister (on land, neither held) plays a scene with the sad theme: she walks to it, falls into mourning, "By the gods! He's dead! Oh my poor brother!"; if she still has the stone, "You are so cruel. Take the Gate Stone. Have it. But leave me in peace." and it is handed over (fully done, alignment -0.5); otherwise the hand-over scene plays again anyway (alignment -0.8). Once out of view she rejoins the village | todo | |
| If the sister dies, the camera follows her body and the record goes to 50% (alignment -0.4, reminder "Way I see it, we need that Gate Stone. Wreck the house and grab it!"); evil: "Wow. A world of pain. I like it."; if she still had the stone he points at the house, "Now let's smash up the house with a rock to get that Gate Stone.", otherwise the record closes fully done; good: "Aah. How appalling. Life is sacred you know." | todo | |
| Bringing the living brother within 10 of his sister's body: "My Sister? Dead? How?", then "Ahhhh. Uh." and he dies of grief; recorded 50% (alignment -1) with the evil advisor pointing at the house, "We've finished off both of them. Now let's smash up her house to get the Gate Stone!", or fully done (alignment -1) if the stone was already taken | todo | |
| With the sister dead, the brother, unless healed, dies two minutes later wherever he is; recorded 75% (alignment -0.5) if the stone is still in the house, fully done otherwise, and the quest ends | todo | |
| Brought home hurt (within 10 of the house, not held or flying) with the sister dead, the good advisor: "You've saved the brother but there's no-one to cure him." (recorded 50%, alignment -0.3); 90 seconds later the record closes fully done with alignment -0.8 and the quest ends | todo | |
| Set down right at the house (within 1) with the sister dead and him alive, the quest simply ends | todo | |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Handing over the stone is a cut-scene: the stone is unlocked (can be picked up and moved) and set on the doorstep in front of the house | partial | `SetIdPickupable`, `SetIdMoveable` and `SetPosition` work; nothing runs them |
| A lasting sparkle marks the stone, and a ten-second "success" burst plays on it | todo | `SpecialEffectObject`, `SpecialEffectPosition` stubs |
| If the camera is within 100 of the stone it glides over 10 seconds to a view past the stone towards the gates; otherwise the screen fades to black for 1.5 seconds, cuts to beside the stone, fades in over 3 seconds and makes the same glide | partial | the cuts and fades work (`SetCameraPosition`, `SetCameraFocus`, `SetFade`, `SetFadeIn`); the glide (`MoveCameraPosition`) doesn't |
| The scroll is removed and the record is set with that ending's success and alignment, keeping the reminder "The Lost Brother is by some trees. Reunite him with his sister." | todo | `ObjectDelete`, `Snapshot` stubs |
| Each record update moves the player's alignment by its alignment; the best ending (+0.9) is the reunion with the stone still in the house, the worst (-1) the brother dying of grief | todo | see [../challenges_and_rewards.md](../challenges_and_rewards.md) and ../../worship/alignment.md |
| The player takes the stone to the plinth by the creature gates; that is [Choose Your Creature](choose_your_creature.md)'s part | todo | |

## After it ends

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five seconds after the quest ends, if the stone still exists it is unlocked; while it stays within 5 of the house the evil advisor points at the house every 30 seconds: "It doesn't look like that woman is going to give us the Gate Key." | todo | |
| The brother, if he exists, is faded out of the world, even after the happy endings where he was walking home (the check meant to spare him is always true) | todo | script bug; `ObjectDelete` stub |
| The scroll is cleared once the stone is picked up or gone; with the two-minute death ending, the record is then set fully done (alignment -0.5) | todo | |
| The siblings, if still around, rejoin the Norse village as ordinary villagers | todo | |

## Unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut ending where both siblings arrive home together: "We offer thanks, O mighty spirit. Here. The Gate Stone." and the good advisor: "A Gate Stone! Let's give it to the woman at the Gate!" | n/a | commented out of the script; the lines are in the text table |
| A cut scene for both siblings killed: "The siblings are dead. You gotta trash the house and get the Gate Stone now, Boss." | n/a | commented out |
| A cut 60-second countdown and slow health loss, ending with the brother dying of his fever: "The lost one must be at death's door. This is dreadful!" / "Maybe we've got better things to do. You thought of that?" | n/a | commented out |
| A cut scene for flying the brother to his sister | n/a | only a commented-out call; no such script |
| An older copy of the quest run by an unshipped demo control script | n/a | not in the game's program |
