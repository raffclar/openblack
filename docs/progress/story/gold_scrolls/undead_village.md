# Undead Village

The fourth land's last story scroll before the Creed: once all three of Nemesis's Guardian Stones are broken, a man of
the Japanese village tells how one village refused Nemesis, had its two totems driven into the ground and was turned
into walking skeletons. Raising both totems back to full height breaks the curse; the freed villagers tell the player
that the Creed lies in the body of the Guide, and the creature fetches it from the mountain top.

**Land:** 4 · **Giver:** a man of the Japanese village, under a gold scroll at the village's edge · **Script:** Land4Meteorites (its undead-village part) · **Reward:** the second element of the Creed (the creature picks it up) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table (`InfoScript2.txt`) for every spoken line and its speaker, and the executable.
openblack is judged on the physics work tree (`ob-wt-physics`): the land never runs there (openblack starts the story's
top script, which always runs Land 1's control script first, and the map-loading command has an empty body), and nearly
every command the quest uses (villagers, highlights, snapshots, dialogue, advisors, skeletons, totem heights, creature
orders, the Creed) only logs "not implemented" in `src/CHLApi.cpp`. Every row is todo unless the notes say otherwise.
The land as a whole is in [../land_4.md](../land_4.md); its script program in
[../../scripts/land4_script.md](../../scripts/land4_script.md).

**Progress: 0/49 done, 6 partial — 6%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The undead village is set up as soon as the land starts: the town by the two totems is made skeletons, two African totems (scale 2.233, turned 352.1° and 36°) are made beside it fully sunk into the ground (height 0), both totems and the whole town are made indestructible | todo | `SetSkeleton`, `CreateWithAngleAndScale` for totems (it only makes mobile statics and rocks) and `SetProperty` for the height are stubs; `SetIndestructable` works and marks every villager of a town |
| The quest itself only starts after the land's control script sees all three Guardian Stones broken: the lightning stone (beating Sleg, [the_defending_ogres.md](the_defending_ogres.md)), the darkness stone ([the_heartbroken_man.md](the_heartbroken_man.md)) and the fire stone ([the_totem_puzzle.md](the_totem_puzzle.md)), in any order | todo | the land's control script is never reached |
| The control script waits for the whole quest (it is run in the foreground), then runs the Creed scene and only then the exit vortex quest ([leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md)) | todo | |
| Until the Creed is in the creature's hand, the undead town's health is set back to full every five minutes, "so they don't hobble about" | todo | `SetProperty` (health) is a stub |
| Until the town is saved, any belief of the player's in it that reaches 0.1 is wiped to 0 (checked every second): the town cannot be impressed while cursed | todo | the town belief commands are stubs |
| After the story scene has been seen, the second time the belief is wiped the good advisor steps out: "Leader, we aren't getting anywhere trying to impress the people here." / "We've just GOT to break the curse Nemesis has put on this place.", then the evil advisor "But we'll need influence to get near those Totems." | todo | it fires once only (on exactly the second wipe); before the scene, wipes are silent and not counted. The text table gives the third line to the evil advisor though the script ejects only the good one |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears at the edge of the Japanese village, on the man's spot | todo | `CreateHighlight` is a stub |
| While the camera is within 100 of the scroll and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | todo | the shared notify script; `SpiritEject`, `SpiritPointGameThing` are stubs |
| The quest waits until the scroll (or its spot) is clicked; nothing else moves on | todo | `HighlightClicked`-style checks are stubs; the scroll is left in place, active, until the quest ends |
| As the scene starts, both totems are pushed up to their full height so the story can show them | todo | the totem's full height comes from its own info and was not looked up |

## The story scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's music plays through the whole scene | partial | `StartMusic` plays the track (`src/Audio/GameMusic.cpp`); the scene never runs |
| The camera moves in 5 seconds to look at the river bank; a Japanese farmer appears from behind a hut, walks to a mark, turns 90° left, walks to his spot and faces the camera, gossiping | todo | villagers can't be made by scripts; the play-animation and move commands are stubs |
| The man: "Your success is great, Mighty One." / "You have almost restored our land to blessed normality." / (the camera creeps closer over 20 seconds) "But one more area remains tainted by Nemesis." | todo | the Japanese man's narrator; `RunText` is a stub |
| Fade to black; the town is shown as it was, alive: the skeletons are switched off, the time of day set to 16:50 and stopped, and the camera glides past the houses for 30 seconds | partial | the fade (`SetFade`, `SetFadeIn`), the time of day (`SetGameTime`) and stopping the clock (`GameTimeOnOff`) work; the skeletons and camera moves don't |
| The man: "One Village refused to bow to Nemesis." / "They saw his might and were afraid." | todo | the text table's second line differs from the script's notes ("They didn't like his ways.") |
| Fade; a happy scene in the village: two children (ages 10 and 9) jumping and facing each other, a man walking slowly by, two women gossiping, and a man the camera follows walking to the middle of the village; all are drawn in high detail | todo | villagers can't be made by scripts; `SetHighGraphicsDetail` is a stub |
| The totems shake: the gate-stone grinding sound and a camera shake (radius 150, strength 0.25, 5 seconds) around each totem, then the camera looks down from high behind them | todo | `ShakeCamera`, `PlaySoundEffect` are stubs |
| The scroll is entered in the challenge log here, titled "Undead Village", at 0%, with the reminder "Break Nemesis' curse to get the Creed." (said by the good advisor when the log entry is clicked) | todo | `Snapshot` is a stub |
| From here the totems start to sink back (see "Raising the totems") | todo | |
| The man: "So Nemesis buried their Totems, their source of power." | todo | |
| The curse: "And cursed their Village." — when the followed man reaches his spot a puff of smoke (scale 6) bursts and he is yanked into the ground; one child points and talks, then one after another, a quarter to half a second apart, each villager vanishes in smoke (scale 3) and is yanked underground; each comes back a skeleton, drowns, mourns and lies poisoned | todo | special effects (`SpecialEffectPosition`) and skeletons are stubs |
| The whole town turns to skeletons; after the last villager six seconds pass, then a fade and the six actors are removed | todo | |
| Back with the man (the time of day is put back and the clock restarted): "If you help them. I'm sure they'll be forever in your debt." / (the camera creeps closer over 20 seconds) "They know where the Creed you seek can be found." | partial | the clock commands work; the rest doesn't |
| Fade back to where the player's camera was before the scene; the music stops; the man is removed | todo | |

## Raising the totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player raises each totem with the hand (the town's totem pole is gripped and pulled up), which needs the player's influence there, as the advisors hint | todo | totems and raising them by hand don't exist in openblack (see [../../creature/town_actions.md](../../creature/town_actions.md)) |
| A totem that is partly raised and not being held by the hand sinks back, a tiny step (0.00005 of its height) every pass of the watcher, and puffs dust (scale 7) while it sinks | todo | how fast this is in seconds depends on how often the watcher runs and is not determined |
| Quirk: the whole watch (sinking and the creature's help) only runs while the player has a creature; with none, the totems never sink | todo | |
| If the creature is within 50 of a totem that has once been raised above 90 and has since fallen right back to the ground, the creature is ordered to raise that totem itself; when the totem reaches full height the creature is released | todo | `CreatureDoAction` is a stub; raising totems is not a creature action in openblack (see [../../creature/decision_making.md](../../creature/decision_making.md)) |
| The curse breaks the moment both totems stand at exactly full height together | todo | |
| Nothing fails this quest and nothing times it out: the land waits for it for ever | todo | there is no failure branch, so the story only moves on once both totems are up |

## The curse lifted

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A skeleton villager appears in the village, and a scene starts with the second epic theme: the camera moves in over 6 seconds and the skeleton walks to the village's middle | partial | `StartMusic` works; the rest doesn't |
| He is yanked into the ground in a large puff of smoke (scale 16); the whole town stops being skeletons, and so does he; he drowns, mourns and then stands | todo | |
| The freed man: "We owe you everything." / "We can tell you where the Creed you need can be found." / "The Creed lies in the body of the one you knew as the Guide." | todo | the second and third are played in that order, though their text numbers are the other way round. For the Guide's death see [../creature_guide.md](../creature_guide.md) |
| The Creed is made on a mountain top (at height 138.7, the height of the mountain top where the Guide died on the first land, though at a different spot of the map), with an anti-influence of radius 30 around it so the hand cannot take it | todo | creating it is a stub (`Create` only makes mobile statics and rocks, not the Creed object) |
| Fade; the camera is set to the start of the land's seventh camera path and flies along it to show where the Creed lies | todo | `RunCameraPath` and the camera-to-path command are stubs |
| The scroll goes to 100% in the challenge log, the gold scroll is removed, and the village can be harmed again | todo | `Snapshot` is a stub |
| The music stops; the creature is released; the town counts as saved (the belief wiping stops, so it can now be impressed) | todo | |

## The Creed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the freed man tells where it is, the Creed piece is made on a hill east of the home village, with a 30-radius ring of anti-influence round it so the player's hand cannot take it | todo | `Create` only makes mobile statics and rocks; anti-influence from a script is a stub |
| Only the creature can collect it: the scene waits until the Creed is in the creature's hand | todo | `InCreatureHand` is a stub; the Creed object is not made (`Create` only makes mobile statics and rocks) |
| The moment it is, the creature counts as having the Creed; the screen fades to white over a second and, 2 seconds later, the Creed object is removed | partial | `SetFade` works (any colour); `ObjectDelete` is a stub |
| The creature is put on the hilltop facing east, with a faint glow above its hand (scale 5, power 0.1) | todo | `SetPosition` works; `SetFocus` and `SetCreatureCreedProperties` are stubs |
| A cut scene with the epic script music (number 4): the camera looks up at the creature's head from below and fades in over a second | partial | `StartMusic`, camera cuts and the fade work |
| The camera climbs slowly (20 seconds) high above the hill; after 4 seconds the glow above the hand grows to full power | todo | `MoveCameraPosition`, `SetCreatureCreedProperties` are stubs |
| Both advisors step out. Good: "You found the second Creed!" Evil: "Yeah. Oh this is a great day!" | todo | `SpiritEject`, `RunText` are stubs |
| A second later a screen rumble sounds and the camera shakes hard (amplitude 1, radius 100, 5 seconds) | todo | `PlaySoundEffect`, `ShakeCamera` are stubs |
| 6 seconds later the good advisor: "What was that?" Both go home, the music stops, the creature is let go | todo | |
| While the creature has not got the Creed, the Undead Village's people are kept at full health | todo | see [the curse lifted](#the-curse-lifted) |
| The rumble leads straight into the exit vortex quest | todo | [leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md) |
| What the Creed does to the creature in play (beyond the glow) | todo | not determined from the scripts; see [../land_4.md](../land_4.md) |

## Unused or cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A line "And they know the location of the object you desire." is recorded but taken out of the scene and marked as no longer needed | n/a | never played |
| The quest's "finished" entry is marked in the script as possibly belonging elsewhere, but it stays at the moment the curse lifts | n/a | a note only; no behaviour |
