# Choose Your Creature

The first land's first creature chapter: Sable, the creature trainer, challenges the player to open the gates of the
creatures' glade with three gate stones; the first is the stone the villagers danced round, the second is earned in
[The Lost Brother](the_lost_brother.md), the third is carved by [The Sculptor](the_sculptor.md). With all three on the
platform the gates open, the cow, the ape and the tiger show off, and the player picks one by clicking it twice.

**Land:** 1 · **Giver:** Sable, the creature trainer, at the creature gates north of the Norse village · **Script:** ChooseYourCreature (with CreaturesInGlade and the gate-stone guards in ProtectGateKeys) · **Reward:** the player's creature (cow, ape or tiger); no alignment change · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table, the land file `Land1.txt` and the executable (how a gate stone fits into the
platform). openblack's state is judged on the physics work tree (`ob-wt-physics`): the land's control script never
reaches this quest (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and nearly every command it
needs only logs "not implemented" in `src/CHLApi.cpp`, so every row is todo unless the notes say otherwise. How a
creature is chosen and what the species differ in: [../../creature/species_choice.md](../../creature/species_choice.md).

**Progress: 1/90 done, 11 partial — 7%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold scroll (a story highlight), logged as "Choose Your Creature" | todo | `CreateHighlight`, `Snapshot`, `UpdateSnapshot` are stubs in `src/CHLApi.cpp` |
| It starts once the opening is over: after the family has led the player home and the temple scene is done, and after the Throwing Stones and Lost Flock silver scrolls have been set going | todo | the land's control script; see [../land_1.md](../land_1.md), [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md) |
| The land's control script waits for the whole quest (all three stones and the choice) before going on | todo | |
| What it unlocks next: the creature breeder, the Explorers and the guide's wandering are started, then the trainer's first lesson (seeing the creature's home pen) begins | todo | [the_creatures_learning.md](the_creatures_learning.md); [../creature_guide.md](../creature_guide.md); [../silver_scrolls/the_creature_breeder.md](../silver_scrolls/the_creature_breeder.md); [../silver_scrolls/the_explorers.md](../silver_scrolls/the_explorers.md) (waits for the choice) |
| The Lost Brother is started by this quest when the first stone is placed, and The Sculptor when the second is | todo | [the_lost_brother.md](the_lost_brother.md), [the_sculptor.md](the_sculptor.md) |
| The scroll's progress mark and reminder change at every step: 0 at the start (reminder "Didn't we see the Villagers dancing around a Stone a while ago?"), 0.25 with one stone ("We need to find another Gate Stone. Perhaps the Villagers can help."), 0.5 with two ("We should speak to the sculptor about the Gate Stones."), 0.75 in the glade and 1 when a creature is chosen ("We need to choose a Creature!") | todo | |
| Each reminder is said by whichever advisor owns the line (all of these are the good advisor's), stepping out to say it | todo | the shared reminder script; advisors are stubs (`SpiritEject`) |

## The scroll appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze did-you-know scroll is placed beside the gate stone platform: "Every time you activate a Gold Story Scroll you start the next chapter of the story of Black and White. How quickly you progress depends on how often you choose to activate them." | todo | `CreateHighlight`, `HighlightProperties` stubs |
| A gold scroll appears in front of the creature gates, raised 14 above the ground | todo | |
| Whenever the camera is within 100 of the scroll and it is on screen, the evil advisor steps out, points at it and says "There's a Gold Story Scroll down here, Boss.", at most once every 30 seconds and only when no cut-scene is running | todo | the shared notify script; `SpiritPointPos`, `RunText` stubs |
| The nagging stops when the scroll (or the place it stands) is clicked | todo | `GameThingClicked` is a stub |
| Nothing ever removes the gold scroll in this script once it is clicked (undetermined: whether the game hides a story scroll by itself once its chapter is logged as finished) | todo | |

## The trainer's challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cut-scene with the creature guide's theme; the good advisor is sent home | partial | the music command plays the track (`StartMusic`, `src/Audio/GameMusic.cpp`); the cut-scene, villager and advisor commands are stubs |
| Sable, the trainer, is made inside the hut by the gates, walks out of its door to a spot before the camera and turns to face it, the camera gliding down to her and following her | todo | `CreateScriptObject` makes no villagers; `MoveCameraPosition`, `FocusFollow`, `MoveGameThing` stubs |
| The quest is logged in the story's log at this point, with a picture, at 0 progress | todo | `Snapshot` is a stub |
| Sable bows and says "Greetings, Holy One. You've activated a Gold Story Scroll." then "My name is Sable. I am a trainer of Creatures." | todo | each line waits to be read |
| Gossiping: "I challenge you to open these gates.", "Behind them are three wondrous Creatures.", "Every god has a Creature.", "They can grow as tall as a mountain and can perform Miracles." | todo | |
| "But you can't open the Gate unless you find the three Gate Stones." then "Each Stone you find must be placed on this platform.": the camera swings round to show the empty platform for a few seconds and comes back to her | todo | |
| "The Villagers were dancing around a Gate Stone when you arrived.": the camera rises over the land and flies down to the tiger gate stone at the end of the ravine the family led the player through, which is lit up | todo | the lighting-up is done by the stone's guard script (below) |
| Over the stone: "Once you place this Gate Stone on the platform, I will tell you of the other two." | todo | |
| A quick fade to black and back to Sable: "Now retrieve the first Gate Stone, Holy One." | partial | the fade commands work (`SetFade`, `SetFadeIn`, `FadeFinished` in `src/CHLApi.cpp`); the line and camera don't |
| The camera rises high over the hut, looking south over the village, the music stops and the cut-scene ends | todo | |
| The tiger stone can now be picked up and moved (it was fixed in place since the family's welcome dance) | partial | the pick-up and moveable flags work (`SetIdPickupable`, `SetIdMoveable`); the script never reaches them, so in openblack the stone can be picked up from the start |
| Sable walks back to her door and is removed 3 seconds later | todo | |

## Finding the first stone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three stones are part of the land: the tiger stone where the villagers danced, the ape stone by the lost brother's house and a blank rock in the quarry, with the gate stone platform and the gates by the trainer's hut | done | `Land1.txt` places them (`MobileStaticArchetype`, `AnimatedStaticArchetype`); see [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| A gate stone is dropped from the hand onto the platform to place it: the stone disappears and the platform shows it in one of its three sockets | todo | nothing fills the platform in openblack (`AnimatedStatic` keeps its socket state at 0) |
| The quest knows which stones are in by what the platform holds: the ape stone counts 1, the tiger 2 and the cow 4, so 3 means ape and tiger and 7 all three; the blank rock counts nothing | todo | `ObjectInfoBits` is a stub that always answers 0 |
| A stone once in the platform stays in it; the player can't take it out again | todo | |
| When the camera is within 100 of the tiger stone and it is on screen, the good advisor points at it: "Here's the stone the people were dancing around! Pick it up by clicking the Action Button on it.", at most every 30 seconds, until the player first picks it up | todo | |
| The first time the player hits the gates (for instance throwing something at them), the good advisor says "I sense that battering the Gates won't work, esteemed one."; only once | todo | the dialogue commands are stubs |
| Clues while the platform is empty: 2 minutes after the cut-scene the good advisor says "Didn't we see the Villagers dancing around a Stone a while ago?" | todo | `CreateTimer` stub |
| Every 2 minutes after that, still empty, the good advisor points at the stone and says "The Stone's at the end of the ravine we followed the Villagers through!" if it is within 50 of where they danced, or the first clue again if it has been moved | todo | |
| From the second clue on the scroll's reminder becomes the ravine line, even when the advisor actually said the first clue | todo | |
| The clues stop for good once any stone is in the platform | todo | |

## The first stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placing the tiger stone plays a scaffold-planting sound and moves the scroll to 0.25 | todo | `PlaySoundEffect` stub |
| A cut-scene brings Sable out of her hut as before: she cheers ("Excellent. You've found a Gate Stone. You'll need to search for another, now.") then looks about ("Try looking for a Gold Story Scroll in the Village.") | todo | |
| The Lost Brother starts here, its scroll appearing in the village | todo | [the_lost_brother.md](the_lost_brother.md) |
| The camera rises looking south over the village, Sable walks back in and is removed | todo | |

## The second stone placed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ape stone, earned in The Lost Brother, is placed the same way; with ape and tiger in, the scaffold sound plays and the scroll moves to 0.5 | todo | the ape stone can only be picked up once the brother's reward makes it so: [the_lost_brother.md](the_lost_brother.md) |
| Sable comes out cheering: "Good. You have two of the Gate Stones. But the third could be a problem.", "The Villagers say the final one was destroyed aeons ago. All is not lost, though.", "The Village sculptor can carve a new Gate Stone for you. You should see him." | todo | |
| The Sculptor starts here; the camera rises over the village and Sable goes back in | todo | [the_sculptor.md](the_sculptor.md) |

## The uncarved rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the blank quarry rock is put down within 20 of the platform (not held, not flying), Sable comes out puzzled: "The stone isn't carved. Perhaps the sculptor can help?", and the camera looks south towards the sculptor | todo | undetermined: whether a blank rock dropped right onto the platform is swallowed by it like a gate stone (the game's check is on the stones' shared kind, which the rock may share); the script expects it to stay on the ground |
| If the rock is still there afterwards, a reminder starts: every 3 minutes that the rock stays put and isn't picked up, the next time the platform is in view within 80, Sable comes out and says the same line again | todo | |
| Moving or picking up the rock ends the reminder, and putting it back by the platform starts it all over | todo | |
| The cow stone carved from the rock counts as the third stone when placed: the scaffold sound plays | todo | the carving and the cow stone: [the_sculptor.md](the_sculptor.md) |

## The stones are guarded

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guards start in the opening, as the family reaches the unfinished temple; the ape stone is fixed in place from then until The Lost Brother frees it | partial | started by the family's script ([../tutorial.md](../tutorial.md)); the flags work (`SetIdPickupable`, `SetIdMoveable`), the script never reaches them |
| A gate stone left lying (not held or flying) where the player has no influence is removed and made again at once at its home: the tiger stone's home is the dance site (only when it is more than 20 from it), the ape stone's is the soapbox by the brother's house, the blank rock's is the quarry | todo | `GetInfluence` (player influence at a point) and the timer commands are stubs; creating a mobile static works (`Create`) |
| A stone picked up and dropped more than 50 from its home and more than 20 from the platform goes back home a minute later (for the rock, more than 20 from the sculptor) | todo | `CreateTimer`, `SetTimerTime`, `GetTimer` stubs |
| A stone that no longer exists (destroyed, thrown away) is made again at its home | todo | |
| Each stone made again is lit up by a marker on it; the tiger stone is first lit up when Sable shows it, the ape stone when it is more than 100 from its home, the rock once the sculptor has asked for it | todo | `AddSpotVisualTargetObject` is a stub |
| A guard stops when its stone is in the platform; the rock's guard stops when the sculptor has carved it, and a guard for the cow stone starts then | todo | the cow stone's guard: [the_sculptor.md](the_sculptor.md) |
| A rock left by the platform goes back to the quarry a minute after it was dropped, as the platform is more than 20 from the sculptor, so the 3-minute reminder above can hardly ever play | todo | follows from the two rules; not seen in the game |

## Opening the gates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With all three stones in, the game's clock is set to 15:40 and runs for the scene, the scroll moves to 0.75, and the camera is kept inside a new zone around the glade | partial | `SetGameTime` and `GameTimeOnOff` work; `SetCameraZone` is a stub (zone files not read: [../../camera/camera_limits.md](../../camera/camera_limits.md)) |
| The three young creatures are made in the glade beyond the gates: the cow, the ape and the tiger | todo | `CreateScriptObject` makes no creatures |
| Sable comes out with a lasting "well done" and says "The Gate Stones are together. Pass through and claim your Creature!" | todo | |
| The camera moves onto the platform; with a grinding stone sound and a 7-second camera shake (within 20, amplitude 0.1) the stones sink into the platform | todo | `ShakeCamera`, `SetOpenClose` stubs |
| The camera cuts to the gate's chain, the gates open with a bolt sound, Sable beckons, and the camera watches the gates swing open with their opening sound | todo | the gates' and platform's collision models already follow an open state (`src/ECS/PhysicsClasses.cpp`), but nothing opens them |
| The camera flies through to the glade along a recorded path, with the third epic theme | partial | the music works; script camera paths are not played (`RunCameraPath` stub; [../../camera/camera_paths.md](../../camera/camera_paths.md)) |

## The creatures show off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tiger sits, the cow walks to its place, the ape points at the camera; the cow looks confused, then cow and ape turn to face the camera | todo | `CreatureForceAction`, `PlayAnim`, `SetFocus` stubs |
| The quest is logged again with a picture of the glade, at 0.75 | todo | |
| Both advisors come out. Good: "Look at them. Just look at them. These are Miraculous Creatures indeed." (the cow asks to be picked, the ape waves) | todo | |
| Evil: "They certainly are. But not quite as big as I expected." Good: "Not yet, maybe. But they can become the most powerful Creatures in the world." (the cow prays, the tiger growls) | todo | |
| Evil: "Now that I'd like to see. We must have one. Which should we choose?" Good: "Any. They're all special." Evil: "If rather small at the moment." (each creature plays happy or "pick me" in turn) | todo | |
| The advisors go home, cow and ape wave, the camera settles on the choosing view, the music stops and the clock is stopped again | partial | stopping the music and the clock work; the rest doesn't |
| Sable is removed and the camera is kept to a small choosing zone | todo | `SetCameraZone` stub |

## Choosing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Left alone, each creature cycles through show-off moves (pointing at the camera, waving, "pick me", happy, praying, tired, impressing, "look at me", the ape laughing), one every 5 to 20 passes of the choosing loop | todo | |
| Every 5 seconds each idle creature turns to face the camera | todo | |
| With the hand right over a creature (within 1), it asks to be picked; the other two take turns looking at the camera, "look at me" and a sad face (the cow and ape cry, the tiger gets angry) | todo | `GetHandPosition` works; the creature commands don't |
| Hovering over the cow for ten passes, with nothing being said, the good advisor points at it: "We could have the cow. A strong and noble beast." and the evil advisor answers "What? Not the fierce, lethal tiger? Click the Action Button on him!" | todo | |
| Over the ape: the good advisor, pointing: "Hmm. How about the ape. Intelligent and quick to learn?" | todo | |
| Over the tiger the evil advisor should say "I'm up for the tiger. Look at those claws.", but the line waits on the ape's hover count, which is zero whenever the hand is over the tiger, so it never plays | todo | script bug, in the shipped program too |
| A comment isn't repeated for the same creature twice running; the creatures keep showing off while a comment is read | todo | |
| Clicking a creature once has the good advisor ask "Are you sure you want this Creature? Click on him again if you are."; clicking another creature starts its count afresh | todo | `GameThingClicked`, `ClearClickedObject` stubs |
| Clicking the same creature a second time chooses it | todo | [../../creature/species_choice.md](../../creature/species_choice.md) says picking up and putting down also chooses; the script only counts clicks |

## The choice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The chosen creature is happy; the others cry (the tiger is angry instead when the ape is chosen); one points at the chosen creature and the other eyes it without coming near | todo | |
| The camera closes in on the chosen creature over 10 seconds; evil advisor: "We've chosen a Creature."; a second later the creature-chosen theme plays | partial | the music works |
| The quest is logged as finished (1, no alignment change) and the screen fades to black over 2 seconds | partial | the fade works; the log doesn't |
| In the dark the creature becomes the player's creature and is moved to its home by the Norse village, which becomes its home position; the other two are deleted (they don't wander off) | todo | `CreatureSetPlayer`, `SetCreatureHome` stubs; openblack's creature comes from its spawner and becomes the player's leashable creature (`LeashSystem::ClaimOnArrival`) |
| The camera zone widens to the fifth stage of the land and the creature is released to the game | todo | `ReleaseFromScript` stub |
| The creature's development is started: it begins growing up from its first stage, kept at home | partial | the start-development function works on the creature openblack's spawner makes (`DevFunction` in `src/CHLApi.cpp`), but the script never reaches it |
| The screen stays black until the trainer's first lesson, which fades in on the creature waking at its pen | todo | [the_creatures_learning.md](the_creatures_learning.md) |

## Skipping and keeping an old creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new game that skips the opening (the "skip tutorial" choice) deletes the three stones and goes straight to the glade: a 2-second fade to black instead of Sable and the platform, the gates opening as the screen fades in, and no advisor talk | todo | `CanSkipTutorial` is a stub that always answers no; the platform isn't sunk on this path |
| A player keeping the creature of an earlier game (patch 1.1) skips the glade too: clock to 15:40, the fifth camera zone, the gates open, and the old creature is loaded at a fixed spot | todo | `IsKeepingOldCreature`, `CurrentProfileHasCreature`, `LoadMyCreature` stubs; undetermined: where on the land that spot lies (the script calls it arbitrary) |
| Both paths count the quest as done for the scripts waiting on it (the Explorers) | todo | |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't fail: lost, thrown away or destroyed stones are made again at home, and the platform keeps every stone placed | todo | |
| The scenes wait for Sable to reach her spots with no time limit; if something blocked her, the cut-scene would never end (not known to happen) | todo | |
| The story waits for the choice with no time limit; there is no way to leave the glade without choosing, as the camera zone holds the camera there | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature guide's theme under Sable's first talk, the third epic theme for the gates and the glade, the creature-chosen theme for the choice | partial | `StartMusic`/`StopMusic` work; the scripts never reach them |
| The scaffold-planting sound when a stone goes in; the stone-grinding, bolt and gate-opening sounds as the gates open | todo | `PlaySoundEffect` stub |

## Saves, other versions and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps which stones are in, the stones' guards and the chosen creature | todo | openblack has no saved games |
| The cow's look-at point in the opening of the glade has a typing slip (a coordinate ten times too far north), so it stares far off past the glade | n/a | a data quirk in the script itself; openblack reproduces it by running the script as shipped |
| A cut placement of the ape stone first (and its line "Right. We need another Gate Stone…", missing from the text table) is commented out of the script | n/a | not in the program |
| An older, unshipped version of the quest (a good-advisor "Shh." line and a swap-style "are you sure" line) and a bare test script ("Choose one of these mothers") | n/a | not in the program |
| A cut silver scroll version of the sculptor's part | n/a | see [../silver_scrolls/the_sculptor_cut_silver.md](../silver_scrolls/the_sculptor_cut_silver.md) |
