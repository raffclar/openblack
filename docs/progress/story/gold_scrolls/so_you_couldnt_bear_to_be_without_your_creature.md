# So You Couldn't Bear to Be Without Your Creature?

The third land's main gold quest, "free the creature": the player follows Lethys through the vortex and finds their
creature frozen in a prison of three stone pillars, each one fed by the prayers of a village. Winning a village sinks
its pillar; losing it raises the pillar again. When all three are down the creature is freed; later, once Lethys's last
town is in trouble, Lethys begs for his life, hands over the creed from Khazar's creature and opens the vortex to the
next land. The quest has no title of its own: its challenge-log entry is headed by Lethys's line "So you couldn't bear
to be without your Creature?", which is never actually spoken.

**Land:** 3 · **Giver:** the advisors, on arrival (no scroll; logged straight away) · **Script:** GetThroughVortexL2, FreeTheCreature · **Reward:** the creature back (leash and temple restored), the creed from Khazar's creature, and the exit vortex · **Repeatable:** no

The land as a whole is in [../land_3.md](../land_3.md), the land's map script in
[../../scripts/land3_script.md](../../scripts/land3_script.md), the vortex mechanics in [../portals.md](../portals.md).
The two attacks started part-way through have their own files: [the_wolves_are_possessed.md](the_wolves_are_possessed.md)
and [fire_fire_im_on_fire.md](fire_fire_im_on_fire.md); the exit is
[leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md). How the creature was taken at the end of Land 2:
[lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`), the
game's text table and the executable. openblack's state is judged on the physics work tree (`ob-wt-physics`): the
land-loading command does nothing and the story's top script always begins with Land 1's control script, so Land 3's
control script and everything below never runs. Of the 97 script commands this quest and the scripts it starts need, 28
do something in `src/CHLApi.cpp` (camera set, fades, music, fire, the climate switch, making mobile statics); creating
villagers, animals, highlights and flocks, dialogue, snapshots, creature actions and the computer-player moves are
stubs. Rows are todo unless the notes say otherwise.

**Progress: 0/97 done, 4 partial — 2%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script first sets the land up, then starts the man who wants to be thrown and this quest's prison in the background, then runs the arrival and waits for it to end before starting the monk, the rejuvenator and the tree puzzle | todo | Land 3's control script never runs (see [../../scripts/land3_script.md](../../scripts/land3_script.md)) |
| The land ends when the exit vortex's scroll is clicked; the control script then returns and every other script of the land is stopped, so anything still waiting (such as Lethys's own end) is cut off | todo | `StopAllScriptsExcluding` works, but the land never loads |
| The quest is logged as a gold (story) entry, not a silver challenge | todo | `Snapshot`, `UpdateSnapshot` are stubs |
| The log's title is Lethys's line "So you couldn't bear to be without your Creature?"; that line is not spoken anywhere in the shipped scripts | todo | |
| A commented-out log entry with a proper title and reminder (title text 22, reminder 21) was replaced by this; neither text exists in the text table | n/a | cut before release |

## Setting up the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's influence is virtual for this land and the player is set to have no alliance with Lethys | todo | `SetPlayerAlly` and the virtual-influence switch are stubs |
| The player's creature is loaded at the prison on the hill (it was left with Lethys at the end of Land 2) | todo | `LoadMyCreature` is a stub |
| The creature is barred from the temple | todo | `SetCreatureInTemple` is a stub |
| The land's four towns are found: the player's home, the Japanese village, the Indian village and the Egyptian village | todo | |
| All weather is switched off and cleared within 220 of the home town for 10 minutes, to give the player a chance to build up, then switched back on | partial | pausing the climate and clearing storms work (`PauseUnpauseClimateSystem`, `KillStormsInArea` in `src/CHLApi.cpp`); the script never runs |
| Lethys's creature, a wolf named "Laetes", fully grown and 1.5 times normal size, is made near the prison | todo | loading a computer creature is a stub |
| Lethys's computer player is told not to react to an aggressive creature and not to expand his influence | todo | `SetComputerPlayerPersonality` is a stub |

## The prison

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three prison pillars on the hill are made indestructible | partial | `SetIndestructable` works; finding the pillars by type and position does not |
| The creature is put on the prison spot facing the player's temple, its leash is taken away, it is made indestructible and its health is set to a tenth | todo | `SetPosition` works; leash, health and indestructible-on-creature commands are stubs or never reached |
| Each pillar is tied to one village: the second pillar to the Japanese village, the third to the Indian village, the first to the Egyptian village | todo | |
| Every 3 seconds each pillar still standing, if the creature is within 15 of the prison spot, fires a 3-second magic beam from the creature to a point 8 above the pillar, with a small plasma sound, and casts a creature freeze on it | todo | special effects and creature spells from a script are stubs |
| Every 3 seconds each pillar's script also keeps the creature from tiring (its exhaustion reset to nothing) | todo | |
| Lethys's creature is set at a spot by the prison facing the player's creature, and loops until all three pillars are down | todo | |
| If a cow is within 50 of Lethys's creature, it stomps on it and eats it | todo | `CreatureDoAction` is a stub |
| Otherwise, if it is within 75 of the player's creature it tortures it in turn with a fireball, a lightning bolt and an itchy spell, its miracle energy refilled each time, then waits 3 seconds | todo | |
| Otherwise it walks back to its spot | todo | |
| Every tenth time round this loop the creature's frozen moan is heard at the player's creature | todo | `PlaySoundEffect` is a stub |
| When the last pillar falls Lethys's creature looks confused, frightened and angry, four times over, and is then let go | todo | |

## The arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene starts with Nemesis's music; the arrival vortex opens and throws out the player's people into the home town while the camera starts inside the vortex and pulls back over it | todo | the vortex itself: [../portals.md](../portals.md) |
| The shared arrival script is handed 30 people, but it never uses that number: how many people come out is decided by the vortex itself | todo | see [../portals.md](../portals.md) |
| The arrival script also tries to load the player's creature at the vortex, but the game only loads it when the player has none, and it was already loaded in the prison, so the creature does not come through | todo | checked in the executable: loading the player's creature does nothing if the player already has one |
| The vortex fades out 15 seconds after opening and is gone 8 seconds later | todo | `VortexFadeOut` is a stub |
| Both advisors step out. Evil: "Well, that wasn't a trap. But we'd better find our Creature here." Good: "He's got to be around here somewhere." | todo | `SpiritEject`, `RunText` are stubs |
| The creature's frozen moan is heard; the camera swings over to the prison and both advisors look at the creature. Good: "What was that?" | todo | |
| The camera moves closer. Evil: "It sounded like our Creature." | todo | |
| The evil advisor points at it: "Look!" Good: "He's trapped! He must be in agony." | todo | |
| Evil: "Lethys' Creature is torturing him." Evil: "Boy, is he gonna pay for this!" as the camera flies round the prison | todo | |
| Good: "It looks like these statues are holding him in place." | todo | |
| Out of view, the player's temple and worship site are built, a storage pit and a workshop scaffold are set down near the home town, and the town centre is built | todo | `BuildBuilding`, `SetScaffoldProperties` are stubs |
| The quest is logged at nothing done, its reminder the good advisor's line about the statues | todo | |
| The camera turns to the villages. Evil: "But where are they drawing their energy from?" Both advisors look at the camera. Good: "Only the prayers of many Villages could be that strong." | todo | |
| The camera returns over the home town, the advisors go home and the music stops | todo | |
| 30 seconds later the advisors speak again. Good: "I think we need to get this Village built." Evil: "Yeah, we gotta be strong for when we get our Creature back." Good: "This place will give us a foundation to fight from." | todo | |
| The arrival waits for the vortex to have closed before the rest of the land's scripts start | todo | |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Win over the Japanese, Indian and Egyptian villages (by impressive miracles and belief, or by destroying them: a village that no longer exists counts as won) | todo | see ../town/ for village ownership |
| A pillar is checked every 3 seconds: when its village becomes the player's, it falls; when the village is lost again, it rises | todo | |
| The quest's progress mark is a third per pillar down, and goes back down when one rises | todo | |
| There is no time limit and nothing ends the quest in failure | todo | |

## A pillar falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene with epic music: the camera flies to look at the won village, waits 2 seconds, fades to black and back onto the pillar (aimed a quarter of the creature's height higher for the Japanese pillar) | partial | setting the camera and fading work (`SetCameraPosition`, `SetFade`, `SetFadeIn`); camera flights and cut scenes are stubs |
| The evil advisor points at the pillar. First time: "Look! When we took control of that Village, one of the Pillars fell." Later times: "We've taken over the Village!" | todo | |
| The camera shakes for 5 seconds within 150 of the pillar, a big cloud of dust (7 times normal size) rises and the gate-stone grinding sound plays | todo | `ShakeCamera`, `SpecialEffectPosition` are stubs |
| The pillar sinks into the ground in steps of a quarter until it is 20 below | todo | |
| The log's mark is updated, its reminder becoming the evil advisor's line about the pillar falling | todo | |
| The first time only, the good advisor: "If we manage to keep control of it, I reckon it will stay down!"; and if this is the first pillar: "There are two more pillars and two more Villages. It's clear to me we should take those Villages, Leader." | todo | |
| For the first two pillars the scene fades back to the village it was won from (not to where the player was looking) | todo | |
| The creature-help prompts are switched off at the end of the scene | todo | developer function 11; not handled by `DevFunction` in `src/CHLApi.cpp` |

## A pillar rises again

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If a won village is lost (to Lethys or anyone else), its pillar rises: a cut scene with the failure music fades to the pillar | todo | |
| Both advisors step out and the evil advisor points. First time: good, "Disaster! We lost the Village!"; later: "Oh no. A Village has been lost!" | todo | |
| The same shake, dust and grinding sound; the pillar rises by quarters back to ground level | todo | |
| The first time only, evil: "The pillar's back, keeping our Creature imprisoned." | todo | |
| The log's mark drops a third, its reminder becoming the evil advisor's line about the pillar being back | todo | |
| The scene fades back to where the player was looking | todo | |

## The creature is freed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the third pillar falls the evil advisor says "We've taken over the Village!" (the long line is skipped) and nothing else of the usual pillar talk | todo | |
| Lethys: "Quickly, my Creature. Take that last Village back for me!" | todo | |
| The camera flies over the prison. Good: "Our Creature's free but he's really hurt! Leash him and get him home. He needs rest." Evil: "It's good to have the big guy back." Good: "Yes. We're complete again." | todo | |
| The freeze is lifted; the camera turns to the creature, which looks at it | todo | |
| The log is marked complete | todo | |
| The creature waves for attention ("look at me"), walks down towards the lowland while the camera follows, looks at the camera again and gives a friendly wave | todo | |
| The creature is released from the script: its miracle energy is filled, the leash is given back and it may go into the temple again | todo | |
| Lethys's computer player is told to react to an aggressive creature again | todo | |
| The pillars turn their faces away from the prison and the beams and plasma sound stop | todo | |
| The creature stays at a tenth of its health, as the good advisor warns; nothing in the script heals it | todo | |
| The creature is no longer indestructible once all three pillars are down | todo | |

## Lethys gives up the creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 5 minutes after the creature is freed, the script starts watching Lethys's own town (the fourth, high in the west) | todo | |
| It waits until Lethys's lead in belief over the player in that town has fallen below half of what Lethys's belief was when the creature was freed, or the player's belief there passes 400 | todo | `BeliefForPlayer` is a stub |
| The three pillars are forced fully down, in case they were cheated past | todo | |
| A fade to black; both creatures are moved to a spot in the lowland, both leashes taken away, the player's creature made indestructible, and Lethys's hand held still and flown above the prison | todo | `MoveComputerPlayerPosition` is a stub |
| A cut scene with epic music. Lethys: "Your power is mighty and I have little strength." | todo | |
| The camera flies to the prison; it shakes for 5 seconds within 300, dust rises at all three pillars and the pillars rise back to ground level | todo | |
| Lethys: "Spare me and I will give you everything." The camera closes in. Lethys: "Here. I give you the Creed from Khazar's Creature." | todo | |
| Lethys's hand drifts back over the prison; a fade to white, and the player's creature is back on the prison spot | todo | |
| A creed glow appears above the creature's head (five times normal size), faint at first, then at full strength as the camera rises to it | todo | `SetCreatureCreedProperties` is a stub; see ../creature/ for the creed |
| Lethys: "Allow me to survive and I will guide you to yet another Creed." Lethys: "It is in a land you once knew. Use this Vortex." | todo | |
| The exit vortex is opened with its own gold scroll ([leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md)) and the camera turns to it. Lethys: "Find the Creed there. It is yours." | todo | |
| The camera turns back. Lethys: "But I beg. Please leave me with my last Village." Lethys: "Without it I will be banished to the void." | todo | |
| The glow is switched off, the music stops, and the creature, its leash, Lethys's creature's leash and Lethys's hand are all let go | todo | |

## Sparing or finishing Lethys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player may leave through the vortex straight away, sparing Lethys | todo | |
| If the player instead wins Lethys's last town (or it is destroyed), Lethys's hand is held and flown away from his temple during a cut scene with Nemesis's music | todo | |
| A fade to his temple. Lethys: "You have taken everything from me." Lethys: "I am nothing now. A denizen of the great void." | todo | |
| Lethys's temple explodes, watched from three camera angles over 26 seconds | todo | see [../losing_and_game_over.md](../losing_and_game_over.md) for temples blowing up in the story |
| Lethys's computer player is switched off; good: "Lethys has gone." Evil: "Serves him right. Now let's go through the Vortex to look for the other Creed." Good: "Yes. If we're ready." | todo | `EnableDisableComputerPlayer2` is a stub |
| The choice is remembered: arriving on Land 4, the good advisor praises sparing Lethys ("You know, you did the right thing by sparing Lethys, Leader.") or the evil advisor cheers his death | todo | the Land 4 arrival; see [../land_4.md](../land_4.md) |
| Leaving through the vortex before Lethys's town is won stops this script, so Lethys counts as spared | todo | |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The story cannot move on until all three villages are held at the same time: the exit vortex only opens after the creature is freed and Lethys's town is in trouble | todo | |
| If Lethys's town no longer exists before the creed scene, his belief there may never be readable and the creed scene may never come, leaving no way out of the land (undetermined: what the belief reading gives for a destroyed town) | todo | not traced in the executable |
| Its own steps start the Lethys fanatics' fire attack (when the Japanese village is the first won) and the possessed wolves (when the Indian village is the second) | todo | see [fire_fire_im_on_fire.md](fire_fire_im_on_fire.md), [the_wolves_are_possessed.md](the_wolves_are_possessed.md) |
| When the wolves attack and the monk's own quest is done, the monk's Wonder gift is started too | todo | see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| Next: the exit vortex to Land 4 | todo | [leave_through_the_vortex_land_3.md](leave_through_the_vortex_land_3.md) |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music over the arrival and over Lethys's end; epic music when a pillar falls and over the creed scene; the failure music when a pillar rises | partial | `StartMusic`, `StopMusic` work; the scenes never run |
| The creature's frozen moan on arrival and while it is held; the gate-stone grinding as pillars move; a small plasma sound with each freeze beam | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The main loop that starts the two attacks, the loop waiting for Lethys's town and the loop waiting for a town to be taken over have no wait in them | todo | |
| The fire attack only starts if exactly one pillar is down and the Japanese village is the player's; the wolves only if exactly two are down and the Indian village is the player's. Win the villages in another order and one or both attacks never happen | todo | |
| When a pillar falls the cut scene ends looking at the won village, not where the player was looking (a rising pillar does return the camera) | todo | |
| A commented-out end to the creed scene would have blown up Lethys's creature with three explosions | n/a | cut before release |
| A debug switch makes every pillar fall at once | n/a | developer switch, never set by the shipped scripts |
| A land-specific copy of the arrival script, which would have thrown out alternating Celtic men and housewives, is compiled but never used | n/a | the shared arrival script is used instead |
