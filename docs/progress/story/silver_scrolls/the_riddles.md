# The Riddles

A land 2 silver scroll: once the player owns the second Indian village (Town3), a woman there offers an ancient riddle
whose answer is four things to be put at once inside a ring of toadstools in the woods nearby: a wolf, something hot,
a creature's dropping and a shield miracle. Solving it raises the "great power" the riddle promised: a zebra creature
the player may swap to. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Annika, a woman of the Indian village Town3, at her hut · **Script:** LostTreasure · **Reward:** a zebra creature to swap to · **Repeatable:** no

Sources: the quest's script source (`LostTreasure.txt`), the shared creature-swap script it ends with
(`SwapCreatures.txt`), the scroll-notice and reminder helpers, and its trigger in the land's control script
(`LandControl2.txt`), all checked against the PC game's compiled `challenge.chl` (object types, distances, which
shield is which); the game's English text table for every line and its speaker. openblack's state is judged on the
physics work tree (`ob-wt-physics`): of the 66 script functions the quest and the scripts it starts need, 46 only log
"not implemented" in `src/CHLApi.cpp`, and the land's control script stops at an unwritten function long before it
starts the quest's check (see the first row of [../land_2.md](../land_2.md)), so the quest never appears; every row is
todo unless the notes say otherwise. The internal script name ("lost treasure") is not shown to the player; the scroll
is titled "The Riddles".

**Progress: 0/48 done, 5 partial — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the check for Town3 (the Indian village, id 13) at the start of the land | todo | `LandControl2.txt`; the control script stops before this point in openblack |
| Every 6 minutes it looks whether Town3 belongs to the player; the first time it does, the quest starts (so up to 6 minutes after taking the village) | todo | `GetProperty` (the town's player), the timed wait are stubs |
| A silver scroll is made over Annika's hut (the house found within 5 of its spot), 5 above it | todo | `CreateHighlight` stub |
| Eight toadstools at 0.8 size are made in a ring of radius 10 around the riddle's spot, 45 degrees apart, each turned to face the centre: the "ring of life" | todo | `CREATE_WITH_ANGLE_AND_SCALE` works in openblack but only for scenery and rocks (`CreateScriptObject`); toadstools are features, so nothing is made |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll with it on screen, the evil advisor steps out, points at it and says "There's something going on in this hut.", at most every 30 seconds; the scroll then stays active | todo | the shared scroll-notice script; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |

## The riddle (Annika's scene)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene starts with the second generic script music; Annika (an Indian housewife) comes out of her hut in high detail and walks to a spot in front of it | partial | the music works (`StartMusic`); making a villager, high detail and moving her are stubs |
| The camera glides in over 3 seconds, holds 2 seconds, then closes on the hut over 3 more; when she has arrived and the camera stopped, she faces the camera, which then drifts slightly over 8 seconds | todo | `MoveCameraPosition`, `MoveCameraFocus`, `SetFocus` stubs |
| The challenge is recorded in the player's challenge log: title "The Riddles", success 0, alignment 0, and as its reminder this whole scene, so replaying the reminder (or tapping the scroll again) tells the riddle again | todo | `Snapshot` stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| If her hut is damaged (below full health), the evil advisor: "The woman's saying nothing until her house is all fixed up again. Huh." and she walks home without telling the riddle | todo | `GetProperty` (health) stub |
| Otherwise, gossiping: "I have an ancient riddle for you. Solve it and great power will be yours." | todo | `RunText` stub |
| The camera cuts to above the ring and glides down to it over 12 seconds while she gives the clues: "Place in the ring something which howls at night." / "Something hot." / "Something unique to your Creature." / "Then all must be protected." / "You need to place all the ingredients in the ring of life at the same time." with three gossip animations between them | todo | `SetCameraPosition`/`SetCameraFocus` work in openblack; the rest are stubs |
| The camera cuts back to her: "Knock on my house and I'll remind you of the clues." Two seconds later the camera glides back to where the player had it (4 seconds) | todo | the "knock" is the scroll staying active over her hut: tapping it replays the reminder, which is this scene |
| She walks back into her hut and fades away; the music stops | todo | `ObjectDelete` stub |
| After the first telling, the good advisor: "This looks promising. I wonder what we'll be getting?" | todo | |
| The wolves and the ring's watch start after the first telling, even if her hut was damaged and the clues weren't given | todo | quirk of the source |

## The answers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Something which howls at night": any wolf within 10 of the ring's centre, scripted or not (it need not be one of the night pack) | todo | `CallNear` stub. Undetermined: whether a wolf held in the hand counts (a held object isn't on the map) |
| "Something hot": anything burning within 10 of the centre; a rock heated to 500 or more counts, as does a burning tree, hut or fire | partial | the fire test works (`IsFireNear` in `src/CHLApi.cpp`, `FireSystem::IsFireNear`); see [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md) |
| "Something unique to your Creature": any creature dropping within 10 of the centre; it need not be the player's creature's | todo | `CallNear` stub; creature droppings: [../../creature/physiology.md](../../creature/physiology.md) |
| "Then all must be protected": a physical shield or a spiritual shield miracle within 5 of the centre | partial | finding a shield works (`SpellAtPoint` in `src/CHLApi.cpp` looks for a shield standing over the spot, ignoring the radius); see [../../miracles/physical_shield.md](../../miracles/physical_shield.md), [../../miracles/spiritual_shield.md](../../miracles/spiritual_shield.md) |
| All four must be there at the same moment; they are checked in order (wolf, then a tenth of a second later dropping, shield and fire) and the checking goes on until solved | todo | |
| Nothing checks the order they arrive in, and nothing reacts to a wolf escaping or a fire going out; there is no time limit and no failure | todo | lines for these exist but are unused (below) |
| The player may bring the four by any means (the hand, the creature, miracles) | todo | the scripts only test positions; undetermined from them how a wolf is best kept inside |

## The night wolves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the first telling on, every night (game time after 21:00 or before 6:00) a pack of five wolves (inner 5, outer 10) appears in a forest near the ring, once that spot is off screen | todo | `FlockCreate`, `PopulateContainer` stubs; reading the game time works (`GetGameTime`) |
| Two seconds later they go down to the water's edge, and once within 35 of it (or all dead) wander there as a flock | todo | |
| At day (after 6:00 and before 21:00) they go back into the forest and, once within 35 of it (or all dead) and the spot is off screen, the pack is removed | todo | |
| The pack comes back every night for the rest of the land, even after the riddle is solved: the flag meant to stop it is never set by any script | todo | quirk of the source |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When solved, the wolf and the dropping fade away and the shield found is removed (the physical shield if both kinds were there, leaving the spiritual one); the fire is left | todo | `ObjectDelete` stub |
| A cut scene starts with the fourth epic music; the camera moves to look down at the ring from 15 out and 10 up over 3 seconds | partial | music works; camera moves are stubs |
| The log is updated to success 1, alignment 0 (neutral), with the reminder (good advisor) "Remember, we should ask the woman in this hut about her riddles." | todo | `Snapshot` stub; the reminder reads oddly once solved |
| Both advisors step out; good advisor: "This looks promising. I wonder what we'll be getting?" (the source's comment shows a different first line about a groovy effect was meant here) | todo | |
| A zebra creature appears in the ring, turns to the camera and waves; the music stops | todo | `CreateScriptObject` can't make creatures; `CreatureDoAction` stub |
| Evil advisor: "Look. It's a zebra. What do we want with that?"; good: "If we want to swap to the Zebra, summon our Creature and click on him."; evil: "Yeah, yeah. Do we really want a Zebra, though?"; good: "A rare beast indeed. I'm all for it." | todo | the last line isn't waited for before the dialogue ends |

## Reward: the zebra

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The zebra can't be picked up; a silver scroll stands just above its head | todo | the shared creature-swap script, in full in [creature_swaps.md](creature_swaps.md); `SetIdPickupable` works, `CreateHighlight` doesn't |
| It keeps turning to look at the camera; with the camera within 100 of its scroll and the zebra on screen, the evil advisor points: "Swap your Creature with this one if you want.", at most every 60 seconds | todo | |
| Clicking the zebra or its scroll: if the player's creature isn't within 50 of it, evil advisor: "You'll need to bring our Creature, Boss." and the scroll returns | todo | |
| Otherwise the leash is taken off, the player's creature walks up to within 15 (given 5 seconds to get within 20), and the two look each other over | todo | `DetachObjectLeash` works; moving the creature is a stub |
| Good advisor every 30 seconds: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel."; clicking the player's own creature, or waiting 110 seconds, cancels | todo | `ClearClickedObject`, `CreateTimer` stubs |
| Clicking the zebra again swaps: a two-creature camera, sparkles on both, both point at each other, the swap sound plays and the player's creature becomes the zebra; three seconds after both animations end the camera is let go | todo | `SwapCreature`, `StartDualCamera`, `PlaySoundEffect` stubs; the swap itself: [../../creature/species_choice.md](../../creature/species_choice.md) |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." The old creature stays at the spot under its own scroll and can be swapped back the same way, any number of times | todo | the zebra as a species exists in openblack (see species_choice.md) |
| The offer never ends: the swap script sets up a random 60–120 second limit but never checks it, so the swap scroll stays for the rest of the land | todo | quirk of the shared script |

## Advisors, music and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's lines are spoken by the evil advisor (the notice, the damaged-hut line, the zebra doubts), the good advisor (the hopes, the swap instructions) and Annika | todo | `SpiritEject` stubs; see [../advisors.md](../advisors.md) |
| Music: the second generic script theme for Annika's scene, the fourth epic theme for the zebra's arrival; both are stopped at the end of their scenes | partial | `StartMusic`/`StopMusic` work in openblack, unreached |
| The creature can supply the dropping and could carry or throw things in, but the quest asks nothing of it until the swap | todo | |

## Script quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ring's watch has no wait while no wolf is in the ring: it checks continuously | todo | as compiled |
| The two shield variables are named the wrong way round (the "magic" one holds the physical shield); harmless | todo | |
| Unused variables in the main script (a tiger, a "chosen one", a clue number, a clue on the ground, a fourth artifact, effect timers) and a "hot object" in the ring's watch point to a longer, step-by-step design | todo | |
| A turn-around animation for Annika, an earlier camera focus and a second snapshot after the scene are commented out | todo | |
| The source's comments quote earlier wordings: Annika's opening ("I've got this here ancient scroll that legend says leads to a great power…"), "Knock on my door…", and the damaged-hut line ("Apparantly the broad won't tell us any more clues until she's fixed her house.") | todo | |
| A tiny test script exists that runs the whole quest directly, for testing | n/a | `RunLostTreasure.txt` |
| Unused clues of the earlier design, in the text table but said by no script: "Place in the ring a cunning hunter." / "And a bush of fire if you want to." / "A lump your Creature leaves behind." / "And protect with magic hand and mind." | n/a | unused text |
| Unused step-by-step clues: "You need to place a cunning hunter into the ring of life." / "Now add to the ring of life, the Burning Bush." / "Next a lump of your Creature's waste products must be placed into the ring." / "Finally, protect the objects you placed into the ring with a Shield." and the hints "You may find one drinking fresh water after dark." / "You will find it between the snow and the sea."; an alternative opening "I have a legend for you. Solve it and you'll gain great power." | n/a | unused text; the wolves drinking at the water's edge at night match the first hint |
| Unused progress lines: good "Excellent. Something's happening.", evil "Let's get the next clue from that woman!", good "This is getting interesting!", good "We're getting closer! There can't be much more to this.", evil "Something's happening!", good "The wolf's escaped.", good "Well that's pretty unique. I suppose.", evil "The object's no longer hot, Boss." | n/a | unused text: the shipped quest has no per-step feedback |
