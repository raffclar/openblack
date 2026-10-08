# Nemesis's Shielded Village

Land 5's story beats around Nemesis's defences: the Tibetan village whose Wonder curses the creature is hidden under a
spiritual shield held up by three worshippers praying at singing stones, which the player must scare off, kill or
unseat; Nemesis's own last village fires burning meteors and raises physical shields at a creature that comes near; and
once the creature is healed, Nemesis sends barrages of blasts at the player's villages and citadel until his last
village falls. None of this is a logged quest: no script makes a scroll or a challenge-log entry for it, and the game's
text table has no title for it, so this file is named after what it is.

**Land:** 5 · **Giver:** none (the evil advisor warns of the shield when the player first nears the Tibetan village) · **Script:** ThrowThroughShield (challenge THROW_THROUGH_SHIELD), and NemesisDefendHisLastTown, MakesSmokeComeOutOfMeteor and ThrowBeamBlock in the land's control script LandControl5 · **Reward:** none (bringing the shield down lets the Tibetan village be won, the third step of the curse) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, which matches the PC game's compiled `challenge.chl`)
and the game's text table (`Scripts/InfoScript2.txt`) for every line and its speaker. openblack's state is judged on
the physics work tree (`ob-wt-physics`): the story's top script always runs Land 1's control script first and the
land-loading command (`LoadMap` in `src/CHLApi.cpp`) has an empty body, so Land 5's control script and everything here
never runs. Every row is todo unless its notes say a working command would do that part. The curse these defences
guard is in [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md), the fight that follows in
[so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md); the land as a whole is in
[../land_5.md](../land_5.md).

**Progress: 0/46 done, 8 partial — 9%**

## Is it a gold scroll?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield script logs nothing: no scroll on the map, no challenge-log entry, no progress, and no title; its challenge name only groups the script | todo | checked: the script has no highlight and no snapshot; `CreateHighlight`/`Snapshot` are stubs anyway |
| It is part of the story all the same: the Tibetan village it guards holds the third Wonder, and winning that village is one of the three steps that lift the curse | todo | [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |
| The land's control script starts it in the background as the land begins, together with the curse and Nemesis's town defences | todo | the land's control script never runs in openblack |

## The shield: how it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the land begins, three singing stones are put at three spots away from the village (one on a hill to the east, one on a hill to its south, one far north near the player's home) and a fourth at the shield's centre over the village | partial | `Create` makes mobile statics, so the stones would appear; the script never runs |
| The shield goes up when the player first has any belief in the Tibetan village, or the creature comes within 300 of the shield's centre | todo | `BeliefForPlayer` is a stub |
| The three outer stones are then made indestructible | partial | `SetIndestructable` works |
| It only goes up if the village is not already the player's and at least one outer stone is still where it was put (within 1); otherwise nothing at all happens | todo | |

## The shield film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film with spooky music: the camera turns to the village over 4 seconds; the evil advisor points at it: "Boss, I don't mean to worry you but some serious defences are going up here." | todo | `StartMusic` works; camera moves and advisors are stubs |
| Three Egyptian worshippers are made by the stones (a forester, a trader and a housewife), each facing his stone | todo | villagers are not made by `Create` in openblack |
| For each stone still in place, its worshipper's guard script starts and the camera flies past for 10 seconds: first worshipper "By the almighty powers, we will protect our town.", second "Nemesis, ever generous, has granted us a Spiritual Shield.", third the first line again | todo | the third worshipper's own line was dropped as a bad recording (see Cut parts) |
| The camera rises high over the village (10 seconds); a magic beam runs down from 80 above the centre into the village, two flashes, and the spiritual shield is cast over the village, endless, its radius 35 for each stone that is working (up to 105) | partial | the shield is cast by `SpellAtPos` ([../../miracles/spiritual_shield.md](../../miracles/spiritual_shield.md)); beams and flashes are stubs |
| Both advisors step out: evil "These three are the source of the Shield. We gotta knock them out.", good "Their strong belief in Nemesis won't make that easy.", evil "Kill them! Smash their stones. Yeah!", good "Or we could just break their concentration.", evil "Oh yeah. Break their concentration with a boulder in the face!"; the music stops | todo | |

## The worshippers and their stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each worshipper does a mocking dance by his stone | todo | villager animations are stubs |
| A stone is working while its worshipper is alive and within 1 of where he started, and the stone is within 1 of its spot: it chants (a Tibetan chanting sound, one of three, tied to him) and a magic beam runs from 5 above it to the top of the shield | todo | 3D sound tags and beam effects are stubs |
| When either moves away the chant and beam stop; they come back when both are in place again | todo | |
| Every 3 seconds a working stone sparkles; if the creature is within 100 of it and not invisible, a lightning strike leaps from the stone at the creature's head and either side of it, then the creature is let go after 3 seconds | todo | the strike is a visual effect; whether it hurts the creature is not determined (the real lightning cast is commented out) |
| Every 3 seconds the worshipper checks for near misses within 40: a flying rock or tree (not one the script made) or a fireball. If one is close he ducks, runs to a hiding spot, looks round for 45 seconds, then walks back and dances again | todo | |
| Killing a worshipper switches his stone off for good; a stone also stays off while it is out of place | todo | the stones are indestructible, so "Smash their stones" cannot work, but moving one off its spot does |
| The shield shrinks or grows by 35 each time a stone stops or starts | todo | `SetMagicRadius` is a stub |

## Bringing it down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield falls the moment all three stones are off at once; a scared worshipper's 45 seconds of hiding are the window to deal with the others | todo | |
| The shield and the beam are deleted and the worshippers' guard scripts end, so they never put it back up | todo | `ObjectDelete` is a stub |
| If all three worshippers still exist the good advisor says "We did it! The Shield's down and no one got killed."; otherwise the evil advisor: "Sweet, Boss. One dead Shield and a few human casualties. Couldn't have done better myself." | todo | |
| With the shield down the Tibetan village can be impressed or conquered; its first capture blasts its Wonder and logs the curse's step | todo | [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |
| It cannot soft-lock: a worshipper can always be killed, and nothing restarts the shield once it has fallen | todo | |

## Nemesis's last village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start of the land, whenever the creature is within 200 of a point by Nemesis's village, a burning meteor is fired at it every 6 seconds (the first after 10) | todo | `CreateTimer`/`GetTimerTimeRemaining` are stubs |
| Each meteor is a half-size rock made 250 up the mountain behind the village, set on fire, heated to 2000, kept out of the wind and thrown to land in 6 seconds within 30 of a fixed spot west of the village (not at the creature) | partial | `CreateWithAngleAndScale` makes rocks, `SetOnFire` and `SetTemperature` work; `SetAffectedByWind` and `SetTarget` are stubs; burning rocks: [../../nature/rocks_splitting_and_heat.md](../../nature/rocks_splitting_and_heat.md) |
| While it flies and is not held it trails a huge smoke plume (a bonfire effect eight times normal size, renewed constantly) | todo | `SpecialEffectPosition` is a stub |
| 6 seconds after launch, once it is neither held nor flying, a fireball is cast just by it and it is deleted with an explosion; a meteor caught by the hand explodes once it is let go and has landed | partial | the fireball is cast by `SpellAtPos` ([../../miracles/fireball.md](../../miracles/fireball.md)); deleting with an explosion is a stub |
| Every 30 seconds while the creature is near, a physical shield (radius 75, 30 seconds) is raised over Nemesis's village | partial | cast by `SpellAtPos` ([../../miracles/physical_shield.md](../../miracles/physical_shield.md)); the timer is a stub |
| These defences run until Nemesis's last village is won | todo | |
| While the curse is on the village cannot be won (the player's belief there is reset whenever it reaches 0.1) | todo | [i_have_a_surprise_for_you.md](i_have_a_surprise_for_you.md) |
| After the healing scene, it can be won only while Nemesis holds none of the land's other villages (the neutral Japanese one included): whenever he holds one, the player's belief there is set back to 0 | todo | `SetPlayerBelief` is a stub |
| The first time he holds one again: good "Leader! Nemesis has retaken one of our Villages!", evil "Aw, shoot. Everything's lost unless we get it back. Come on, Boss."; when he holds none again: evil "Boss! We can conquer Nemesis' last town! We got the power. I can sense it!", and the warning can then come again | todo | the "we can conquer" line only plays after such a loss and recovery, never the first time the player holds the other villages |
| Winning it ends the land's loop, stops every other Land 5 script and starts the big fight | todo | [so_this_is_a_fight_to_the_death.md](so_this_is_a_fight_to_the_death.md), [../ending.md](../ending.md) |

## Nemesis's blast barrages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the healing scene, every 15 seconds one of four targets is picked at random: the Aztec village, the Greek village, a spot south-east of the Tibetan village, or the player's citadel; the three villages are only attacked if the player owns them, the citadel always | todo | `Random` works; the rest is stubbed |
| A barrage is a line of blasts marching from a start point by Nemesis's land towards the target: each step is a row of seven blasts (one in the line and three either side, 20 apart), each a level-one blast (radius 50, 15 seconds) dropped from 30 above | partial | each blast is cast by `SpellAtPos` ([../../miracles/blast.md](../../miracles/blast.md)) |
| Village barrages take 20 steps, 2 seconds apart; the citadel barrage takes 10 steps, 1 second apart, from a closer start | todo | |
| The steps start at the start point and stop one step short of the target | todo | from the step count and spacing in the script |
| The barrages end when Nemesis's last village is won | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield film plays the spooky script theme | partial | `StartMusic` works; nothing reaches it |
| The three working stones chant (three Tibetan chanting recordings, one per worshipper) | todo | 3D sound tags are stubs |

## Script quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The shield's main loop, Nemesis's defence loop and the barrage loop have no pauses; the shield loop makes a flash at the top of the shield on every pass | todo | how the virtual machine paces a loop with no wait is not determined here |
| A new near-miss check is started every 3 seconds even while an earlier one is still sending the worshipper to hide, so two can move him at once | todo | |
| If the player moves all three stones off their spots before the trigger, the shield never goes up and nothing is said | todo | |
| The shield's creature check at the centre stone does nothing: the reaction it was meant to start (the creature running away) is commented out | n/a | never in the game |
| The control script still holds a comment that the shield script replaced the Tibetan village's old lines, though its capture film is still there | n/a | nothing to do |

## Unused and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The third worshipper's own line "No one shall harm us or test our faith." was dropped as a bad recording (it is filed under the good advisor's voice) | n/a | never in the game |
| A fuller version had shouts when worshippers gave up or were hit ("Argh! I'm giving up!", "Ow! I give up!", "I've had enough. This is too dangerous.", screams), advisors' comments on each ("Perfect. He survived. Let's move quickly before he recovers.", "I spy a dead guy", "He's toast.", "You iced him, Boss.", "You got him, but the guy ran off.", "He'll be back sooner or later to reactivate the Shield, though.") and "One of the rays has been reactivated!"; no script says them | n/a | never in the game |
| A battle plan for Nemesis is compiled into the game but nothing starts it: every 30 seconds it would have had him defend his own village when the creature or camera came within 250, destroy villages the player had won, impress villages the player was gaining, and attack the creature with spells when it was outside the player's influence | n/a | never started by the game; see [../../rival_gods/nemesis.md](../../rival_gods/nemesis.md) |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can break the shield by throwing rocks or trees near the worshippers, or by killing them; a working stone strikes at it with lightning when it is within 100 unless it is invisible | todo | creature throwing: [../../creature/object_actions.md](../../creature/object_actions.md) |
| Bringing the creature within 200 of Nemesis's village draws his meteors and shields | todo | |
