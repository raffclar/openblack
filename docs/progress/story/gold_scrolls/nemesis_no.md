# Nemesis. No!

Khazar's death on Land 2: as soon as the player starts winning towns from Lethys (or has six towns), Nemesis opens a
vortex beside Khazar's town, burns it with fireballs, blows up Khazar's temple and kills Khazar's creature with a
storm; Lethys's creature takes the piece of the Creed from the body and walks into the vortex, and the advisors work out
what the Creed is. It has no scroll: it is a story film that records a gold (quest) entry in the challenge log, filed
under Khazar's last words, "Nemesis. No!". The land as a whole is in [../land_2.md](../land_2.md), Khazar in
[../../rival_gods/khazar.md](../../rival_gods/khazar.md#his-death), Nemesis in
[../../rival_gods/nemesis.md](../../rival_gods/nemesis.md).

**Land:** 2 · **Giver:** none (a story event at Khazar's town; Nemesis speaks) · **Script:** KillKhazarMain · **Reward:** none · **Repeatable:** no

Sources: the quest's script source (`KillKhazar.txt`) and the land's control script (`LandControl2.txt`), checked
against the compiled form in the shipped `challenge.chl`; the land's map script (`Land2.txt`) for the towns' owners;
the game's English text table; and the executable (read-only) for stopping scripts by name and for the challenge log.
openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 59 script functions this film needs, 39
still only log "not implemented" in `src/CHLApi.cpp` (among them the moving camera, dialogue, creatures' actions and
desires, the computer players, weather, deleting objects, the Creed glow and the challenge log), and openblack never
runs Land 2's control script at all (see [../land_2.md](../land_2.md) row 1), so Khazar never dies; every row below is
todo unless the notes say otherwise.

**Progress: 0/63 done, 11 partial — 9%**

## When it happens

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds whether Khazar should die, until it has happened | todo | the control script never runs in openblack |
| Each check counts reasons: one for each of Lethys's three starting towns (the two Celtic towns nearest the player and his home town) that he no longer holds; one if the player has more than five towns; one if Khazar has no towns left or Lethys has two or fewer | todo | town totals and owners are stubs (`GetTownWithId`, the town's owner, the player's town total) |
| Khazar dies if there is any reason at all, so in practice the first town the player takes from Lethys (which also drops him to two towns), a sixth town of the player's own, or Khazar losing both his towns sets it off | todo | Lethys starts with exactly three towns and Khazar with two (map script); the script tests "count is not zero", so one reason is enough |
| It waits while Khazar is busy in one of his own lessons or challenges (Khazar's "in a script" flag); the check repeats until he is free | todo | see [worship_site.md](worship_site.md), [impress_village.md](impress_village.md), [khazars_fireball_challenge.md](khazars_fireball_challenge.md), [khazars_shield_challenge.md](khazars_shield_challenge.md) |
| The control script waits for the whole film to finish before going on | todo | |
| Straight after it, the land's last gold scroll is started if it wasn't already | todo | see [destroy_it.md](destroy_it.md) |

## Setting the stage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's own behaviour script (his visits and comments) is stopped, and the land is told Khazar is gone | partial | stopping a script by name works (`StopScript` in `src/CHLApi.cpp`); the land never gets here |
| Two cows are put in a small flock (inner 5, outer 15) just east of where the vortex will open | todo | `FlockCreate`, `PopulateContainer` are stubs |
| Khazar's hand flies fast (speed 400) to above his temple | todo | moving a computer player is a stub |
| The player's creature is moved to a spot north of Khazar's town (by the Indian town there) and made to stand idle | partial | `SetPosition` works; `CreatureDoAction` is a stub; not determined what happens if the creature is in the hand or on a leash at the time |
| Lethys's and Khazar's creatures have all their own wants switched off; Khazar's creature is put just west of its town's buildings and made to cower on the spot; Lethys's creature is made to stand idle | todo | `CreatureSetDesireActivated`, `CreatureDoAction` are stubs |
| While the film runs, a background script keeps both rival creatures alive and not burning, every 2 seconds: Khazar's until the storm strikes it, Lethys's until it has gone into the vortex | todo | |

## The film: the vortex and the fireballs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The screen fades to black over 2 seconds; the camera's position and aim are remembered | partial | `SetFade` and reading the camera work |
| The time of day is set to 16:50 and stopped | partial | `SetGameTime`, `GameTimeOnOff` work |
| The camera is put high to the north-east, aimed at the vortex spot east of Khazar's town, with a wide lens slowly narrowing; Nemesis's music starts; the screen fades in over 3 seconds | partial | `SetCameraPosition`, `SetCameraFocus`, `StartMusic`, `SetFadeIn` work; the lens is a stub |
| The ground shakes (radius 700, strength 0.5, 3 seconds), then a vortex opens there and the ground shakes harder (radius 200, strength 0.7, 6 seconds) | todo | `ShakeCamera` is a stub; making a vortex: see [../portals.md](../portals.md) |
| Khazar's creature turns to the vortex and cowers; the camera drifts down towards the town over 18 seconds | todo | |
| A huge black storm cloud forms over the vortex (rain, heavy overcast, lightning sheets and a few forks, wide reach) | todo | the weather-property commands are stubs |
| Nemesis: "Lethys, my ally. You have failed me. I, Nemesis, have arrived to take control." | todo | `RunText` is a stub |
| Fireballs fly from the vortex in curling volleys: five at the crèche, four at the tents, four at another tent, four at the town centre; Khazar's creature turns to watch each and cowers | partial | casting a miracle at a place works (`SpellAtPos`), but from a point in the air with a curl is not checked |
| The crèche is set on fire; between volleys, Nemesis: "But I will not let them threaten my destiny." and "I will be the only god." | partial | `SetOnFire` works |
| The building at the town centre is set on fire | todo | |

## The film: Khazar's temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second black cloud (no forks, 30-second life) forms over Khazar's temple; the camera looks at it from close, the lens widening | todo | |
| Nemesis: "Khazar. You have defied me for the last time." | todo | |
| Khazar's temple is blown up | todo | deleting a temple with its explosion is a stub (`ObjectDelete`); see [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-is-destroyed) |
| Khazar: "Nemesis. No!" | todo | |
| The challenge log records a quest (gold) entry titled "Nemesis. No!", success 1, alignment 0, its picture the view of the exploding temple, and no reminder line | todo | `Snapshot` is a stub; the source comment: "Snapshot here because it's a big story bit. No reminder though." |
| Nemesis: "Khazar, I know your Creature hides a part of the Creed inside him.", "I do not need it. I already have the power that three combined Creeds brings.", "And I will use it to destroy you." | todo | |
| Khazar's computer player is switched off, and the player's alliance with Khazar is set to none | todo | `EnableDisableComputerPlayer`, `SetPlayerAlly` are stubs; see ../../rival_gods/khazar.md |

## The film: Khazar's creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 10 seconds, five of the town's buildings burn (from 0.4 to 0.7) | partial | `SetOnFire` works |
| The camera looks across the burning town at Khazar's creature, closing in on it over 14 seconds | todo | |
| The storm cloud moves (speed 20) over the storage pit, then the big building, then onto Khazar's creature, its lightning forks dying away when it arrives | todo | moving a weather object is a stub |
| A level-2 explosion miracle (radius 50, 7 seconds) strikes Khazar's creature from the cloud, and it dies for good | todo | `CreatureDoAction` (die permanently) is a stub |
| The Creed's glow shows over the body (scale 5, full power) | todo | `SetCreatureCreedProperties` is a stub |
| The cloud drifts back to the vortex | todo | |

## The film: Lethys takes the Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's creature is put on the hill west of Khazar's town, facing the body; the camera follows it from near the vortex | todo | `SetFocusFollow` is a stub |
| It is given the strong creature spell and walks (speed 10) to the body | todo | |
| The buildings are set burning again | partial | `SetOnFire` works |
| Nemesis: "Lethys. Retrieve the Creed from Khazar's dead Creature. It must not fall into our enemy's hands." | todo | |
| Lethys: "As always, I will do as you command. I'll send my Creature for it now." | todo | |
| Once it is within 32 of the body (or after 55 seconds), the glow fades on both, then moves to Lethys's creature at full power, and the body's glow goes out | todo | |
| Lethys's creature turns and walks into the vortex | todo | |
| The evil advisor: "He's got an element of the Creed!"; the good advisor: "Does this mean all is lost? I fear it does!" | todo | the advisors speak without being made to step out |
| The camera pulls up; once out of view, Khazar's creature's body is deleted | todo | |
| When Lethys's creature reaches the vortex (or no longer exists, or after 60 seconds) its spell is removed and the creature itself is deleted: Lethys has no creature for the rest of the land | todo | the source comment wonders whether the vortex would remove it automatically; the script deletes it |
| The vortex fades out; the camera rises and widens over 14 seconds, aimed above the vortex | todo | `VortexFadeOut` is a stub |
| After 4 seconds the screen fades to black over 2 seconds; the music stops; the storm cloud is deleted; the camera is put back where it was and the screen fades in over 3 seconds | partial | fades, setting the camera and stopping music work |

## The advisors explain the Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The evil advisor steps out: "I can't believe it. Khazar wiped out like a bug!" | todo | `SpiritEject` is a stub |
| The good advisor steps out, and they talk: good "I understand what the old Guide meant, now.", good "The power of three Creeds must be combined.", evil "But Nemesis said he has three Creeds.", good "Yes. But he's after the others to stop us getting them.", evil "We've got to get them, then!", good "Absolutely!" | todo | the guide is Land 1's ([../creature_guide.md](../creature_guide.md)) |
| Game time starts again; the film ends; the player's creature is released back to its own will | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar is gone for the rest of the land: no more visits or comments, no more reactions when towns change hands, and his lessons and challenges that check whether he is gone stop offering him | todo | the town-ownership reactions skip Khazar's lines once he is gone; see ../../rival_gods/khazar.md |
| His temple is gone and his town is left burning; his towns stay where they are, unowned by a god with a temple | todo | not determined what happens to the believers of a god whose temple is gone |
| Neither rival god has a creature on the land afterwards | todo | |
| The land's last gold scroll starts if it hadn't ([destroy_it.md](destroy_it.md)); from the next check on, Lethys steals the creature once he is down to one town or the player holds two of his three ([lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md)) | todo | the creature theft is only checked after Khazar's death |
| It can't fail and can't soft-lock by itself; but if Khazar were stuck "in a script", he would never die, the creature would never be stolen, and the land could only end through Lethys's temple vortex | todo | see Script quirks |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays through the film and stops before the advisors' talk | partial | `StartMusic`, `StopMusic` work; the film never runs |
| The voices are Nemesis's, Khazar's, Lethys's and the two advisors' | todo | |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature is moved to the north of Khazar's town and held idle for the whole film, then released | todo | it watches nothing in particular; it is not in any of the camera's shots by design |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The trigger test counts several reasons but fires on any one, so Khazar dies at the very first loss of a town by Lethys | todo | the reasons are added up as if a total were needed, but the test is "not zero" |
| If Khazar is in one of his scripts when the player wins the land outright (Lethys has no towns and his temple is nearly destroyed), the land's exit can open on the same check before Khazar dies, skipping Khazar's death and the creature's theft | todo | the exit check runs every loop after the death check, and only the death check waits for Khazar to be free |
| A position for Khazar's hand is remembered but never used | todo | |
| The player's creature is released and then looked up again, which changes nothing | todo | |
| Nemesis's first line differs from the source comment ("Lethys, my ally. You have failed. Combined, our enemies are too strong.") | todo | the spoken text is the text table's |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lines written but not played: Nemesis "Did you think you could hide the Creed from me?", "Creature, you have something for me.", "Lethys, retrieve the Creed for me."; the good advisor "That must have been the Creed the Guide was talking about!" and the evil advisor "And we lost it."; good "Maybe we haven't lost it forever, Demon." and evil "I hope you're right, Beardy." | n/a | commented out in the source |
| Lines in the text table never used by any script: Nemesis "I will be the only god." (an earlier copy), Khazar "I'm losing my…", good "Oh no! Khazar's Creature!", Nemesis "Lethys, use your Creature to bring the Creed to me.", evil "He's taken the Creed from Khazar's Creature!", good "So this is what the Guide meant!" | n/a | |
| Dropping the Creed as an object for Lethys's creature to pick up, and freezing Khazar's creature with a creature spell, were tried and replaced by the glow moving from one creature to the other | n/a | commented out in the source |
