# Worship Site

The first of Khazar's lessons on the second land, logged as the gold scroll "Worship Site": once the player's temple
and village centre are finished, Khazar gives a builder disciple to raise the Norse worship site; when it stands he
comes back with a second scroll and teaches worship (raising the village totem) and charging miracles, then places the
workshop's scaffold and a third scroll that leads to his two miracle challenges. The land as a whole is in
[../land_2.md](../land_2.md), the land's script in [../../scripts/land2_script.md](../../scripts/land2_script.md), the
script program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Khazar (the friendly god's hand), at scrolls he lays by the player's temple · **Script:** LearnWorshipping · **Reward:** a builder disciple and the Norse worship site's miracles (grain, wood, water) to charge; the lesson ends with the workshop scaffold and the scroll to Khazar's two miracle challenges · **Repeatable:** no

Sources: the quest's script source (`LearnWorshipping.txt`), the shared Land 2 scroll notifier (`SetupLand2.txt`), the
hand demo scripts it runs (`HandDemos.txt`), the god-confronts-player helper (`Land2ComputerAI.txt`), the land's arrival
(`BeginLand2.txt`) and control script (`LandControl2.txt`), checked against the compiled scripts in the shipped
`challenge.chl`; the game's English text table (`InfoScript2.txt`) and its hand demo recordings (`Data/HandDemo`).
openblack's state is judged on the physics work tree (`ob-wt-physics`): of the 93 script functions this quest and the
scripts it runs need, 73 still only log "not implemented" in `src/CHLApi.cpp` (among them scrolls, dialogue, advisors,
moving and forcing Khazar's hand, hand demos, the challenge log, timers, worship site mana and totem reads, disciples,
scaffold properties, the moving camera). More basically, openblack never runs Land 2's control script: the land-loading
function does nothing and the story's top script always begins with Land 1's control script (see
[../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never appears; every row is todo unless
the notes say otherwise.

**Progress: 0/74 done, 6 partial — 4%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background straight after the arrival film (Khazar's introduction, the scaffolds, the disciple and the one-shot miracles), along with the land's other watchers | todo | the land control script never runs in openblack (see above) |
| The arrival leaves the player's citadel at 30% built and the village centre at 30% built, a builder disciple at the village centre, and Khazar's three gifts of one-shot food, wood and water (plus two heals) by the temple; the quest needs both buildings finished | todo | `BuildBuilding` is a stub; the arrival is not a gold scroll and has no file of its own (see [../land_2.md](../land_2.md), Arriving) |
| When the quest finishes, the control script (which checks every 9 seconds) opens The Workshop's watcher, the 20-minute timer for Impress Village, the influence watcher and a "spruce up your worship site" info sign | todo | see [the_workshop.md](the_workshop.md) and [impress_village.md](impress_village.md) |
| The lesson's own end lays the scroll that leads to [Khazar's Fireball Challenge](khazars_fireball_challenge.md) and [Khazar's Shield Challenge](khazars_shield_challenge.md) | todo | |

## Waiting for the temple and village centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At once, the village centre can no longer be picked up and the citadel can't build a worship site | partial | pick-up flags work (`SetIdPickupable`); the worship site switch (`SetCanBuildWorshipsite`) is a stub |
| The quest waits, checking without pause, until both the citadel and the village centre are fully built, or Khazar is gone | todo | building completeness reads (`GetProperty`) are stubs |
| When the citadel is finished, Khazar's hand flies to 10 m in front of the camera (snapped there if it hasn't arrived within 10 seconds) and he says "Your Temple is built. Excellent." | todo | `MoveComputerPlayerPosition`, `RunText` stubs; see "Khazar coming to the player" below |
| When the village centre is finished, Khazar comes the same way and says "Good. You have built much. But there is more for you to construct." | todo | |
| Each line is said once, in whichever order the buildings finish | todo | |

## The first scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's hand flies to a spot in front of the citadel (speed 250, at a fixed height), then is let go | todo | `MoveComputerPlayerPosition`, `ReleaseComputerPlayer` stubs |
| A gold scroll appears there, raised 5 above the ground | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and the scroll is on screen, the good advisor steps out, points at it and says "Khazar has something important to tell us.", at most once every 30 seconds and only when no film is playing | todo | the Land 2 scroll notifier; `SpiritEject`, `SpiritPointPos`, `RunText` stubs |
| The quest waits until the scroll (or its spot) is clicked, or until Khazar is gone; the scroll is then switched on | todo | `GameThingClicked`, `SetActive` stubs |
| A new man is made at the citadel and joins the player's home town | todo | creating villagers does nothing (`CreateScriptObject` only makes scenery and rocks) |

## The builder film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film starts; Khazar's hand (speed 350) picks the man up and drops him by the worship site's plot while the camera swings over to look at it | todo | forcing Khazar to pick up and drop (`ForceComputerPlayerAction`) is a stub; widescreen works (`SetWidescreen`) but the film never starts |
| Khazar: "I have given you an expert builder of Worship Sites" | todo | `RunText` stub |
| The man becomes a builder disciple (with its sound), carries a heavy mallet, walks to the plot and hammers in a loop | todo | `SetDisciple`, `SetObjectCarrying`, `OverrideStateAnimation` stubs |
| The camera pulls back over the plot; the home town and the citadel are now allowed to build a worship site | todo | `SetCanBuildWorshipsite` stub |
| Khazar: "This building is of great importance." then "I shall return once you have finished building the Worship Site." | todo | the lines are said in that order, the reverse of their text numbers |
| Khazar's hand is let go, the dialogue closes and the camera pulls further back | todo | |
| The log entry "Worship Site" is recorded at 0 with the good advisor's reminder "You need to build the worship site. Create some Disciples." | todo | `Snapshot` is a stub |
| Both advisors step out; as the film ends the evil advisor says "Oh boy, oh boy, oh boy, are we going to cause some havoc now!" and the good advisor "Havoc? No, but it will be good to cast lots of lovely Miracles when we need them." | todo | `SpiritEject`, `RunText` stubs |
| The builder is let go to work on his own | todo | `ReleaseFromScript` stub |

## Building the worship site

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player has the Norse worship site built by the temple (any builders, the given disciple or the player's own) | todo | see [../../worship/worship_sites.md](../../worship/worship_sites.md) |
| At half built, the log entry is set to 0.5: recorded afresh if the worship site is on screen, otherwise only updated quietly | todo | `Snapshot`, `UpdateSnapshot`, `GameThingFieldOfView` stubs |
| When it is fully built the first scroll fades away | todo | `ObjectDelete` stub |
| The quest then waits until the worship site is on screen, or 30 seconds, and logs "Worship Site" at 1 (complete) | todo | |
| Khazar comes up to the camera and says "The power you wield grows!" | todo | |
| There is no timer and no way to fail the building part: the quest waits for ever | todo | |

## The second scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's hand flies (speed 150) to a spot by the worship site and is let go; a second gold scroll appears there on the ground | todo | |
| The good advisor nags the same way: "Look. One of Khazar's Scrolls. Let's click on it." | todo | |
| Two bronze info signs appear once it is clicked: one at the village centre on raising and lowering the totem to send people to worship, one by the temple on worship sites, worshippers' needs, charging and the chant store | todo | the info sign script; `CreateHighlight` stub |
| The village centre can be picked up again from now on | partial | `SetIdPickupable` works; never reached |

## Khazar's lesson on worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen film with Khazar's music; Khazar's hand flies to the village centre (speed 400) and the camera follows | partial | `StartMusic` works; the moving camera (`MoveCameraPosition`, `MoveCameraFocus`) and moving Khazar are stubs |
| Khazar: "Your Worship site is built. Now let me teach you how to create Miracles." | todo | |
| Khazar: "Your people will worship you at the Temple." then "To send them there you must raise the Totem at the Village Centre." | todo | |
| The totem hand demo plays, paused at each step: "You move the Hand over the Totem." · "Then you hold down the Action Button to grab the Totem." · "You move the Hand up and down to raise and lower the Totem." · "I've raised the Totem so your followers will go to the Worship Site." | todo | `PlayHandDemo`, `HandDemoTrigger` stubs; the recording is `Data/HandDemo/totem.hnd`; see [../../hand/hand_in_scripts.md](../../hand/hand_in_scripts.md) and [../../hand/totem.md](../../hand/totem.md) |
| So that worship has begun by the film's end, up to eight of the home town's villagers (not ones held by scripts) are put straight at the worship site, within 3 of its centre, and gathered into a small group (inner 3, outer 10) that is then broken up | todo | `FlockCreate`, `FlockAttach`, `FlockDisband` stubs; villagers in openblack don't worship yet ([../../worship/worshippers.md](../../worship/worshippers.md)) |
| Khazar: "The higher you raise the Totem the more people from your Village will go to worship you" then "Now come and look at your followers." while his hand drifts slowly to the worship site | todo | |
| The camera flies a recorded track around the worshippers; the worship site's stored power is emptied before and after it | todo | `RunCameraPath` and `GameSetMana` stubs; see [../../camera/camera_paths.md](../../camera/camera_paths.md) |
| Khazar: "They are dancing, worshipping you. This gives you power." · "These are the Miracles that your people offer you." · "The Miracles of Grain, Wood and Water." | todo | |

## The miracle hand demo

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The worship site's power is set to nothing and the "miracle" hand demo plays | todo | `Data/HandDemo/Miracle.hnd`; `PlayHandDemo` stub |
| Khazar: "To activate one, grab the Miracle with the Action Button and it will be charged by your worshippers."; while this is being read the site's power is held at nothing | todo | |
| Khazar: "You'll see the energy rings flowing into your hand."; the site's power is raised to three quarters of a grain miracle's cost, in 50 steps with no pause between them (so in effect at once) | todo | `GetManaForSpell`, `GameSetMana` stubs |
| Khazar: "When the Miracle is charged it appears in your hand and you are ready to cast the Miracle."; if nothing is in the hand by then, the site's power is set to that three quarters again | todo | `GetObjectHeld1` stub |
| The film waits for the demo to finish | todo | `IsPlayingHandDemo` stub |
| Khazar: "Remember worshipping comes at a cost, the needs of your worshippers can be seen on these flags." as the camera moves to the worship site's needs flags | todo | see [../../worship/worship_sites.md](../../worship/worship_sites.md) |
| The "miraclecast" hand demo plays without changing the hand's look; meanwhile Khazar: "So it's good to satisfy these needs and keep your Villagers in good health." | todo | `Data/HandDemo/MiracleCast.hnd` |
| Khazar: "I'll leave you now to experiment with Miracles, and expand your influence by growing your Village." | todo | |
| The home town's totem is set to half height, so half the village goes on worshipping after the film | todo | totem height through `SetProperty` is a stub |
| The camera pulls back over the temple, the music stops and the film ends; the second scroll fades away | partial | `StopMusic` works; the rest is stubbed |

## After the lesson

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar makes a scaffold at his own workshop, set to become a workshop (size 3) and to vanish once used, and his hand carries it (speed 150) to a plot in the player's village to plan the workshop; it then can't be picked up or moved | partial | the pick-up and move flags work (`SetIdPickupable`, `SetIdMoveable`); making a scaffold (`CreateScriptObject` makes no scaffolds) and `SetScaffoldProperties`, `ForceComputerPlayerAction` are stubs; see [../../building/workshop_and_scaffolds.md](../../building/workshop_and_scaffolds.md) |
| Khazar's hand then flies to a third spot by the temple and is let go; the scroll for his miracle challenges appears there (see [khazars_fireball_challenge.md](khazars_fireball_challenge.md) and [khazars_shield_challenge.md](khazars_shield_challenge.md)) | todo | |
| The quest marks the lesson done, which lets the control script start The Workshop ([the_workshop.md](the_workshop.md)), the timer for Impress Village ([impress_village.md](impress_village.md)) and the "spruce up your worship site" info sign | todo | |

## Dying worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the lesson's end, a watcher checks the home town's count of worshippers who died from worship, at most once a minute (first check a minute after it starts) | todo | `GetTownWorshipDeaths`, `CreateTimer` stubs; worshippers dying is not in openblack ([../../worship/worshippers.md](../../worship/worshippers.md)) |
| If more have died than when the watcher began, both advisors step out: evil "Ha! Your worshippers are working and dying for you!", good "Save them! You only have to lower your Village Totem." | todo | |
| The count it compares with is never moved on, so after the first death the warning comes back every minute, new deaths or not | todo | script quirk, kept as the game does it |
| The watcher ends for good as soon as the totem is below 0.4 of its height | todo | `GetTotemStatue` and the height read are stubs |

## Khazar coming to the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| For each remark, Khazar's hand flies (speed 350) to 10 m in front of the camera, rechecking every 0.3 seconds; after 10 seconds it is put there at once | todo | the god-confronts-player helper; `MoveComputerPlayerPosition`, `SetComputerPlayerPosition`, `GetFacingCameraPosition` stubs |
| The remark is shown as a one-line dialogue in his voice, then his hand is let go | todo | |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Both scrolls are nagged by the good advisor (see above); the log entry's reminder is the good advisor's "You need to build the worship site. Create some Disciples." and is the same at every stage, even after the site is built | todo | tapping the log entry runs the standard reminder, which steps out whichever advisor owns the line |
| The advisors' banter after the builder film and the dying worshippers warning are the only other advisor lines | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's theme plays through the worship lesson and stops at its end; the builder film has no music of its own (the arrival's Khazar theme has already stopped) | partial | `StartMusic`/`StopMusic` work in openblack; never reached |
| The builder disciple is made with its disciple sound | todo | `SetDisciple` stub |
| Every spoken line is the voice of its speaker in the text table (Khazar, good spirit, evil spirit) | todo | dialogue is a stub |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature takes no part in the quest; nothing in it moves or locks the creature | n/a | |

## Failing, Khazar's death and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't be failed and has no time limit | todo | |
| If Khazar is gone (killed by Nemesis, see [../land_2.md](../land_2.md), the end of the land) before the temple and village centre are done, the scroll, builder and film are skipped and the worship site is simply allowed | todo | the control script's death check runs from the land's start, so this can happen if the player takes enough of Lethys's land early |
| If Khazar is gone when the second scroll would appear, the lesson, the workshop scaffold and the challenges' scroll are all skipped; the lesson is still marked done | todo | The Workshop then never starts (its watcher ends when Khazar is gone) |
| If Khazar dies while the second scroll waits, the scroll notifier stops but the scroll is never removed: it stays on the ground | todo | script quirk |
| Nothing here can stop the land being finished: the land ends on Lethys's towns and temple, not on the lessons, so there is no soft-lock | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wait for the citadel and village centre and the dying worshippers watcher loop without any pause | todo | |
| The first 0.5 log entry is recorded only if the worship site is on screen at that moment; otherwise the entry is updated without being announced | todo | |
| The builder's two closing lines are said in the reverse order of their numbers in the text table ("of great importance" before "I shall return") | todo | |
| The miracle demo's slow fill of the worship site's power has no pause in it, so the power jumps straight to three quarters of a grain miracle | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An earlier place for the "Worship Site" log entry, in the middle of the builder film, is commented out | n/a | never in the shipped game |
| A separate hand demo script for casting is commented out in favour of playing the cast demo directly | n/a | never in the shipped game |
