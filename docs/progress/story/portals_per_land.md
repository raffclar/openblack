# Portals, land by land

Every vortex the story makes, land by land: where it opens, what makes it open, what the scripts say and do around it,
how it differs from the others and what can go wrong. How a vortex itself works (what it pulls in, the crossing file,
the creature, the arrival) is in [portals.md](./portals.md). The quests that open the exits are told line by line in
[gold_scrolls/](./gold_scrolls/); rows here keep the order and the speakers and link there for the full scenes.

**Progress: 0/91 done, 4 partial — 2%**

Sources: the shipped challenge scripts' source text (land control, the exit quests, the arrival scripts and the shared
arrival script), the game's text table and the executable. openblack is judged on the physics work tree
(`ob-wt-physics`); none of the land control scripts reach their vortices there.

## Every vortex in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shipped scripts make eleven vortices: Land 1's exit; Land 2's arrival, the one Nemesis opens at Khazar's death, Lethys's and the exit; the arrivals of Lands 3, 4 and 5; the exits of Lands 3 and 4; and Land 5's volcano | todo | no other shipped script, land set-up file or playground makes one |
| The tutorial, the Creature Isle, the playgrounds and skirmish games make no vortex | n/a | nothing to port; see [tutorial.md](./tutorial.md) and [creature_isle.md](./creature_isle.md) |
| Every arrival but Land 2's uses one shared arrival script: make the outgoing vortex, give it its town and spread, load the creature, wait 15 seconds, start the fade-out, wait 8 seconds, mark the vortex closed | todo | Land 2 has its own copy of the same steps |
| The shared arrival script is told to bring 30 people, but never uses the number: the vortex always tops up to 30 villagers itself | todo | quirk; see [portals.md](./portals.md#arriving) |
| The story's control script, started by the game, loads each next land after its exit and stops every other script first | todo | `LoadMap` is a stub in `src/CHLApi.cpp` |

## Land 1 exit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The exit opens on the north coast near the Norse village, about two minutes after Nemesis's storm kills the guide (40 seconds, the scene at the village, a minute, then 30 seconds more) | todo | the storm's part: [creature_guide.md](./creature_guide.md); the quest: [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md) |
| A yellow "see this" beam marks the spot; good advisor: "This is hopeless. Wait. What's this golden light?" while the evil advisor points at it | todo | |
| Nothing more happens until the camera comes within 200 of the spot | todo | |
| The player's creature is forbidden to go through, and its kind is recorded for the next land though nothing ever reads it | todo | quirk |
| In a cinema a Norse shepherd is made 24 from the spot, looking about; the incoming vortex is made and the beam removed | todo | creating villagers and vortices is not done (`CreateScriptObject` in `src/CHLApi.cpp`) |
| The shepherd really is pulled in once the vortex is fully open 7 seconds later: he stands inside the half radius it searches every turn, so he is written to the crossing file and comes out in Land 2 as a Norse shepherd | todo | the script plays this as a scene, but the vortex does the work |
| Evil: "What's this? Some trick? Some new Miracle?"; good: "It looks like a portal of some kind."; evil: "It looks like a trap to me." and "Look. It's sucking stuff in."; good: "Try throwing something into it." and "I bet you anything it'll come out wherever this Vortex leads to." | todo | |
| The shepherd comes back after 60 seconds, or once the player has picked something up and let go of it (or held it for 60 seconds), when the vortex is in view with the camera within 150 | todo | |
| The returning shepherd is a new villager made 16 from the middle and flung up and towards the camera at speed 16: "Cor. There's a whole new world through there." | todo | he is made inside the vortex's reach; whether he lands outside it was not worked out |
| Good: "That's it. I'm going through for a look." (pointing high, then into the vortex); evil: "He's braver than I thought. He's gone through."; good: "The Villager was right. There's a new land through there." and "And we should send plenty of food, wood and followers into the Vortex as well." | todo | the last line is shared with Land 4 |
| Evil: "You really aren't afraid of this thing, are you?"; good: "I say we throw things through and then go through it ourselves. Our Creature will follow. Try clicking the Action Button on the Scroll." as the gold scroll appears 20 above the vortex; evil: "What the hell. Do we want to live forever?"; good: "Well actually…" | todo | |
| A game that skips the creature guide opens the vortex and its scroll at once, with no scenes | todo | |
| Straight away and then every 30 minutes until the scroll is clicked, "There's a Gold Story Scroll ya gotta click on, Boss." is said, with no advisor stepping out | todo | quirk: the quest passes a voice where an advisor is expected |
| After the scroll the creature is allowed through; in a skipped-guide game the permission is set on nothing, so it is never given | todo | quirk; the creature crosses anyway because it is saved with the land |
| Clicking the scroll: the camera climbs to 100 above the vortex over 6 seconds, then dives into it over 8; 5 seconds into the dive the screen fades to black over 2 | todo | `MoveCameraPosition` is a stub |
| The storm's and guide's scripts are stopped and the land is marked as left; the control script loads Land 2 | partial | stopping scripts by file works (`StopScriptsInFiles`); the land load does not |

## Land 2 arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The screen is black; the game time is set to noon; the village gets the wood, food and water miracles and may not build a worship site yet | todo | see [land_2.md](./land_2.md) |
| An empty flock is made at the arrival point (inner 2, outer 10), and the outgoing vortex is made there for town 0, gathering the newcomers 220 to the west, spreading them 27 within 48, with that flock | todo | `VortexParameters` is a stub |
| Because a flock is given, every villager that comes out becomes a disciple from the vortex, standing in the crowd, until the opening scenes end and the flock is disbanded | todo | Land 2 is the only arrival that does this; what the villagers do once the flock is disbanded was not traced; see [../villager/disciples.md](../villager/disciples.md) |
| The creature is loaded beside the vortex, turned to face it, and told to walk to its spot | todo | `LoadMyCreature` is a stub |
| The camera starts just over the vortex looking down; the ground shakes (within 300, strength 0.3, 12 seconds) as the land fades in over 6 seconds | partial | the fade works (`SetFadeIn`); `ShakeCamera` is a stub |
| The camera rises over the vortex (10 seconds), moves above it looking over the island (12) and swings round (14) as Khazar flies in | todo | |
| Good: "I feel all inside out. Ugh."; evil: "Yeah, what a buzz." | todo | |
| Khazar then greets the player ("I greet you as one god to another. I am Khazar." … "It was I who provided the Vortex to save you from Nemesis.") and gives scaffolds, a disciple builder and supplies | todo | see [land_2.md](./land_2.md) and [../rival_gods/khazar.md](../rival_gods/khazar.md) |
| 15 seconds after it is made the vortex starts to fade out, and 8 seconds later counts as closed | todo | it stays open longer if it still has things to throw |

## Land 2: Khazar's death and Lethys's vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At Khazar's death Nemesis opens an incoming vortex east of Khazar's town and fires fireballs from it | todo | the scene: [gold_scrolls/nemesis_no.md](./gold_scrolls/nemesis_no.md) |
| That vortex is a real exit: while it is open it pulls in Khazar's villagers, animals and loose things within its reach and empties the crossing file | todo | nothing it takes ever reaches Land 3, because the next incoming vortex empties the file again (quirk) |
| Lethys's creature carries the Creed piece to that vortex; the script deletes it there and fades the vortex out | todo | the vortex does not take it |
| Lethys takes the player's creature only after Khazar's death scene has run, checked every 9 seconds: when at least two of Lethys's three key towns are no longer his, or he has at most one town left | todo | the quest: [gold_scrolls/lethys_has_taken_our_creature.md](./gold_scrolls/lethys_has_taken_our_creature.md) |
| Lethys flies over his temple. Lethys: "You are truly a powerful god but you failed to defeat me."; his incoming vortex opens in front of the temple; the creature is put on the far side of the valley facing it | todo | |
| Lethys: "Let's see how powerful you are without your Creature."; a beam from Lethys holds the creature, which is walked to the vortex; its leash stops working and it cannot go back to the temple | todo | |
| Evil: "Look! He's heading for the Vortex! He's mesmerised!"; good: "The Creature! Get him on a Leash quickly!"; the creature cannot die while it walks; if the player leashes it, good: "The Leash isn't working." (once) | todo | |
| At the vortex the creature sparkles away (the script is moving it, so the vortex lets it near) and stays invisible; it can die again | todo | see [portals.md](./portals.md#the-creature) |
| Evil: "NO! He's gone through! And so has Lethys!"; good: "Lethys has taken our Creature! What are we going to do?"; evil: "We're going to get him back. Let's go through!"; good: "Yes! No! Let's think about this. It could be a trap." | todo | |
| A challenge scroll appears over the vortex, with no reminders; evil: "No! There isn't time to think! The Vortex is closing!"; good: "But we don't know what's on the other side!"; evil: "Our Creature is on the other side! That's all we need to know. Let's go!" | todo | |
| The player has 10 seconds after that talk to click the scroll | todo | |
| Clicked in time: the land is marked as left; the camera flies to the start of the exit camera path, runs it and fades to black over 2 seconds | todo | `RunCameraPath` is a stub; see [../camera/camera_paths.md](../camera/camera_paths.md) |
| The next land is loaded as soon as those 10 seconds are up, cutting the dive short if it is still running | todo | quirk: the land control script waits for Lethys's scene, not for the dive; the path's length was not measured |
| Missed: the vortex fades out; evil: "All we gotta do is destroy his Temple to eradicate all belief in Lethys!"; the creature stays invisible in Land 2 and is carried on anyway when the land is left | todo | |

## Land 2 exit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The exit opens when Lethys has no towns and his temple is at a tenth of its health or less, checked every 9 seconds | todo | the quest: [gold_scrolls/leave_through_the_vortex_land_2.md](./gold_scrolls/leave_through_the_vortex_land_2.md) |
| The check runs on every loop, separately from the creature's theft: in the loop that first runs Khazar's death the theft check is skipped, so if Lethys is already beaten the exit can open with the creature never taken | todo | quirk: the advisors then still talk of getting the creature back |
| The camera looks over Lethys's temple, which explodes; it then looks down over the spot | todo | |
| A new incoming vortex opens 12 from where Lethys's stood, emptying the crossing file of anything sent through his vortex | todo | quirk |
| Good: "Look! The Vortex has opened again!" and "I see! Lethys' Temple was hiding a Vortex. Let's go through!"; good: "We've got to get our Creature back."; evil: "Yeah. The poor guy. I really miss him."; good: "We've no choice. We must follow him through the Vortex." | todo | |
| A gold scroll appears 20 above the vortex; the good advisor reminds the player, "Action Button the Scroll and we'll get through the waiting Vortex.", at most every 30 seconds while the camera is within 100 and the scroll in view | todo | |
| Clicking it: the camera flies to the start of the exit camera path over 4 seconds, runs it, and fades to black over 2; the land is marked as left and Land 3 is loaded | todo | |

## Land 3 arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 3 fades in over 3 seconds; its set-up has already put the player's creature in Lethys's prison | todo | the scene: [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](./gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md) |
| The outgoing vortex opens in the player's valley for the home town, gathering at the vortex, spreading 20 within 80, with no flock: villagers that come out simply join the town | todo | |
| The script asks for the creature to be loaded at the vortex, but the player already has it, so nothing happens: the creature does not come out of this vortex | todo | |
| With Nemesis's music, the camera starts just over the vortex and climbs to 250 over 6 seconds | todo | |
| Evil: "Well, that wasn't a trap. But we'd better find our Creature here."; good: "He's got to be around here somewhere."; the creature's frozen moan is heard from Lethys's prison | todo | earlier notes said Nemesis's prison; it is Lethys's |
| Good: "What was that?"; evil: "It sounded like our Creature." and "Look!"; good: "He's trapped! He must be in agony."; evil: "Lethys' Creature is torturing him." and "Boy, is he gonna pay for this!"; good: "It looks like these statues are holding him in place." | todo | |
| Out of sight the temple, worship site, storehouse and workshop scaffolds and village centre are started; Lethys: "So you couldn't bear to be without your Creature?" is logged as the gold quest | todo | |
| Evil: "But where are they drawing their energy from?"; good: "Only the prayers of many Villages could be that strong."; 30 seconds after the scene, good: "I think we need to get this Village built."; evil: "Yeah, we gotta be strong for when we get our Creature back."; good: "This place will give us a foundation to fight from." | todo | |

## Land 3 exit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The exit opens in the lowland when Lethys gives the second Creed piece after the creature is freed: Lethys: "Allow me to survive and I will guide you to yet another Creed." and "It is in a land you once knew. Use this Vortex." | todo | the quest: [gold_scrolls/leave_through_the_vortex_land_3.md](./gold_scrolls/leave_through_the_vortex_land_3.md) |
| The incoming vortex and its gold scroll (20 above) appear at once; the evil advisor reminds the player, "Let's go, Boss. Hit the Scroll and we're out of here.", at most every 30 seconds near the scroll | todo | |
| Lethys goes on: "Find the Creed there. It is yours.", "But I beg. Please leave me with my last Village." and "Without it I will be banished to the void." | todo | |
| The vortex stays open while the player decides; taking Lethys's last village first kills him ("You have taken everything from me." … ; evil: "Serves him right. Now let's go through the Vortex to look for the other Creed."), leaving first spares him | todo | Land 4's arrival lines depend on it |
| Clicking the scroll: the camera climbs over 6 seconds, then dives into the vortex over 8 while the screen fades to black over 8; the land is marked finished and Land 4 is loaded | todo | |

## Land 4 arrival

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The outgoing vortex opens beside the old home village for its town, gathering at the vortex, spreading 20 within 80, with no flock; the creature is loaded nearby | todo | see [land_4.md](./land_4.md) |
| Until the vortex counts as closed the creature is made to keep pointing at it over and over, so it cannot wander in | todo | |
| With Nemesis's music the camera starts in the vortex looking down and rises 65 out of it over 10 seconds as the land fades in over 6 | todo | |
| If Lethys was spared — good: "Wow! Here we are." and "You know, you did the right thing by sparing Lethys, Leader."; if he was killed — evil: "Hey Boss! Here we are!" (the line praising his death is cut) | todo | |
| Along a camera path — good: "Just look at the state of the place."; evil: "It's been utterly blasted. How cool is that?"; good: "It's awful. It used to be green and pleasant."; evil: "What? We've never been here before!" | todo | |
| Good: "We have, actually. You remember these?" pointing at the gates; good: "And I recall some of these buildings."; good: "And even our Temple. It's all rather damaged, though."; evil: "You're right. Still, the atmosphere's much better now." | todo | |

## Land 4 exit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once all three meteorites are broken and the Creed scenes are over, the ground starts to rumble from a hidden spot | todo | the quest: [gold_scrolls/leave_through_the_vortex_land_4.md](./gold_scrolls/leave_through_the_vortex_land_4.md) |
| Each rumble plays the screen-rumble sound and shakes the camera; within 500 of the spot the shake is stronger and longer the closer the camera (up to strength 3 for 3 seconds) | todo | `ShakeCamera` is a stub |
| The sound plays with every rumble however far away the camera is, and the shake keeps the strength of the last rumble felt in range | todo | quirk |
| Rumbles start 30 seconds apart; once the camera has been in range they come a second sooner each time, down to 3 seconds, even if the camera leaves again | todo | quirk |
| The first strong rumble draws one comment by its strength — evil: "That was big. It came from over there."; good: "That was a mighty rumble. It came from here."; or good: "Did you feel that rumble, Leader? What could cause that?" and "It seemed to come from over there." | todo | |
| As the rumbles quicken — evil: "Ooh. I got another rumble then. Boss." (25 and 20 seconds apart), good: "These rumblings seems to be getting faster, Leader." (15 and 10), evil: "The rumbling's really fast now, Boss." (5), each only for a strong rumble | todo | |
| Five rumbles at 3 seconds open the vortex with a quick fade to a view of it; the camera coming within 100 of the spot opens it at once with a short camera move | todo | |
| The incoming vortex opens; Nemesis: "Ah yes. My adversary.", "You have the second part of the Creed. But it is useless unless you have three parts." and "But I am still more powerful than you can possibly imagine." | todo | |
| The ground trembles lightly (within 300, strength 0.03, 10 seconds); Nemesis: "I have opened a Vortex to my realm.", "Come through. I invite you. For only you and I remain." and "Come and face your destiny." | todo | |
| Evil: "Yeah. Time to take on Nemesis. I've been waiting for this."; good: "If we go through, there's no coming back. This is where we stand or fall."; evil: "Hey. You're right. Boss, are we ready for this? I mean we might die through there."; good: "We might. If we go through, we must be well prepared for what we face." and "And we should send plenty of food, wood and followers into the Vortex as well." | todo | |
| A gold scroll appears 20 above; the evil advisor reminds the player, "Boss, when you're ready, let's get ourselves to Nemesis' realm!", at most every 30 seconds near it | todo | |
| Clicking it: the camera climbs to 250 over 6 seconds, then dives into the vortex over 3 while fading to black over 3 | todo | |
| The screen then fades back in over 1 second on the old land before Land 5 loads | todo | quirk |

## Land 5 arrival and the volcano

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 5's set-up makes the volcano vortex at the crater; it sits there for the whole land, rumbling, until the creature's last walk | todo | see [ending.md](./ending.md) and [land_5.md](./land_5.md) |
| The outgoing vortex opens below the home village for its town, gathering at the vortex, spreading 20 within 50, with no flock; the creature is loaded beside the village | todo | the scene: [gold_scrolls/i_have_a_surprise_for_you.md](./gold_scrolls/i_have_a_surprise_for_you.md) |
| Nemesis is paused and moved in, made hostile and not allied | todo | |
| With Nemesis's music the land fades in over 6 seconds as the camera rises from just over the vortex over 10 | todo | |
| Evil: "Time to face the ultimate foe, Boss." and "But hey - let's show no fear." | todo | |
| Nemesis: "I have been watching your power grow.", "You are the only foe left and I have enjoyed your successes." and "The stronger you are, the more impressive it will be when I crush you." | todo | |
| For an evil player Nemesis ends "…Nemesis, the embodiment of good."; otherwise "…the embodiment of evil." | todo | the player's alignment carried over from the earlier lands |

## Fades between lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every exit ends on a black screen before the land is cleared | partial | `SetFade` works through the cinematic director; the exits are never reached |
| Lands 2, 4 and 5 fade in over 6 seconds from their arrival scripts, Land 3 over 3 seconds from the story's control script | partial | `SetFadeIn` works; the arrivals are never reached |

## Unused vortex content

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A helper that sets a newcomer's town, flings it out and keeps it at full health while it flies is never used | n/a | the vortex does this itself |
| An older Land 3 arrival that flung out 30 hand-made Celtic villagers (alternating men of random jobs and housewives, at speed 15 to 30) is never run | n/a | |
| Test scripts that start each exit or arrival on their own (including one that runs Land 1's exit and loads Land 2) are not in the shipped set | n/a | |
| A test script with a black storm cloud mentions vortices but is not shipped | n/a | |
| The shared arrival script's commented-out lines would have walked the creature to its spot and stopped it reacting to other creatures | n/a | |
| Land 1's recorded creature kind "for setting up the next land" is never read | n/a | |
