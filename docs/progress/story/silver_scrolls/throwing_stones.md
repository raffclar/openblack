# Throwing Stones

A silver scroll of the first land that teaches throwing: a boulder sits on top of a pillar down on the shore, and the
player throws rocks from an everlasting pile on the hill above until one knocks the boulder off. A fisherman watches
every throw from outside his hut. The prize is a toy ball for the creature, and afterwards the boulder keeps coming
back for practice, turning into water miracle seeds. The land as a whole is in [../land_1.md](../land_1.md); the
mini-game summary is in [../minigames.md](../minigames.md).

**Land:** 1 · **Giver:** the advisors (scroll over the hill above the pillar) · **Script:** ThrowingStones ·
**Reward:** a toy ball in a chest from the sky; then up to six water miracle seeds for practice throws ·
**Repeatable:** no (the practice afterwards is endless until six seeds)

Sources: the quest's challenge script (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's setup and control scripts, the throwing hand demo, the shared reward and did-you-know
scripts, the game's text table and the executable (how a scroll record and its alignment are applied). openblack's
state is judged on the physics work tree (`ob-wt-physics`): the land's control script stops long before it would start
this quest (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and of the 63 script functions the quest and the shared scripts it runs (notice, reward,
did-you-know, hand demo) call, 51 only log "not implemented" in `src/CHLApi.cpp` (the held-object read included). The
12 that work are the camera reads, widescreen and its transition check, distance and position reads, random numbers,
the game time, the existence check, setting a position and making rocks and mobile statics. Every row below is todo unless the notes say otherwise.

**Progress: 0/47 done, 5 partial — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's setup marks the boulder's spot with an area villagers avoid (20 across the target) | todo | `InfluencePosition` (anti-influence) is a stub |
| The land's control script starts the quest at the very beginning of the land, in the background, right after the family's "follow us" walk and the temple guide (or at once when skipping to the creature choice), and lets it go at once | todo | the land's control script stops at its first unwritten native; see [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| The boulder on the pillar is always drawn, however far away it is | todo | `ThingJcSpecial` (always visible) is a stub |
| A bronze did-you-know scroll is placed nearby at once: "Silver Reward Scrolls are not vital so you don't have to do them, if you don't want to. But there are often rewards available if you complete them." | todo | the did-you-know script; `CreateHighlight` and the text commands are stubs; see ../../interface/scrolls_and_signs.md |
| A silver challenge scroll is put up on the hill above the pillar, 15 up in the air | todo | `CreateHighlight` and `SetProperty` (its height) are stubs |
| While it is unclicked, whenever the camera is within 150 of it and it is on screen (at most every 30 seconds, and not during a film), the good advisor points at it: "Look. A Silver Reward Scroll. Let's see what it means." and the evil advisor adds "Yeah, Silver ones aren't as vital as the Gold ones, but we can still check them out." | todo | the quest's own notice script; it is passed the general "Your godly attention is required here, Leader." line and a radius of 100 but uses its own two lines and 150 |
| Clicking the scroll starts the quest; the scroll is switched to active | todo | `GameThingClicked`, `SetActive` stubs |
| If the player knocks the boulder off before ever clicking the scroll, the scroll's notice stops, there is no introduction, no fisherman and no reward: the evil advisor just points at the pillar and says "You rule at rock throwing, Boss." and the practice round starts | todo | |

## The fisherman

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On clicking, one Japanese fisherman is made at his hut by the shore and walks to a spot on the hill by the rock pile, faces the camera and plays an idle animation | todo | creating a villager from a script does nothing (`CreateScriptObject` makes only mobile statics and rocks) |
| Every second he checks what the hand holds; only a rock counts (trees were meant to, but were left out) and only while he is on screen with the camera within 100 | todo | `GetObjectHeld`, `IsOfType`, `GameThingFieldOfView` |
| He watches the rock in the hand; if it comes within 10 of him he ducks like a goalkeeper and waits a moment | todo | |
| Once thrown, he follows its flight with his eyes; the script tracks how close the rock came to the foot of the pillar | todo | `SetFocus` stub |
| If it came within 12 of the pillar he cheers ("Ooh. Now that was close."), otherwise he despairs ("Hey, I saw that. You missed the pillar by miles!"); the lines are spoken from him only if no other dialogue is running | todo | `GamePlaySaySoundEffect`, `IsDialogueReady` stubs |
| If his hut drops below three quarters of its health (before the quest is won), the evil advisor points at it: "Great shot! The house didn't stand a chance." and the good advisor: "Please watch where you're throwing things. You could have someone's eye out."; he plays an angry animation and stomps off to a friend's house, then is left to his own life | todo | `ReleaseFromScript` stub |
| Once the quest is won, if his hut is undamaged he walks back home and is left to his own life | todo | |
| If he dies, his watching stops | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film (widescreen): the camera glides over 5 seconds to a view of the rock pile; both advisors come out | partial | `SetWidescreen` works; `MoveCameraPosition`, `MoveCameraFocus`, `SpiritEject` are stubs |
| Evil: "OK. Let's get our projectile skills up to scratch." Good: "What do you suggest? I presume it involves killing…" Evil: "Not at all. I was going to propose, er, we, um… " | todo | `RunText` stub |
| The camera swings over 4 seconds to look at the boulder on its pillar; the good advisor looks at it and points: "I know, let's see if we can hit that pillar over there." | todo | `LookGameThing`, `SpiritPointPos` |
| Good: "It's a test of our godly powers." — "And nobody gets hurt." | todo | |
| The camera goes back up to the hill; five rocks of half to nine tenths of full size appear in a pile there; the evil advisor points at them: "How nice. And I guess we use this everlasting pile of stones." Good: "Hmm. Handy, that." | partial | making rocks at a size works (`CreateWithAngleAndScale` makes a rock in openblack); the film around it doesn't |
| The advisors go home, the dialogue box closes and the quest is recorded under the title "Throwing Stones" at 0% with the good advisor's reminder "See if you can hit that pillar with these handily-placed stones." | todo | `Snapshot` stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| The throwing hand demo plays: the camera settles on the hill and a recorded hand shows how to throw, pausing at each step for the good advisor: "Move your Hand over a rock." — "Press the Action Button until the rock is in your hand." — "Hold down the Action Button." — "Put momentum on the throw by moving the Hand in the correct direction." — "Throw the rock by releasing the Action Button." then "Try throwing a rock now." | todo | `PlayHandDemo`, `HandDemoTrigger`, `IsPlayingHandDemo` stubs; the demo files are in `Data/HandDemo`; see ../../hand/hand_in_scripts.md |
| After the film a second did-you-know is placed by the hill, explaining aftertouch: "You can throw in different ways. A long smooth throw sends the missile low along the ground, and a quick hand move and release will throw the object higher. Moving the Hand after you release gives you aftertouch…" | todo | throwing and aftertouch themselves are in ../../hand/throwing.md |

## The everlasting pile

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rock from the pile that is destroyed comes back at its place in the pile, at nine tenths size whatever size it was | partial | making the rock works (`CreateWithAngleAndScale`); checking that it is gone works (`ThingValid`); the script never runs |
| A rock that has come to rest (not flying, not in the hand or the creature's hand) more than 40 from its place is put back there at half size | partial | `SetPosition` works; flying/held checks (`GetProperty`, `InCreatureHand`) and resizing are stubs |
| The pile stops refilling once the quest is won, so it is "everlasting" only while the quest runs | todo | the refilling scripts end with the quest |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Throw rocks at the pillar on the shore until the boulder on top is knocked more than 2 away from its spot; the script checks every 2 seconds | partial | picking up and throwing rocks work in openblack (../../hand/throwing.md); the script's checks never run |
| Hitting the pillar while the boulder stays on top (checked 2 seconds after the hit): a film looking at the pillar; the evil advisor: "You're doing well but you have to knock the rock clean off." The record moves to 50% with the reminder "Can you knock the rock off the top? Eh?" This happens once only | todo | `GameThingHit` stub |
| There is no time limit, no limit on throws and no way to fail; damaging houses only upsets the fisherman | todo | |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the camera flies over 2 seconds to look at the shore, following the falling boulder; the good advisor: "You hit it. Marvellous." and the record moves to 50% | todo | `SetFocusFollow`, `UpdateSnapshot` stubs |
| The camera rises over 5 seconds; the quest is recorded as fully successful (no alignment change) and the evil advisor: "Neat, Boss. You're really gonna do damage with this skill later!" | todo | |
| The camera then looks at where the reward will land, the area villagers avoided is removed and an attraction 10 across is put at the reward spot | todo | `InfluencePosition` stub |
| The scroll is removed | todo | `ObjectDelete` stub |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A chest holding a toy ball for the creature falls from the sky by the pillar, without a film | todo | `CreateReward` is a stub; see [../rewards.md](../rewards.md) |
| If it is the game's first reward (likely, as this quest is open from the start of the land) the good advisor: "Wow. We've been given a chest. I wonder if there's anything in it?" and the evil advisor: "Yeah, click the Action Button on it to open it."; once opened, the reward's help line and "Now click on the contents to activate them." | todo | `GetHelp` stub; see [../rewards.md](../rewards.md) |
| Otherwise, once the chest is clicked, the reward's help line is spoken in one line if the dialogue is free | todo | |

## Practice afterwards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new boulder is put back on the pillar (through a line of the land's map script), with a small area villagers avoid around it | todo | `MapScriptFunction` (running a map script line) is a stub |
| Each time it is knocked off and comes to rest, it fades away and a water miracle seed appears where it stopped | todo | `ObjectDelete` stub; one-shot miracle seeds: ../../miracles/dispensers_and_seeds.md |
| A boulder destroyed rather than knocked off gives no seed; a new one is put up either way | todo | |
| After six seeds the boulder is no longer put back | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest plays no music of its own; the fisherman's cheer and jeer are spoken from him | todo | `GamePlaySaySoundEffect` stub |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature isn't needed; rocks in its hand count as held (they aren't put back) but the fisherman only watches rocks in the player's hand | todo | |
| The toy ball is a plaything for the creature | todo | see ../../creature/object_actions.md |

## Script quirks and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Knocking the boulder off before clicking the scroll forfeits the toy ball and the record (the quest is never recorded) | todo | |
| After the win, if the fisherman's hut is damaged but still above three quarters, he never goes home and keeps watching throws | todo | he goes home only if the hut is at full health |
| Each practice round puts up another small avoid-area at the pillar and never removes the old ones | todo | harmless |
| A counter of rocks put back at the pile is kept but never read | n/a | |
| An earlier version had three boulders to knock off (each with its own advisor line and the reminder "See if you can hit that pillar…" until all three fell); it was commented out and its lines are gone from the text table | n/a | |
| Only one fisherman is made, though the script is written to make several watchers spread around the spot | n/a | |
