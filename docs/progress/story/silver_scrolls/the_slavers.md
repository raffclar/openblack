# The Slavers

A land 2 silver scroll: once the player wins the Greek village south-west of Khazar's home, a villager tells of missing
people, and five "travelling folk" camped on the hill above turn out to be slavers holding eight villagers in a pen. They
want wild animals for a circus and pay for them in slaves; stealing from them earns warnings and then a raid on the
village, and killing one of them starts the raid at once. A cut bronze scroll, the Slavers Warning, was meant to have an
Indian fisherman warn of kidnapped women; it is covered at the end of this file.

**Land:** 2 · **Giver:** a villager of the Greek village (town id 2), then the slavers' chief at their camp ·
**Script:** TheSlavers · **Reward:** a ground flock (wolf pack) miracle dispenser at the slavers' camp, plus the eight
freed slaves and the five slavers joining the village · **Repeatable:** no

Sources: the quest's script source (`TheSlavers.txt`), checked against the PC game's compiled `challenge.chl` (argument
orders, the loop exits and the and/or order of the theft test), the land's control script (`LandControl2.txt`) for the
trigger, the shared helpers it runs (the scroll notice, the standard reminder, the dispenser reward), the land's map
script (`Land2.txt`) for the town, and the game's English text table. The cut warning scroll is `SlaversWarning.txt`.
openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 86 script functions the quest and the
scripts it starts need, 66 only log "not implemented" in `src/CHLApi.cpp`; the 20 that work include making mobile statics
(the camp fire and the fences), the fire flags, setting a thing alight, script music and stopping scripts. The land's
control script stops at its first unwritten function long before the quest's trigger runs (see
[../land_2.md](../land_2.md)), so the quest never appears; every row is todo unless the notes say otherwise. The land is
in [../land_2.md](../land_2.md), the script program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/104 done, 11 partial — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script watches the Greek village (town id 2, centre near 2249, 2240) from the land's start, checking every 1.7 seconds whether it belongs to the player; the first time it does, the quest starts in the background and the watch ends | todo | `GetProperty` (the town's player) is a stub; the control script never gets this far |
| That village starts the land as Khazar's (player two) and is Greek in the land's map script, though the control script's comment calls it "Norse near Khazar"; the player must win it over before the quest exists | todo | the map script's town list loads (`TownArchetype`), but the quest's watch never runs |
| No other condition: no other town, timer or story step is needed, and the quest can start at any time the village is won | todo | |
| A silver (challenge) scroll is put 4 above the house at a fixed spot west of the village centre (near 2183, 2210) | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and it is on screen, the good advisor pops out, points at it and says "We've got something to do. Let's see what it is.", at most every 30 seconds and only when a widescreen scene can start | todo | the shared scroll-notice script; `GameThingFieldOfView`, `SpiritEject`, `SpiritPointPos` are stubs |
| Clicking either the scroll or the house starts the quest; the scroll is then made active | todo | `GameThingClicked`, `SetActive` are stubs |
| A test-menu script (start the quest straight on land 2) exists in the sources but is not in the shipped challenge file | n/a | developer test harness only; nothing to match |

## The slavers' camp

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the click a slave flock is made at the slave pen (2235, 1974; inner 5, outer 10) and an empty, calm circus flock at the animal pen (2276, 2002; inner 5, outer 10), on the hill about 265 south of the village | todo | `FlockCreate`, `ChangeInnerOuterProperties` are stubs |
| An Egyptian pyramid, turned 185 degrees, stands as the circus 57 south of the camp (2254, 1918) | todo | `CREATE_WITH_ANGLE_AND_SCALE` works only for mobile statics and rocks; features are not made |
| A bonfire burns at the camp's centre (2256, 1975) | partial | `Create` makes the bonfire (`CreateScriptObject` → `MobileStaticArchetype`), but the script never reaches it |
| The animal pen is a ring of eight short Celtic fences 10 from its centre, each turned to face the centre and then a quarter turn | partial | the fences are made by `Create`, but turning them needs `SetFocus` and the angle property (stubs) |
| The animal pen alone carries an influence ring of radius 20, so the player's hand can reach into it although the camp is outside the player's influence | todo | `InfluencePosition` is a stub |
| The slave pen is a smaller ring of eight fences 6.5 from its centre | partial | as the animal pen |
| Eight slaves are made one by one (a fifth to two thirds of a second apart): three Greek housewives, a farmer, two fishermen, a forester aged 8 and a shepherd; each faces the pen centre, joins the slave flock and mourns on a loop | todo | creating villagers is not implemented in `CreateScriptObject`; `FlockAttach` stub |
| A flock of five wild lions (inner 5, outer 30) is made about 185 east of the animal pen (2456, 2047) and let go, a ready supply of rare beasts | todo | `PopulateContainer`, `ReleaseFromScript` stubs |
| Five slavers (all Egyptian shepherd men) appear at the camp: four round the fire and the chief at its centre; none can be hurt by fire or set on fire, and all join the slaver posse (inner 5, outer 10) | partial | the two fire flags work (`SetHurtByFire`, `SetSetOnFire` in `src/CHLApi.cpp` through the fire system), but the slavers are never made |
| After the introduction the circus pyramid plays the circus music in 3D (heard near it) | todo | `AttachMusic` is a stub; the pyramid is not made |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene starts; a Greek farmer is made inside the house, walks out very slowly (speed 0.1) in high detail, and the circus music starts | partial | `StartMusic` plays the circus theme (`GameMusic::StartScriptMusic`); the scene, villager and detail are stubs (`StartCameraControl`, `SetHighGraphicsDetail`) |
| The slavers are made during the scene and the chief walks to a spot by the fire (2264, 1994) | todo | |
| The camera flies to the house (5 seconds, focus 3 seconds); the villager turns to the camera and the camera closes in on him over 6 seconds | todo | `MoveCameraPosition`, `MoveCameraFocus`, `SetFocus` stubs |
| He talks: "Some of our Villagers are missing and I believe those strangers are responsible, mighty Leader." | todo | `RunText` stub |
| Three seconds in, he turns to the camp, points and talks with three pointing gestures, then the line closes | todo | `Played`, `TextRead`, `GameCloseDialogue` stubs |
| The camera climbs over the hill to a view of the camp (6 seconds), then 4 seconds later swoops to the chief in high detail; he turns to the camera, which closes in slowly over 12 seconds | todo | |
| The scroll is recorded in the challenge log: a picture of the scene, titled "The Slavers", with no progress and no alignment yet (success 0, alignment 0), and the reminder "Villagers are going missing. Perhaps the Slavers are involved." | todo | `Snapshot` is a stub; the log keeps the title and the reminder script for the temple |
| The chief, gossiping: "Greetings. We are simple travelling folk and wish to start a circus." | todo | |
| Waiting impatiently: "If you give us animals to train we will be able to entertain all the Villages in this area." | todo | |
| The camera moves over the animal pen; the chief, as if holding a meeting: "Put wild beasts into this pen and we will set free our friends over here." Two seconds in, the camera swings round to the camp and slave pen | todo | |
| The farmer fades away (he is deleted, not sent home); the camera returns to where it was (5 seconds), the music stops and the scene ends | partial | `StopMusic` works; `ObjectDelete` with fade is a stub |
| From then on the chief tends the camp: walks to beside the fire, sits for ten loops, gets up, walks 10 off to look about, and repeats (each walk gives up after 10 seconds) | todo | `MoveGameThing`, `SetScriptState` stubs |
| The other four keep their own rounds: one paces by the slave pen looking about and looking overworked; one checks the prisoners, yawns, goes towards the pyramid, stares at the camera and pokes the fire, resting 10 seconds; one sits by a tree and then pokes the fire; one waits impatiently by the animal pen, looks at the camera near the pyramid, yawns and stands by the fire 15 seconds | todo | every walk gives up after 5 (slave-pen guard) or 20 seconds; each round ends when its slaver dies or the slavers are "finished" |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bring live wild animals to the animal pen (by hand, or by any other means) until all eight slaves are paid out | todo | the quest's main loop runs every 0.3 seconds; nothing else is asked |
| An animal counts once it is within 10 of the pen's centre, is not under script control, and is on screen; the trade waits until the camera sees it | todo | `CallNear` (the nearest animal), `GameThingFieldOfView` stubs |
| No time limit; the slavers wait for ever | todo | |

## Trading animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene starts; the chief's round stops, the camera moves over the pen (3 seconds), the animal is frozen with a success sparkle on it for 5 seconds, and the chief walks to a spot by the pen (giving up after 5 seconds), faces it and points | partial | stopping the chief's round works (`StopScript`); the rest are stubs (`SpecialEffectPosition`, `SetScriptState`) |
| A dead animal: one of two lines at even odds, "That animal's dead. It's useless! Get out of here!" or "That animal's dead. I'm not paying for it."; a magic beam from the chief strikes it for 2 seconds and it fades away | todo | `Random` works; `SpecialEffectObject`, `AddSpotVisualTargetObject`, `ObjectDelete` stubs |
| Rare beasts (lion, tiger, wolf, leopard, zebra) are worth two slaves: "Wow. That's a rare animal. It's worth two slaves." | todo | `GameSubType` stub |
| Tortoises and horses are worth one slave: "That's a healthy beast. We'll give you a slave for it." | todo | |
| Only the first two of each kind are paid for; from the third: "We've got plenty of this species, Holy One. So no payment, I'm afraid." and the beast is beamed away | todo | the count for a kind rises with every one brought, paid or not |
| Every other animal (sheep, goats, cows, pigs and the rest): "What do you call that? We don't want it." and it is beamed away and destroyed | todo | the comment lists sheep, goat, cow and pig as worth nothing |
| A paid-for animal joins the circus flock and stays there under script control | todo | `FlockAttach`, `SetScriptState` stubs |
| Payment: the camera cuts to a view of the slave pen and drifts for 3 seconds; that many slaves leave the flock (fewer if fewer are left), each after half a second to a second and a half walks slowly (speed 0.5) to the village, joins it and is let go 20 seconds later | todo | `FlockDetach`, `MoveGameThing`, `ReleaseFromScript` stubs |
| Three seconds after the camera settles, the log entry is recorded again with success of one tenth per slave gone from the pen (stolen ones included) and alignment +0.1, reminder as before | todo | `Snapshot` stub; success = (8 − slaves left) × 0.1 |
| The camera returns (3 seconds), the scene ends and the chief goes back to his round | todo | |
| The most the slavers would pay is 24 slaves (five rare kinds × 2 × 2, two plain kinds × 2 × 1), far more than the eight they hold; the five lions nearby alone can free four | todo | |

## Stealing and warnings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A slave counts as stolen when one is more than 10 from the slave pen's centre and not held by the hand or the creature and not flying, or whenever the slave flock has shrunk since the last look (so a slave that leaves the flock any way, such as by dying, counts) | todo | `CallInNotNear`, `IdSize`, `InCreatureHand` stubs; checked in the compiled program: the test is "(outside, not held, not in the creature's hand, not flying) or the flock shrank". Undetermined: whether picking up a villager takes it out of its flock in the engine |
| A stolen slave that is loose (not held or flying) is taken out of the slave flock, joins the village and is let go | todo | |
| An animal counts as stolen whenever the circus flock has shrunk since the last look (the script notes that a picked-up animal leaves its flock); the kinds kept are then recounted, so a stolen kind can be paid for again | todo | a circus animal that dies also shrinks the flock |
| First slave theft: a widescreen scene closes on the chief (4 seconds), the log is updated to success 0.1, alignment +0.5, reminder "If we give the Slavers animals, we might get our people back."; he: "Hey! Don't steal our slaves or there'll be trouble." | todo | `UpdateSnapshot` stub |
| First animal theft: the same close-up with a 55 lens; the log is updated to success 0, alignment −0.4, same reminder; he: "Hey! Stop stealing our beasts or there'll be trouble." | todo | `MoveCameraLens` stub |
| A second theft of the same kind: a final warning with a 55 lens, log updated to success 0.2, alignment +0.5, same reminder; he, waiting impatiently: "We saw that! Steal one more thing from us and we'll take action." | todo | |
| Slave and animal thefts are counted apart, but the final warning is given only once; after it, any theft of either kind declares war | todo | so a slave, an animal and a slave again gives three different scenes; an animal twice then a slave goes straight to war |
| After each warning the camera returns (4 seconds, focus 3) and the chief goes back to his round | todo | |

## Killing a slaver, and war

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the posse ever has fewer than five slavers (one killed, by the player, the creature or anything else), the trading ends for good and the evil advisor pops out: "Neat. You killed a slaver. " | todo | `IdSize`, `SpiritEject` stubs; undetermined whether a slaver picked up leaves the posse (and so counts) |
| The posse then gathers as a flock and every slaver's round ends | todo | |
| War scene: the chief (or, if he is dead, another slaver) is closed in on with a 55 lens in high detail and, unimpressed: "Very well. So be it. It's war, then!"; two seconds later the camera returns | todo | the same scene follows a theft after the final warning |
| The circus animals (if any) are driven at half speed to the village and set loose (disbanded) once within 15 of it | todo | `FlockDisband` stub |
| The silver scroll moves into the village (2233, 2231) | todo | `SetPosition` works, but the scroll is never made (`CreateHighlight` stub) |
| The log is updated to success 0.5, alignment +0.2, reminder "The Slavers are taking revenge on the Village!" | todo | |
| The slavers march as a flock at half speed to the village centre until within 15 of it (or all dead) | todo | |
| On arrival they stop; the nearest house within 50 is set alight (burn speed 0.5) | partial | setting alight works (`SetOnFire` through the fire system), but finding the house (`CallNear`) is a stub |
| The good advisor pops out pointing at them: "Oh dear. The slavers are killing people in that Village!" and, if the circus had any animals when the march began, "They've unleashed their animals on the people as well! ", then goes home | todo | `SpiritHome` stub |
| Each slaver then hunts the village's people (not those under script control): the nearest within 100 of him, else anyone in the village, else a child | todo | |
| The victim stands scared stiff; the slaver walks to within 15, plays the summoning gesture twice and strikes with a magic beam for 10 seconds; the victim crawls injured and lies dying | todo | |
| If 10 seconds pass undisturbed the victim dies and is counted; the slaver walks to the body, inspects it three times and rests 15 seconds before the next | todo | about one killing per slaver every half minute |
| Picking up the slaver or the victim, the slaver being in the creature's hand or thrown through the air, or the victim vanishing saves the victim at any step; the victim is let go | todo | `IN_CREATURE_HAND` and the held/flying properties are stubs |
| A slaver keeps killing until he dies | todo | |
| If all slavers die (on the march or in the village), the log is updated to success 1.0, alignment −0.65 | todo | the follow-up script has only comments in it (slaves cheering and joining the town were planned), so nothing else happens |
| If they kill 10 people, or the village has no people left, every slaver stops killing and they stand where they are, frozen; the log is updated to success 1.0, alignment −1.0 | todo | `StopScript` (stopping all killers by name) works; the rest are stubs. The source's plan says they would "kill 10 people before fleeing"; fleeing was never written |

## Success and the circus

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is won after any trade that leaves the slave pen empty; all five slavers leave the posse | todo | the check is made only after a trade (see quirks) |
| A widescreen scene: the circus animals are put together at one spot; the good advisor appears: "You've freed the slaves. They're joining us as worshippers!" and disappears | todo | `SpiritAppear`, `SpiritDisappear` stubs |
| The circus music starts; the four slavers are placed in a group facing a point and the chief beside them; the camera cuts to the chief | partial | `StartMusic`, `SetPosition` and `SetCameraPosition`/`Focus` work; the scene and slavers don't |
| The chief, thanking: "We've got all the animals we need, now." | todo | |
| One slaver blows raspberries at another, two cheer and one dances (four loops each) while the camera pans over the group (5 seconds) and back to the chief (3 seconds) | todo | |
| The chief: "We'll get this circus together in no time." with gossip and meeting gestures, then the line closes | todo | |
| A drum roll starts; three slavers stand on the fourth's shoulders at heights 1.2, 2.4 and 3.6, all facing the chief; the top man strikes the "Titanic" pose on a loop | todo | `PlaySoundEffect` stub; altitude property |
| The camera pans across the human tower (5 seconds) and up to the top man (6 seconds); after 4.2 seconds the drum roll stops and a cymbal crash plays | todo | `StopSoundEffect` stub |
| Two town fireworks go off for 60 seconds either side of the camp | todo | `SpecialEffectPosition` stub |
| The log is recorded: success 1.0, alignment +1.0 | todo | |
| The top man turns to the base man; the music stops and the camera pulls back over the camp (6 seconds) | partial | `StopMusic` works |
| Five seconds later all five slavers join the village and are put back on the ground; only the chief is released from script control | todo | `FlockAttach`, `ReleaseFromScript` stubs; undetermined what the other four do afterwards (never released, and the top man's pose was set to loop) |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Norse miracle dispenser of the ground flock miracle (a pack of wolves) is built at the camp (2252, 1954), turned 180 degrees, switched on, refilling every 120 seconds | todo | `MagicSystem::CreateDispenser` exists but the script commands are stubs; see [../rewards.md](../rewards.md) and [../../miracles/flocks.md](../../miracles/flocks.md) |
| The reward sting plays and the camera flies to the dispenser; the evil advisor explains it (the full explanation and a signpost if it is the player's first dispenser, otherwise "another dispenser" remark), then its help lines | todo | the shared dispenser reward; see [../rewards.md](../rewards.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After success: the eight freed people and the five slavers belong to the village; the circus animals stay at the camp under script control; the pyramid, fire, fences and fireworks' spot remain | todo | |
| After a war: the circus animals run wild in the village; the slaves stay in their pen mourning, never freed or released; the burning house burns | todo | the empty "all slavers dead" script was meant to free them |
| The silver scroll is never removed by the script (it stays on the house, or in the village after a war) | todo | undetermined whether the engine takes down a finished scroll |

## Advisors' comments

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good advisor: the scroll notice "We've got something to do. Let's see what it is." | todo | |
| The reminders (spoken by the good advisor, who steps out to say them): "Villagers are going missing. Perhaps the Slavers are involved." (start, trades, endings), "If we give the Slavers animals, we might get our people back." (warnings), "The Slavers are taking revenge on the Village!" (war) | todo | the standard reminder script |
| Evil advisor: "Neat. You killed a slaver. " | todo | |
| Good advisor: "Oh dear. The slavers are killing people in that Village!" / "They've unleashed their animals on the people as well! " | todo | |
| Good advisor: "You've freed the slaves. They're joining us as worshippers!" | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circus theme plays over the introduction and over the circus finale | partial | `StartMusic`/`StopMusic` work (`MUSIC_TYPE_SCRIPT_CIRCUS` in `src/Audio/GameMusic.cpp`) |
| The 3D circus music is fixed to the pyramid after the introduction | todo | `AttachMusic` stub; the 3D bank is listed in `src/Audio/GameMusic.cpp` |
| Drum roll and cymbal crash for the human tower | todo | `PlaySoundEffect`, `StopSoundEffect` stubs |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing asks for the creature, but it counts like the hand: a slave in its hand is not loose, and a slaver in its hand cannot kill; a slaver the creature kills starts the war like any other | todo | |
| The creature can carry animals to the pen or eat circus animals (which counts as theft); no special handling | todo | inferred from the checks, which look only at flock sizes and positions |

## Script bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| War declared by theft never marks the slavers as finished: the main loop keeps running, so later thefts replay "It's war, then!" and start a second march, a killed slaver adds "Neat. You killed a slaver." and a third, the animal pen still trades (calling the chief back to the pen), and the four slavers' camp rounds keep sending them back to their camp spots while they march | todo | undetermined how the engine settles a slaver given both camp and attack orders |
| Each new attack resets the kill count to zero on arrival | todo | |
| The win is checked only after a trade: if the last slaves are stolen instead, the quest waits for the next animal brought to the pen, paid or not, and then plays the circus finale | todo | |
| The warning scenes record success 0, 0.1 or 0.2, overwriting the progress earned by trading | todo | |
| The slave warning keeps whatever camera lens was last set, unlike the other two warnings (55) | todo | |
| The dead-animal line "That animal's dead. It's useless! Get out of here!" is not waited for, so the beam starts while it is still on screen | todo | the other line is waited for |
| The kind counts are shared by all scripts and rise with every animal brought, so after a recount (theft) the paid-for limit applies to animals still in the flock only | todo | |
| The slaver victims include anyone of the village, even the freed slaves who joined it | todo | |
| Comments disagree with the code: the slaves are said to be Celts (they are Greek), the den script says it makes "both animal flocks" (one, of lions), and the land's control script calls the town Norse (it is Greek) | todo | |
| The posse test is "fewer than five", so a slaver taken from the posse any other way would also count as killed | todo | undetermined what removes a villager from a flock besides death |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Seven slaver lines exist but no script says them: "Good grief. You killed all the slave traders." (good), "You've murdered all the slaves. Dreadful." (evil), "The Slavers have gone." (good), "Hmm. Those slavers are up to something." (evil; the text keeps a stray "TA strokebeard]" stage direction), "The Slavers have kidnapped all the women from the Village!" (evil), "The Slavers are attacking the Village!" (good) and the chief's "Hey. You're our millionth customer. Have all our slaves free!" | n/a | cut: never said in the shipped game |
| The "all slavers dead" script is empty (comments plan cheering slaves released to the town and a "Slavers dead" line); the "killed enough" script's case for a dead chief is an empty block with a note to "do something" and attach slavers and slaves to the town | n/a | cut: the shipped game does nothing there |
| The circus finale has a note to make the slavers do tricks with the animals; the camp builder's comment hoped for better cages made in the editor | n/a | cut |

## The Slavers Warning (cut bronze scroll)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The warning is compiled into the shipped challenge file, but nothing starts it: no land control script, no other script and the file's own header run it (it has no start-up line), so it never appears in the game | n/a | searched every script source and the compiled program's start-up list; only its own definition exists |
| It was set at the Indian village (town id 10) by the shore: a bronze (tips) scroll by a hut (2821, 3052), about 80 from the village centre and just east of the Baywatch quest's hut | n/a | cut |
| Its notice would have sent the evil advisor out pointing with "Your godly attention is required here, Leader." (a line the text table gives to the good advisor) until the scroll or hut was clicked | n/a | cut; the shared scroll-notice script |
| An Indian fisherman would come out of the hut and walk to a spot 14 in front; the camera flies to him (position 4 seconds, focus 2), and once he arrives he turns to the camera | n/a | cut |
| He says: "A band of foreigners have taken some of our womenfolk!" / "They kidnapped them when we were out hunting!" / "We tried to track them but they're long gone now." / "Three women were taken. Please, holiest, look out for them." | n/a | cut; the source's comments carry an older draft ("Strange foreigners have kidnapped our wives!" and so on) |
| The camera returns over 3 seconds, but a typo moves the camera's position twice (to the old position, then to the old focus point) and never restores its focus; the fisherman walks home and is let go | n/a | cut; the same in the compiled program |
| No log entry, reward or link to the Slavers quest: a closing comment only hopes to check whether "the Indian Slave women" are returned once the Slavers quest has begun. The Slavers' slaves are Greek, three of them women, and the quest's own introduction already has a villager warn of the missing people | n/a | cut |
