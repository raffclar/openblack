# The Hermit

A first-land silver scroll: a hermit on a hillside refuses to worship a god until he sees a huge enough creature. Bring
a big creature and he converts, tells of a miracle seed under a rock and the player gets a water miracle dispenser; damage
his hut instead and he buys the player off with a water miracle, then burns the village store in revenge; kill him and
the advisors are appalled.

**Land:** 1 · **Giver:** the hermit, at his hut on a hillside away from the village · **Script:** HermitMain · **Reward:** a water miracle dispenser (impressed), or a water miracle reward from the sky (hut damaged) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on the physics work tree
(`ob-wt-physics`): of the 74 script functions the quest and the scripts it starts need, 58 only log "not implemented"
in `src/CHLApi.cpp`, among them every dialogue, advisor, camera move, property read, villager state, highlight,
snapshot and reward command; creating the hermit, the miracle seed and the dispenser does nothing (`Create` only makes
mobile statics). The land's control script stops long before this quest is started (see
[../../scripts/land1_script.md](../../scripts/land1_script.md)), so none of it happens; rows are todo unless the notes
say otherwise. The land as a whole is in [../land_1.md](../land_1.md), the script's function coverage in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/88 done, 11 partial — 6%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script starts after the creature's trainer has taught it to eat, the slap-and-stroke lesson and the introduction to the leash (or straight after the creature is chosen if the training is skipped); the Immersion Mushrooms quest starts just before it | todo | the land's control script never gets that far |
| Five seconds later a silver scroll appears over the hermit's hut and a bonfire is placed in front of it | partial | the bonfire is a mobile static, which `Create` makes; `CreateHighlight` is a stub |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll and it is on screen, the evil advisor pops out at most every 30 seconds, points at it and says "We got ourselves a task down here!" | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll makes it active and the rest begins | todo | `GameThingClicked`, `SetActive` stubs |
| Only then is a rock placed on the hill above the hut | partial | the rock is a mobile static, which `Create` makes |
| A bronze did-you-know scroll is put next to the rock: "If your Creature picks up rocks the exercise will make him stronger." | todo | `CreateHighlight`, `HighlightProperties` stubs; bronze scrolls: ../../interface/scrolls_and_signs.md |
| The hut's health is set to full | todo | `SetProperty` stub |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit (the hermit villager) appears at his hut; he can't be picked up, moved or hurt during the scene | partial | the three flags work (`SetIdPickupable`, `SetIdMoveable`, `SetIndestructable`), but creating the villager doesn't |
| A widescreen scene with the hermit's theme: he walks out of his hut, the camera following him over 7 seconds, then to his spot by the fire | partial | widescreen and music work (`SetWidescreen`, `StartMusic`); `MoveGameThing`, `SetFocusFollow` stubs |
| The camera closes in on him and slowly tilts up over 35 seconds as he talks to it, gossiping: "As I see it, there ain't no need for me to bow before you, son." | todo | `SetFocus`, `OverrideStateAnimation`, `RunText` stubs |
| Unimpressed: "I ain't seen diddly from you which impresses me one itty bit." | todo | |
| "Gods have huge Creatures. That's what my momma always told me." | todo | |
| Very close up, unimpressed: "And until I see a big enough one, you don't mean nothing to me." | todo | |
| The quest is recorded: title "The Hermit", 0 done, alignment 0, reminder "This Hermit still needs to be impressed by your Creature." (good advisor) | todo | `Snapshot` stub; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He wanders about his spot (within about 6) and the camera returns to where it was; the music stops | todo | `SetScriptState` and its settings are stubs |
| Both advisors pop out. Good: "Ah. Don't leave him as a lost soul. Show him your Creature." Evil: "Tchoh. Let him die alone. It's what he wants. The hick." Evil: "You don't have to justify yourself to him." Evil: "Come on. You've got better things to destroy." | todo | |
| Afterwards he can be picked up, moved and hurt | partial | the flags work; nothing else does |

## The hermit's wanderings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every second the script notes where he is (or that the creature is holding him) | todo | used to point the camera if he dies |
| If he is more than 75 from his spot, not flying, not held and alive, and the dialogue is free, he walks back; he tries again every 10 seconds, and starts wandering again once within 15 | todo | `CreateTimer`, `MoveGameThing`, `IsDialogueReady`, `InCreatureHand` stubs |
| This stops once he has set off to take revenge after the hut (see below) | todo | |

## Showing him the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 31 passes of its loop the script reads the creature's size and checks the hermit | todo | `GetProperty` stub; undetermined: how long a loop pass takes in game time |
| A creature larger than size 0.4, within 50 of him, while he is within 50 of his spot and not held, flying or in the creature's hand, impresses him | todo | undetermined: which in-game creature size the script's 0.4 is |
| Smaller than 0.2 and within 15: the first time, a widescreen scene with his theme: the creature's leash is let go and it is walked up to him; suspense music sting; unimpressed: "Call that a Creature? I could swat him flat!"; the creature looks sad | todo | `CallPlayerCreature`, `CreatureDoAction`, `PlaySoundEffect`, `StopSoundEffect` stubs |
| The same size again, after the creature has been more than 50 away and come back within 15: "That durned critter ain't no bigger than the last time I seed him!" and the creature shows off | todo | |
| Between 0.2 and 0.4 and within 15: the first time "Ha! Not the largest beast in the land, is he?" and the creature looks embarrassed | todo | |
| Between 0.2 and 0.4 again, after it has been away more than 50, within 30: "That durned critter ain't no bigger than the last time I seed him!" and the creature looks angry | todo | |
| Each kind of taunt is played at most twice; during them he can't be picked up, moved or hurt | partial | the flags work |
| A creature of exactly 0.2 or 0.4 gets no taunt | todo | follows from the script's comparisons |

## Impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with his theme: the hut is made whole; the creature's leash is let go and it walks to stand before him; the hermit walks to his spot | todo | |
| Suspense sting; he faces the creature, prays three times: "Yes. Now that is one worthy Creature. I'll get worshipping!" | todo | |
| The camera turns to the creature, which does its summoning gesture | todo | `CreatureDoAction` (playing an animation on a creature) is a stub |
| If the seed under the rock hasn't been found: he points at the rock: "I done seen a firefly a-heading under that there rock at break of dawn." as the camera rises to show it | todo | the scripted seed stands in for the real mechanic: fireflies hide in trees and rocks at dawn and leave a seed when lifted ([../../nature/fireflies.md](../../nature/fireflies.md)) |
| "Now being as your Creature's so mighty, I be a-wondering whether he can move that rock." as the camera closes on the rock | todo | |
| "And see what the fiery little critter's doing under there." (waits for the player) | todo | |
| The record becomes 60% done, alignment +0.4, reminder "Look under the rock where the Hermit saw the fireflies." | todo | `Snapshot` stub |
| If the seed was already found, the record goes straight to fully done, alignment 0, with that reminder | todo | the +0.4 is only added on the other path; see quirks |
| A water miracle dispenser is given beside his spot (see Reward) | todo | |
| The player gains influence round the hermit (radius 4) and round his hut (radius 20) | todo | `InfluenceObject` stub; influence: ../../worship/ |
| The camera returns, the creature is given back and the music stops | todo | |
| When the seed is found (or he dies) the record is updated to fully done with the reminder "This Hermit still needs to be impressed by your Creature." | todo | `UpdateSnapshot` stub |
| He then walks into his hut and is gone | todo | `ObjectDelete` stub |

## The seed under the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start, if the rock is in place, a watcher waits for it to be moved more than 5 from its spot, or picked up by the hand or the creature; this can happen before or after the hermit is impressed | todo | |
| A one-shot "strong" creature miracle seed then appears where the rock was, with a target effect for 10 seconds | todo | creating a seed from a script does nothing; `SpecialEffectPosition` stub; seeds: ../../miracles/dispensers_and_seeds.md |
| If the hermit is alive and within 50 of his spot: a widescreen scene; the creature (if within 30 of the rock) is sent to the bottom of the hill; good advisor: "Great! A Miracle Seed! Well, done, Mighty One." | todo | |
| The camera rises and turns to the hermit, who gossips: "Them fireflies turn theirselves into Miracle Seeds at dawn, I reckon." | todo | |
| Good advisor: "This is interesting news. We should watch out for fireflies and see where they hide at dawn." | todo | |
| Otherwise: the creature (if within 100) is sent down the hill; good advisor: "Golly. A Miracle Seed. How did that get there?"; evil: "The fireflies hide and turn themselves into Miracle Seeds at dawn. Duh!"; good: "Oh. Of course. Now that is worth knowing, Leader." | todo | |

## Damaging the hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any damage to the hut (its health below full) while he is alive and the dialogue is free sets off this branch, whoever caused it | todo | `GetProperty` stub |
| He can't be picked up until he is put down; a second later he can be picked up, moved and hurt again | partial | the flags work |
| A widescreen scene with his theme, once he has landed: the camera closes in as he despairs: "No! Not my cotton-picking hut! It's my only possession! Leave me alone!" | todo | |
| A water miracle reward falls from the sky beside him | todo | `CreateReward` stub; see Reward |
| The record becomes 99% done with alignment -0.4 and the reminder "The Hermit is in a bad mood with you, Leader." (good advisor) | todo | |
| He turns away towards his hut in despair; the music stops | todo | |
| Evil advisor: "Let's kill him anyway. Go on, Boss." Good: "But you mustn't! Please!" | todo | |
| Two seconds later he walks slowly (0.1) to his hut, despairs there and waits 10 seconds | todo | |
| He then hurries (0.5) to the Norse village store | todo | |
| When he is there, and the camera is within 100 of the store with the store on screen, a widescreen scene with his theme: the camera goes inside the store; evil advisor: "The Hermit is destroying our Village Store supplies!" while the good advisor points at the store | todo | `PosFieldOfView`, `SpiritPointGameThing` stubs |
| He looks about, faces the camera and acts the arsonist; he walks out of the store | todo | |
| Good advisor: "Well, you did flatten his hut. What do you expect?" | todo | |
| The store is set on fire (burn speed 0.3) | partial | `SetOnFire` works (the fire system); the scene never runs |
| He dances mockingly round the fire: "Who's your Daddy? Who's your Daddy now? Oh yeah. Look at that!" | todo | |
| He walks back to his hut and the camera returns | todo | |
| Once he is at his hut and neither near the camera nor on screen, he fades away and the record is fully done with alignment -0.4 | todo | |
| If he dies at any point in this branch, the death scene plays instead (see Killing him) | todo | |

## Killing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he no longer exists or is dead before being impressed or offended, and the dialogue is free, the death scene plays | todo | |
| Good advisor: "You killed him! You uncaring, horrid, mean god!" while the camera follows his body (or faces the creature if it was holding him, or looks at where he was last seen) | todo | `SetFocusFollow`, `SetPositionFollow`, `MoveCameraToFaceObject` stubs |
| Once he has landed, five seconds of his theme | partial | the music command works |
| The record is closed fully done with alignment -0.6 more than it had (so -0.6 straight away, -1.0 after damaging the hut, -0.2 after impressing him) | todo | |
| Killing him after he was impressed but before the seed is found also plays the death scene | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Impressed: a water miracle dispenser (Norse style, at an angle of 90) by his spot | todo | creating a dispenser does nothing; `SetMagicProperties`, `SetActive` stubs; dispensers: ../../miracles/dispensers_and_seeds.md |
| If it is the first dispenser of the game: the reward sting, the camera turns to it, a bronze scroll appears at it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), another goes beside it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), and the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | todo | `GetFirstHelp`, `GetLastHelp` stubs; see [../rewards.md](../rewards.md) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | todo | |
| Then the dispenser's own help lines are spoken | todo | |
| Being given the dispenser marks the land as having a water miracle, so when Nemesis's storm wrecks the village the good advisor says "We'd better use our Water Miracle to put out the flames." | todo | see [../creature_guide.md](../creature_guide.md) |
| Hut damaged: a water miracle reward falls from the sky beside him; when clicked, its help line is spoken | todo | `CreateReward`, `GetHelp` stubs; see [../rewards.md](../rewards.md) |
| The strong creature seed under the rock, whichever way the quest goes | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit is gone after any ending (into his hut, faded away, or dead) | todo | |
| After the hut branch the village store keeps burning until put out | partial | fire spreads and is put out by openblack's fire system; the scene never runs |
| The dispenser stays by the hut and keeps recharging | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the seed is found before he is impressed, impressing him gives alignment 0, not +0.4 | todo | follows from the script |
| The dispenser's recharge time is set from the reward's "every so many seconds" value only when the game clock is past zero, which it always is; the hermit passes 0 | todo | undetermined: what a recharge time of 0 does to a dispenser |
| Any damage to the hut counts as the player's, even fire spreading from elsewhere | todo | the hut check doesn't ask who did it |
| The hut branch's comments speak of the hermit giving a gesture; the shipped game gives a water miracle | n/a | comment only |
| During the hut branch his "return home" walks are switched off for good | todo | |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit speaks with the "man" narrator voice | todo | voices: ../../scripts/info_scripts.md |
| Clicking the active scroll again speaks its current reminder through the advisor who owns it | todo | |
| Music: the hermit's theme for every scene | partial | `StartMusic` plays it from its bank (`src/Audio/GameMusic.cpp`); the scripts never reach it |
| Sounds: the mushroom suspense sting when the creature first faces him (impressed or first small-creature taunt) | todo | `PlaySoundEffect`, `StopSoundEffect` stubs |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the hermit, his hut, the rock and the record | todo | openblack has no saved games |
| A test script starts the quest on its own; an unshipped control script had it switched off | n/a | not started by the game |
| The hermit villager is also used by the fifth land's script and an unshipped waving scene, outside this quest | n/a | see ../land_5.md |
