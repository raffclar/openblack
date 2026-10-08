# The Saviour

A first-land silver scroll: a freak wave has swept a fisherman's wife's husband and four other men into the sea off
the player's village. The men drown one by one over five minutes, out where the hand can't reach, so the player must
send the creature to wade out and carry them ashore. Saving all five, or killing all five, earns a strength creature
miracle dispenser; the scroll's alignment follows how many were saved, drowned or killed.

**Land:** 1 · **Giver:** a fisherman's wife on the shore by the player's village · **Script:** CreatureSavingPeople · **Reward:** a "Strengthen" creature miracle dispenser on the shore, only for saving all five with the wife alive or for killing all five · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on the physics work tree
(`ob-wt-physics`): of the 61 script functions the quest and the scripts it starts need, 47 only log "not implemented"
in `src/CHLApi.cpp`, among them every dialogue, advisor, camera move, highlight, snapshot, timer, flock and influence
command; creating villagers does nothing (`Create` only makes mobile statics) and the game-time read (`DllGettime`)
pushes nothing, so the scroll's notice loop can't run. The land's control script also stops long before this quest is
started (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), so none of it happens; rows are todo
unless the notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md), the script's function
coverage in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), dispensers in
[../rewards.md](../rewards.md).

**Progress: 0/51 done, 3 partial — 3%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest once the trainer's lesson of tying the leash to a house is done, before the guide is met (or straight after setup in a game that skips the creature training) | todo | the land's control script never gets that far |
| A note in the land's control script says it waits until the creature is big enough, but the quest itself never checks the creature's size | n/a | a stale comment; the size it reads is never used |
| A woman villager (the fisherman's wife) is made on the shore by the player's village; she can't be picked up, moved or killed for now | partial | the three flags work (`SetIdPickupable`, `SetIdMoveable`, `SetIndestructable` in `src/CHLApi.cpp`), but creating the villager doesn't |
| An anti-influence circle of radius 30 is put over the sea where the men will be, so the hand can't act there | todo | `InfluencePosition` is a stub; four of the five men lie inside it, the fourth one about 33 out, just beyond its edge (whether the hand can then reach him depends on the player's own influence; not confirmed) |
| Until the scroll is clicked, the wife faces the camera and waves for attention every 2 seconds | todo | `SetFocus`, `OverrideStateAnimation` stubs |
| A silver challenge scroll stands on the shore beside her | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and it is on screen, at most every 30 seconds the good advisor steps out, points at it: "Look. Something for you to do here." | todo | the shared notice script; `DllGettime` returns no value |
| Clicking the scroll or the shore spot starts the quest; a 5-minute timer is started | todo | `CreateTimer` stub |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The film starts; the wife is always drawn and in high detail | todo | `ThingJcSpecial`, `SetHighGraphicsDetail` stubs |
| Five Norse men are made out in the sea, each with the drowning animation looping and always drawn, however far | todo | creating villagers does nothing |
| Generic script music 3 plays | partial | `StartMusic` plays the track (`src/Audio/GameMusic.cpp`); the scene never runs |
| The evil advisor appears: "What's all the fuss?" as the camera glides (3 s) to look from the shore over the sea | todo | `SpiritAppear` stub |
| The evil advisor disappears; the camera turns further out to sea while the wife looks at the first man and despairs | todo | |
| The wife faces the camera and despairs again: "A freak wave has just struck! My husband and four others are drowning! Please help them!" | todo | the woman narrator |
| She walks slowly (half speed) to a spot further along the shore | todo | `MoveGameThing`, `SetProperty` (speed) stubs |
| Two seconds later the scroll is entered in the challenge list at 0% with the reminder "My gosh! There are still people to save!" | todo | `Snapshot` stub |
| The camera comes in low over the beach; the evil advisor: "Er, let's see. Who do we know who's tall enough to wade out to them?" | todo | |
| The good advisor steps out; once the wife has reached her spot she faces the sea and despairs in a loop: good advisor: "Don't muck around. Let's get our Creature out there as fast as possible." | todo | |
| The wife goes back to normal detail and can now be picked up, moved and killed; the music stops | partial | the flags work; music stop works |
| The 5-minute timer is set again from full as the film ends | todo | `SetTimerTime` stub |

## Saving the men

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The men drown one after another, the last-placed first: at about 79, 119, 199, 249 and 299 seconds into the five minutes | todo | each man has his own drowning time (1, 51, 101, 181 and 221 seconds before the timer runs out) |
| A man drowns while he is still within 5 of where he started (and alive), or is anywhere in the drowning state, when his time comes: he plays the drowned death and is removed | todo | `GetProperty` (health), the drowning state check, `ObjectDelete` stubs |
| A man counts as out of the water once he is more than 5 from his start, not drowning, and neither in the hand, flying through the air, nor in the creature's hand | todo | `InCreatureHand` stub |
| A man brought out alive joins the village and is handed back to the game | todo | joins town 0 of the land (taken to be the player's village; not confirmed); `FlockAttach`, `ReleaseFromScript` stubs |
| If a saved man is killed before the quest ends, the evil advisor says "I like it, Boss. One less human to deal with." | todo | |
| A man who leaves the water dead, or vanishes, without having drowned (dropped, thrown, eaten) is killed by the player: the evil advisor says "I like it, Boss. One less human to deal with." | todo | |
| The quest ends when no man is left in the water, or when the timer runs out | todo | |
| The hand can't reach the men for the anti-influence, so the creature, tall enough to wade out, is the way to save them (the advisors say so) | todo | creature carrying people: see ../../creature/ |

## The ending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second after the end the count is made: saved men, drowned men, and the rest as murdered | todo | |
| The scroll's alignment is (saved − murdered) × 0.2 − drowned × 0.1, e.g. +1 for all saved, −0.5 for all drowned, −1 for all killed; the scroll goes to 80% with that alignment | todo | `UpdateSnapshot` stub |
| If the last man to leave the water drowned, the scroll waits to be clicked again, with the good advisor's notice "Your godly attention is required here, Leader." | todo | |
| The closing film: if the wife has been killed, a man (her brother) is made on the shore with a woman witness beside him, and he takes her place | todo | |
| The camera turns to face the wife (or brother) from 10 away; they and the witness face it | todo | `MoveCameraToFaceObject` stub |
| If the wife was killed, her brother first: "My sister asked for your help and you killed her!" despairing | todo | the man narrator |
| All five saved and the wife alive: "Oh, I'm so grateful. We'll get some extra worshipping done." then the evil advisor: "Huh. Yeah, you're grateful now." | todo | |
| All five saved but the wife killed: the witness despairs: "At least some survived. We're a little thankful for that." | todo | |
| Some saved: "At least some survived. We're a little thankful for that."; and if any were killed, the witness (if there is one) shakes her fist: "You killed those that didn't drown. I'm shocked beyond words. Almost." then despairs: "We'll remember that. Such acts do not go un-noticed." | todo | |
| None saved, some killed: "You killed those that didn't drown. I'm shocked beyond words. Almost." / "We'll remember that. Such acts do not go un-noticed.", then both advisors come out: evil "I feel gutted." / good "Give it a rest." | todo | |
| None saved, none killed (all drowned): "At times like this I wish we had a choice of gods to worship." then the advisors' "I feel gutted." / "Give it a rest." | todo | |
| All five killed: "I saw that! It was murder!" then the advisors' "I feel gutted." / "Give it a rest." | todo | |
| The scroll is completed (100%) with the same alignment, and removed | todo | |
| The wife (or her brother) joins the village and is handed back to the game; the anti-influence over the sea is removed | todo | |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only all five saved with the wife alive, or all five killed, earn the reward; any mixed result, or all saved with the wife dead, earns nothing | todo | |
| A "Strengthen" creature miracle dispenser is built on the shore by the scroll, turned half round, recharging every 5 minutes | todo | `CreateScriptObject` makes no dispensers; `SetMagicProperties`, `SetTimerTime` stubs; dispensers: [../rewards.md](../rewards.md) |
| A film with the reward sting: the camera glides to the dispenser; the evil advisor steps out | todo | `PlaySoundEffect` stub |
| If it is the first dispenser given by a film in the game: a did-you-know scroll ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") is placed by it, a signpost appears next to it telling "A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."; the evil advisor points: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." pointing at the signpost | todo | the shared dispenser script |
| Otherwise the evil advisor points: "Nice. Another of those cool Miracle Dispensers." | todo | |
| Then the dispenser's own help lines for its miracle are spoken by whichever advisors own them | todo | which lines the game hands out for the strength miracle was not traced |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved man who is killed before the quest ends still counts as saved in the tally | todo | a script quirk |
| The fist-shaking and despairing witness only exists when the wife was killed, so in other endings those animations don't play | todo | a script quirk |
| A man still held by the creature when the five minutes run out isn't counted as saved, so he counts as murdered | todo | a script quirk (the count is made one second after the end) |
| A wife's line "You saved some people, but others still drowned. Call yourself a god? I'm going home." and the evil advisor's "Gutted! Ha! Fishermen. Gutted. Get it?" are never used | n/a | in the text table, used by no script |
| A saved game keeps the men, the wife, the timer and the scroll | todo | openblack has no saved games |
