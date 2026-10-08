# The Heartbroken Man

The gold scroll on the fourth land that frees the third of Nemesis's Guardian Stones, the one that keeps the land in
darkness. A woman asks the player to bring Keiko, kidnapped by Nemesis to the Aztec village, back to her husband Adam,
the lonely old man who keeps the stone on a mountain top. Reuniting them, killing him or killing her all free the stone;
the three endings differ in what is said and in the alignment the challenge log records.

**Land:** 4 · **Giver:** a woman at a hut below Adam's mountain (the scroll is beside her) · **Script:** Land4Nomad (in
Land4Meteorites) · **Reward:** the darkness Guardian Stone is destroyed and the sky turns back to day; influence around the
hut (killing Adam) or Adam's hut (reuniting them) · **Repeatable:** no

The land as a whole is in [../land_4.md](../land_4.md); the other two stones are [The Totem Puzzle](the_totem_puzzle.md)
and [The Defending Ogres](the_defending_ogres.md); what happens once all three are broken is in
[Undead Village](undead_village.md) and [leave_through_the_vortex_land_4.md](leave_through_the_vortex_land_4.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`), the game's
text table (`Scripts/InfoScript2.txt`) for every spoken line and its voice. openblack's state is judged on the physics
work tree (`ob-wt-physics`): openblack starts the story's top script, which always runs the first land's control script
first; the map-loading command (`LoadMap` in `src/CHLApi.cpp`) has an empty body and the first land's script stalls long
before its end, so the fourth land's control script, and this quest, never run. Rows are partial only where every
command they need works in openblack; they still never happen in play.

**Progress: 0/76 done, 10 partial — 7%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background as soon as the land begins, together with the other two stone quests | todo | the fourth land's control script never runs (see [../../scripts/land4_script.md](../../scripts/land4_script.md)); `LoadMap` is empty |
| From the start of the land a small Guardian Stone (a meteor at a third of its size, raised 2 off the ground) sits near Adam's hut on the mountain, smoking: a bonfire effect 25 times normal size that lasts the whole land | partial | `Create` makes the meteor and its indestructible, not moveable, not pick-up flags work (`SetIndestructable`, `SetIdMoveable`, `SetIdPickupable`); scale and altitude (`SetProperty`) and the smoke (`SpecialEffectPosition`) are stubs |
| The quest itself waits until the elder of the Japanese village has finished his introduction to [The Totem Puzzle](the_totem_puzzle.md), which itself waits until the player owns, destroys or empties the Japanese village | todo | the totem puzzle's intro raises the flag this quest waits on; so the Heartbroken Man cannot be found before the Japanese village is dealt with |
| The man who explains Nemesis's curse earlier describes this stone: "The third Stone was given to a lonely old man." "Who worships only Nemesis." | todo | his scene is covered with [The Defending Ogres](the_defending_ogres.md), whose scroll he opens |
| Breaking this stone is one of the three conditions the land waits for before the Undead Village and the way out open | todo | the land waits for the lightning, darkness and fire stones; this quest breaks the darkness one |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears by the woman's hut, on the lowland west of Adam's mountain, and its challenge is made the current one | todo | `CreateHighlight` is a stub |
| While the scroll is not clicked, whenever the camera is within 100 of it and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | todo | the shared notify script; `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| Clicking the scroll (or its spot) starts the introduction and turns the scroll active | todo | `GameThingClicked`, `SetActive` are stubs |
| The scroll is never removed by the script: it stays where it is after the quest is over | todo | no delete of this scroll anywhere; what an old active gold scroll looks like after its challenge is complete is not in the scripts |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A woman (an Aztec housewife) is made at her hut and Adam (a nomad) at his house on the mountain, facing a spot by the door and mourning on an endless loop | todo | making villagers from a script does nothing (`Create` only makes mobile statics and rocks); `SetFocus`, the animation commands are stubs |
| A cut scene begins with the generic script music (number 3) | partial | `StartMusic` works; the cut scene itself (`StartCameraControl`, widescreen, input lock) is a stub |
| The woman (drawn in high detail) walks out of her hut while the camera glides over 6 seconds to look at her from beside the scroll | todo | `MoveGameThing`, `MoveCameraPosition`, `MoveCameraFocus`, `SetHighGraphicsDetail` are stubs |
| When she has arrived and the camera has stopped, she turns to the camera and gossips (three loops of her talking gesture) | todo | |
| The woman: "At last! Someone to hear my prayers!" | todo | `RunText` is a stub; the source's comment marks the line as a late, "dodgy" new text |
| The screen fades to black over 2 seconds, the camera cuts to Adam's door on the mountain and fades back in over 2 | partial | `SetFade`, `SetFadeIn`, `SetCameraPosition`, `SetCameraFocus` work; the dialogue box close (`EndDialogue`) is a stub |
| The challenge log records the quest: title "The Heartbroken Man", progress 0, alignment 0, with the reminder: the good advisor saying "Find Keiko in the Aztec Village and reunite her with Adam." | todo | `Snapshot` is a stub; the reminder is replayed by tapping the scroll in the log; the script waits a tenth of a second so no text is caught in the log's picture |
| The woman (heard over Adam mourning): "My neighbour, Adam is a soul in torment." | todo | |
| The camera drifts round the mountain top for 10 seconds while she goes on: "Keiko, his wife has been kidnapped and taken to the Aztec Village by Nemesis." | todo | |
| "Before he left he made sure she stays alive only while Adam's faith in him remains." | todo | the text table's line is longer than the source comment ("Nemesis keeps her alive only while…") |
| The camera turns to look down at the woman for 4 seconds, then flies back to her in 6 to 7 seconds; she gossips again: "Please help them by rescuing Keiko and reuniting her with Adam." | todo | |
| The camera closes on her over 5 seconds as she walks back into her hut | todo | |
| The evil advisor steps out: "Hey, Boss. Wasn't he the lonely guy with the Guardian Stone?" then "Let's kill him!" | todo | `SpiritEject` is a stub |
| The music stops and the cut scene ends | partial | `StopMusic` works |
| Keiko (an Aztec housewife) is made at the edge of the Aztec village and the woman who told the story is removed | todo | villagers are not made by `Create`; `ObjectDelete` is a stub |

## Keiko in the Aztec village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko walks at a tenth of normal speed round three spots by the village: at the first she sits down, sits for four loops and stands; at the second she inspects something; at the third she mourns (into, three loops, out of); then back to the first | todo | `MoveGameThing`, the speed property and the play-animation commands are stubs |
| She walks between the spots with despairing walks | todo | `OverrideStateAnimation` is a stub |
| Picked up by the hand, thrown, or held by the creature, she is watched: once she is put down again more than 100 from her second spot, she is put straight back there, but only when nobody is holding her, she is not flying and the camera is not looking at her | partial | `SetPosition` works; the held, flying, in-creature-hand and viewed tests are stubs (`InCreatureHand` and others) |
| So the player can carry her anywhere, but she must be kept in view, or she is snapped back to her village | todo | the reset only fires while she is out of view |
| The script sets no condition on reaching her; whether the hand can pick her up depends on the game's usual influence rule, which the script does not touch | todo | undetermined from the scripts which villages the player's influence normally covers at this point |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bring Keiko to within 50 of Adam: the first time she gets that close, Adam walks out of his house to a spot by his door and waves for attention on an endless loop, and the player's influence is made in a circle of 50 around him | todo | `MoveGameThing`, `InfluenceObject` are stubs |
| Once Adam is out, the reunion starts when Keiko is within 50 of him and neither of them is held by the hand or the creature or flying through the air | todo | a thrown Keiko only counts once she has landed |
| The quest has no timer and no distance limit other than these: it waits as long as the player likes | todo | |
| Killing Keiko (or anything that removes her) ends the quest the evil way | todo | her health at 0 or her not existing |
| Killing Adam (or anything that removes him) ends the quest the other way | todo | his health at 0 or him not existing |
| Whichever comes first wins: the script checks Keiko's death, then Adam's death, then the reunion, in that order, every time round | todo | |

## Reunited (the good ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk and its watcher stop; neither of them can be picked up any more | partial | stopping scripts by name (`StopScript`) and the pick-up flag (`SetIdPickupable`) work; the villagers themselves are never made |
| The screen fades to black over a second; the epic script music (number 4) starts; the two are set face to face by Adam's door, drawn in high detail, with the camera close on them; it fades in after 2 seconds | partial | fade, `SetPosition`, camera cuts and music work; facing and detail (`SetFocus`, `SetHighGraphicsDetail`) are stubs |
| After a second they both dance | todo | |
| The challenge log records it complete: progress 1, alignment +0.8 | todo | `Snapshot` is a stub; the record is the challenge log's, not a direct change to the player's alignment (see [../challenges_and_rewards.md](../challenges_and_rewards.md)) |
| When the dance ends they hug, then both turn to the camera | todo | |
| Adam, gossiping: "Oh how can I ever, ever thank you!"; half a second later Keiko joins in with her own gestures | todo | |
| Keiko: "We're together again! I can't believe it." | todo | |
| Adam: "You are a magnificent god! I have a reason to live!" | todo | |
| They walk slowly (Adam at a fifth of normal speed, Keiko at 0.15) to the door and into the house while the camera pulls back over 6 seconds; the screen fades to black and the music stops | todo | |

## Keiko killed (the evil ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black over a second | todo | |
| A cut scene: Adam is set by his door facing the camera, in high detail, mourning; it fades in after 2 seconds | todo | |
| The challenge log records it complete: progress 1, alignment -0.8 | todo | `Snapshot` is a stub |
| Adam: "You've killed my soulmate!" | todo | |
| "Nemesis promised to protect her. He has failed!" | todo | |
| "Take the Stone. I've got no faith left in anything." | todo | |
| The screen fades to black | todo | |

## Adam killed (the other ending)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keiko's walk stops; after 2 seconds the screen fades to black | todo | |
| A cut scene: the neighbour woman is made again at her spot by her hut, facing the camera, mourning for five loops; it fades in after 2 seconds | todo | villagers are not made by `Create` |
| The challenge log records it complete: progress 1, alignment 0 (neither good nor evil) | todo | `Snapshot` is a stub |
| The woman: "My neighbour lies dead." then "You are as cruel as Nemesis. Are all gods like this?" | todo | |
| The evil advisor steps out: "Boss, that guy's out of the way." then "And the Stone isn't under Nemesis' influence any longer." and goes home | todo | `SpiritHome` is a stub |
| The screen fades to black; the player is given influence in a circle of 200 around the woman, which is never taken away; the woman is let go to live as a normal villager | todo | `InfluenceObject`, `ReleaseFromScript` are stubs |

## The Guardian Stone breaks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever the ending, Adam and Keiko are both removed from the world, even after a happy reunion | todo | they vanish behind the fade into the hut |
| A cut scene: the camera is set looking at the stone by the hut and fades in after 2 seconds | partial | camera cut and fade work; the cut scene itself is a stub |
| After a reunion the good advisor points at the stone: "Look! The newfound faith of this man has freed the Guardian Stone." | todo | |
| After either killing the evil advisor points at it: "Ha! Killing. It's the answer to everything. Eh, beardy?" then "Never underestimate death. And fear. And agony." | todo | |
| A second later the Guardian Stones music starts; the camera shakes within 300 of the stone (amplitude 0.1, 5 seconds) and follows the stone as it rises, a tenth at a time, to 20 off the ground | todo | `ShakeCamera`, `FocusFollow`, altitude (`SetProperty`) are stubs; `StartMusic` works |
| A bang and a flash (6 seconds each) go off at the stone, it is destroyed with an explosion and the stone-explosion sound plays | todo | `SpecialEffectPosition`, `ObjectDelete` with explode, `PlaySoundEffect` are stubs |
| After 2 seconds the screen fades, the camera cuts to look at the sky over the home village and fades in | partial | fade and camera cut work |
| The day comes back: game time is switched on again and moved to noon over 200 | partial | `GameTimeOnOff` and `MoveGameTime` work in openblack |
| After 10 seconds both advisors step out. Good: "The sky! It's turning back to normal!" Evil: "Personally I liked it better before." | todo | the source comment for the evil line differs ("Its turning back to its normal colour!" was its first draft, given to the good advisor) |
| 3 seconds later they go home; the screen fades, the camera goes back to where it was before the scene and fades in | todo | the saved camera position is a marker at the camera (`GetCameraPosition` works, the marker is not made) |
| The darkness stone counts as broken; the music stops and the smoke over the stone is removed | todo | |

## What it unlocks next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The darkness the land has been in since the start lifts: the land's darkness effect, which held the clock at dusk, lets time run again as soon as this stone breaks | todo | the darkness effect sets the time to 16:30, moves it to 19:00 over 1000, after 2 minutes stops game time and waits for this stone |
| If the Ogre has not been beaten yet, the gold scroll of [The Defending Ogres](the_defending_ogres.md) is put up over the Ogre's valley (active, 10 off the ground) | todo | the Ogre's challenge was already logged by the man who explained the curse; this is the first time its scroll appears in the world. The Ogre's script removes it when he is beaten |
| With the fire and lightning stones also broken, the land moves on to the [Undead Village](undead_village.md) | todo | |

## Soft-locks and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: every way it ends breaks the stone | todo | |
| It cannot soft-lock while either villager can die: the player can always kill Adam or Keiko (by hand, miracle or creature) | todo | |
| It cannot start until the Japanese village has been taken, destroyed or emptied, because it waits for the bell-tower elder's introduction | todo | |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can carry Keiko: while she is in its hand she is not snapped back, and the reunion waits until it has put her down | todo | `InCreatureHand` is a stub |
| The creature killing either of them (eating or attacking) ends the quest like the player killing them | todo | the test is only on their health or their being gone |

## Unused or cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An early draft of the lines survives in the source comments: Adam was "Knobbo", Keiko his "girlfriend", and the woman was simply his friend and neighbour | n/a | comments only; the shipped text table names him Adam and Keiko his wife |
| A test launcher (not compiled into the game) runs this quest on its own by setting the start flag | n/a | `RunLand4Nomad`; never in the shipped `challenge.chl` |
