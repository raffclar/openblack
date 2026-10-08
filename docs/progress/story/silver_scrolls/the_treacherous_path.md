# The Treacherous Path

A silver scroll on the fourth land. A woman (the "blind woman" of the scripts) must carry a healing potion along a
dangerous mountain path to her dying brother in another village. Wolves, a forest fire and a boulder-throwing ogre named
Morg lie on the way; she has three potions to bring herself back, but each one she uses is one fewer for her brother.
If she arrives with a potion left, the brother is healed and gives the player a wolf creature to swap for.

**Land:** 4 · **Giver:** the woman at her hut in the Aztec village · **Script:** BlindWoman · **Reward:** a wolf creature to swap for, offered for as long as the land lasts (only if the brother is healed) · **Repeatable:** no

The land as a whole is in [../land_4.md](../land_4.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md); the woman is also listed in
[../../villager/special_villagers.md](../../villager/special_villagers.md).

Sources: the land's challenge script source (checked against the PC game's compiled `challenge.chl`), the game's text
table (`Scripts/InfoScript2.txt`) and the executable. openblack is judged on the physics work tree (`ob-wt-physics`):
of the 81 script functions the quest and the scripts it starts call, 57 only log "not implemented" in
`src/CHLApi.cpp`, and a script cannot make a villager, a flock, a tree or a creature (`CreateScriptObject` only makes
mobile statics and rocks), so neither the woman nor Morg ever exists and every row below is todo unless the notes say
otherwise.

**Progress: 0/60 done, 8 partial — 7%**

## Morg, the boulder-throwing ogre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| As soon as the land's control script starts the quest (in the background, at the land's start), an ogre creature is made on a ridge above the path, modelled on the player's creature at 0.9 of its strength and size, then shrunk to half scale, named "Morg" and made fully evil | todo | `CreatureCreateRelativeToCreature`, `SetProperty`, `SetCreatureName` are stubs; the land's control script stops long before (see [../../scripts/land4_script.md](../../scripts/land4_script.md)) |
| Morg works from the land's start, long before the scroll appears: every 10 seconds he throws a small boulder (a rock at 0.3 scale made beside him) at his target | partial | making a rock works (`CreateWithAngleAndScale`), but Morg does not exist and the throw (`CreatureDoAction`) is a stub |
| His target: the player's creature if it is within 75 of him; otherwise the woman, once her quest has begun and she was last seen within 50 of a grassy patch on the path; otherwise a sheep on that patch | todo | |
| A boulder that does not land within 15 of him is deleted instead of thrown; thrown boulders stay 20 seconds once they come to rest, then shrink away over about 20 seconds and go | todo | |
| If he wanders more than 30 from his ridge he walks back (up to 25 seconds) | todo | |
| Whenever the camera is within 75 of him he mutters, at most once a minute: "Sheeps keep me awake with baaings at night!" | todo | `GetTimerTimeRemaining`, `SetTimerTime` are stubs |
| If the player's creature comes within 50 of him, he grows (in steps of 0.1) to the creature's size, both creatures are made to stare at each other, and Morg starts a fight with the creature | todo | `CreatureDoAction`, `IsFighting` are stubs |
| The script waits up to 45 seconds for the fight to start, then until it ends | todo | |
| If Morg ends the fight with less fighting health than the creature, he is driven out: five sparkles burst where he stood, he vanishes, and an Aztec farmer is made in his place, who is given no health and so dies at once; Morg never returns | todo | |
| If Morg wins (or the fight never starts), he walks back to his ridge, heals 1% every 6 seconds between fights (full health in 10 minutes), and 20 seconds later shrinks back down to half scale | todo | the shared fight-healing script |
| The player's creature is released back to the player after either result | todo | `ReleaseFromScript` is a stub |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll appears only once the player owns the Aztec village of this land | todo | `GetProperty` (player) is a stub |
| A silver scroll appears above the woman's hut in the Aztec village, raised 10 off the ground | todo | `CreateHighlight` is a stub |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out every 30 seconds or more, points at it and says "Look. Something for you to do here." | todo | the shared notify script; advisors are stubs |
| The woman does not exist until the scroll is clicked | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll makes the woman (an Aztec housewife) at her hut; she cannot be picked up | partial | the pick-up flag works (`SetIdPickupable`); making her does not |
| A pack of eight wolves is made on the path (gathering within 5, roaming up to 15), and a flock of eight sheep on the grassy patch below Morg | todo | `FlockCreate`, `ChangeInnerOuterProperties`, `PopulateContainer` are stubs |
| A cinematic (letterbox) with generic script music: the woman, in high detail, walks to a spot by her hut while the camera glides to look at her over three seconds; she faces the camera | partial | the letterbox and the music work (`SetWidescreen`, `StartMusic`); the rest are stubs |
| The scroll is entered in the challenge log as "The Treacherous Path", not yet done, with the good advisor's reminder "Help the sister reach her brother." | todo | `Snapshot` is a stub |
| She plays a gossiping animation. Woman: "Holy One, I must take this healing potion to my brother in the Village." | todo | |
| The camera moves to the start of a recorded camera path and flies along the route she must take | todo | `ConvertCameraPosition`, `RunCameraPath` are stubs |
| Woman: "But I've got to travel along this treacherous path." | todo | |
| Woman: "Please watch over me. I'd be so grateful." | todo | the source notes the spoken line was recorded without "me"; the text has it |
| The screen fades to black and back in on the woman; the music stops and the cinematic ends | partial | fades, camera cuts and stopping the music work; the scene is never reached |

## Her journey

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| She cannot be hurt while she walks round her fence and to the start of the path; then she walks a recorded path all the way to her brother's village and can be hurt from then on | partial | the indestructible flag works (`SetIndestructable`); `WalkPath` is a stub |
| Every 2 seconds the quest checks on her | todo | |
| Near the first point of the path: good advisor, pointing at her, "Argh! The woman's heading straight for that pack of wolves!" and "We must save her, Leader." | todo | the wolves are ordinary wild wolves; the script itself never sets them on her (undetermined how readily wolves attack people) |
| Near the second point: three birch trees in a wood beside the path are set alight (each made again first if it was gone or moved more than 5 from its place); evil advisor, pointing at her, "Now this is neat. The healing sister's walking into a forest fire." and "Get over there, Boss. This I gotta see." | partial | setting a thing alight works (`SetOnFire`); making a tree does not |
| Near the third point: the good advisor points at Morg, "There's a bizarre creature causing mayhem over there.", then at her, "Our sister with the healing potions is heading for him. We must act!" | todo | |
| When she reaches the edge of her brother's village she walks into his house and the journey is over | todo | |

## Potions: when she is hurt

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If her health falls to 5% or less (her health is then set to nothing), or she stops existing, she has "died" | todo | `GetProperty` (health) is a stub |
| She has three potions to bring herself back. After each of the first three deaths, two seconds later a cinematic fades to black: any body is faded away and she is made again by her hut, gossiping, with the camera on her | todo | `ObjectDelete` (with fade) is a stub |
| First death: "Holy One, I've had to use one of my healing potions to restore my own health.", "I only have two left, and I need one of those to cure my brother.", "Please watch over me on my journey." | todo | |
| Second death: "I've had to use another potion.", "I have one left, and my brother needs that.", "Please try to ensure I come to no harm." | todo | |
| Third death: "Oh no! I've got no potions left. I'm now going to see my brother on his death bed!", "If I get there at all, that is." | todo | |
| The screen fades back to where the camera was, and she sets off on the whole journey again from her hut | todo | |
| A fourth death ends the quest in failure | todo | |

## Endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Arriving with a potion left (fewer than three deaths): a cinematic with epic script music fades to the brother's house; the brother (an Aztec farmer) limps out injured, very slowly, and stands in despair as she comes up to him | partial | the music works (`StartMusic`); the rest are stubs |
| Woman: "Thank you for looking after me during my journey." and "I can now heal my brother." | todo | |
| She points at him and casts a first-level heal miracle on him from where she stands (for 3 seconds); his health is set to full | partial | casting a miracle from a script works (`SpellAtPos`), but there is no woman or brother |
| The scroll is marked done in the challenge log (success 1, alignment unchanged) | todo | `Snapshot` is a stub |
| He shrugs, she runs to him and they hug; then both face the camera | todo | |
| Brother: "Thanks for helping my sister. We promise to worship you forever." | todo | |
| A wolf creature is made beside the house, made to stand idle and face the camera. Brother: "Please accept this special Wolf as a token of our gratitude." | todo | making a creature from a script does nothing |
| Good advisor, pointing at the wolf: "Ah, they shouldn't have!"; evil advisor: "The Wolf is neat but I'd have preferred it if we'd killed someone." | todo | |
| Afterwards the two walk into the house, and once the brother is off screen both join the Aztec village of this land as ordinary villagers; the woman can now be picked up | todo | `FlockAttach` (attach to a town) is a stub |
| Arriving with no potion (after three deaths): the brother crawls out and dies; she mourns. The scroll is still marked done | todo | |
| Woman: "No! My brother's passed away!" and "I got here, but I have no more heal potions." | todo | |
| Evil advisor, pointing at the brother: "Oh this is too neat. Feel the suffering!" and "Let's make it a job well done. Kill the woman."; good advisor: "No! Her life's already ruined. Leave her!" and "You've done enough harm already. Oh unhappy day." | todo | |
| She walks away in despair into the house and joins the village; the brother's body is removed once off screen | todo | |
| Dying a fourth time: the camera moves (or fades) to her body. Evil advisor, pointing: "Oh yeah! The do-gooding healer woman is pushing up the daisies." and "Her brother's finished." | todo | |
| The screen fades to the brother's house; he crawls out and dies there. The scroll is still marked done | todo | |
| Evil advisor, pointing at him: "I told you so! Look! Ha ha!"; good advisor: "This is not funny. Cruelty is so unbecoming." The body is removed once off screen | todo | |

## The reward: swapping to the wolf

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wolf is offered with the same swap as every story creature: a silver scroll over its head, the evil advisor's "Swap your Creature with this one if you want.", the good advisor's confirmation, a two-creature camera for the swap, and the old creature left in its place to swap back for ever | todo | the shared swap script, see [creature_swaps.md](creature_swaps.md); described in full in [the_fish_puzzle.md](the_fish_puzzle.md#the-reward-swapping-to-the-turtle); `SwapCreature` is a stub |

## Quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every ending marks the scroll as done, even her death or her brother's; only the wolf depends on her arriving with a potion | todo | |
| The scroll itself is never removed by the quest | todo | undetermined whether marking it done removes it |
| At the third point the good advisor is brought out but the script stops and sends home the evil one, so the good advisor stays pointing until the dialogue ends | todo | |
| The man left when Morg is driven out is made with no health and dies on the spot | todo | |
| After a fourth death by being removed entirely (not just killed), the closing camera is aimed at a woman who no longer exists | todo | |
| Morg throws at sheep for the whole land, from its start, whether or not the quest is ever taken | todo | |

## Unused and cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Morg was once to cast enlarge and strength miracles on himself before fighting; the lines are commented out | n/a | cut; the growth is done by scaling instead |
| A fourth tree by the path is looked up but never set alight; a brother's reward spot and reward object are declared but never used; an earlier wolf-pack spot is commented out | n/a | cut |
