# Idle behaviour

What a creature does with itself when nothing presses: it idles, sits, hangs about, looks around at whatever catches its
eye, explores the land, shows the player how it feels, and is moved by the music.

**Progress: 23/68 done, 15 partial — 45%**

## The idle agenda

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing better to do, the creature works through an idle agenda of small steps, chosen once a turn | done | `src/Creature/CreatureIdleMind.h` (`Think`) |
| Being idle is waiting a second or two, then a tired yawn, twice over | done | test `CreatureIdleMind.BeingIdleIsWaitingThenYawningTwice` |
| Sometimes it sits down for ten seconds or so, then gets up | partial | sitting works (test `CreatureIdleMind.SometimesItSitsForAWhile`), but which idle activity is picked is a stand-in random choice (`k_ActivityLots`), not the game's weighing by the desire to rest and what it has learnt |
| Hanging around: it walks somewhere near and sits there | partial | test `CreatureIdleMind.HangingAroundWalksSomewhereNearbyThenSits`; the game hangs around its home, here anywhere 20 to 40 away |
| Each idle step pulls a face as it starts and again every few seconds | done | `k_FaceRepeatSeconds`; test `CreatureIdleMind.IdleStepsPullAFaceAsTheyStartAndEveryFewSeconds` |
| Its faces vary as it goes on, never the same few | done | test `CreatureIdleMind.EachFacePulledMovesTheVarietyOn` |
| Abandoned while sitting, it gets up | done | test `CreatureIdleMind.AnAbandonedSitEnds` |
| With a need and the means at hand, it sees to the need before idling | done | test `CreatureNeedsMind.ANeedComesBeforeIdling`; see [physiology](physiology.md) |
| Curious, playful or angry with something near, it examines it, throws it about or hurls it at a home or tree | partial | `creature_mind::k_ActOnDesire` (0.3) is a stand-in for the planner's weighing |
| Holding something it has no use for, it puts it down | done | `Activity::PutDown` in `CreatureIdleMind.cpp` |
| Its idle mind stops while it is led on the leash, fights, is knocked out or paused | done | `CreatureMindSystem::ProcessTurn` |

## Showing the player how it feels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At most once a minute it shows its strongest desire with that desire's own animation | partial | test `CreatureIdleMind.ItShowsItsStrongestDesireOnceAMinute`; the strength needed to show it is a stand-in (0.2) for the game's planner choosing to show its state |
| Each desire it can show has its own animation (hungry, tired, needing a poo, angry …) | done | `creature_desires::EmoteFor`; test `CreatureDesires.EachShowableDesireHasItsEmote` |
| Within ten seconds of a stroke or slap it shows its pleasure or sorrow at it first | done | test `CreatureIdleMind.AStrokeOrASlapIsShownFirst` |
| It plays its last shown desire again on request | todo | |
| It waves at the player, points at the camera, howls at the player, is pathetic to get attention | partial | these emotes exist as plan actions facing the camera (`CreaturePlanActions.cpp`); the attention sources are stand-ins (see [desires](desires.md)) |
| It watches the player while the player has its attention | todo | |
| It looks at the camera in wide screen during cut scenes | todo | |
| A sound tells its mood when it shows it | todo | see [../audio/](../audio/) |

## Looking about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Looking about, its head turns to the most interesting thing it can see, the nearer the better | done | `src/Creature/CreatureLook.h`; test `CreatureLook.ItWatchesTheMostInterestingThing` |
| Things interest it by kind: doves most, then temples, creatures, animals, villagers, homes, trees, and fixed things least | done | `creature_look::InterestOf`; test `CreatureLook.InterestByKind` |
| Bigger creatures see further | done | `LookRange`; test `CreatureLook.BiggerCreaturesLookFurther` |
| It only sees what is in front of it | done | `CanSee`; test `CreatureLook.ItSeesWhatIsInFront` |
| The longer it watches one thing, the less else catches its eye; after twenty seconds nothing does | done | `k_BoredSeconds`; test `CreatureLook.TheLongerItWatchesTheLessElseCatchesItsEye` |
| What goes out of sight is dropped | done | test `CreatureLook.WhatGoesOutOfSightIsDropped` |
| With nothing to see it looks ahead | done | test `CreatureLook.WithNothingToSeeItLooksAhead` |
| It also considers mountains and the sea as things to look at | todo | only objects are candidates (`GatherCandidates`) |
| It looks at the mountains | partial | the action plays as a generic look about (`Build::LookAbout`), not at the land's high ground |
| It looks out to sea | partial | generic look about, not towards the sea |
| It looks at the sun, only while the sun is up | partial | generic look about; doesn't check the sun is visible |
| It looks at the moon, only while the moon is up | partial | generic look about; doesn't check the moon is visible |
| It looks at its temple | todo | |
| It looks down a cliff | todo | |
| It looks at something without going up to it, or stares for ever | todo | |
| It follows things flying through the air with its eyes | todo | |
| It looks at the player's hand, and at the camera | partial | look-at-hand and point-at-camera emotes exist; its head doesn't track the hand |
| It looks at its feet, down, frightened, stoned, or just woken | partial | the eyes have wide, closed, calm, stoned and ahead modes and droop after sleep (see [face, eyes and hair](face_eyes_hair.md)); looking at its feet or down are missing |
| It sees important things far away and turns to them | todo | |
| It tells the player something interesting is happening off screen | todo | see [lessons and help](lessons_and_help.md) |
| It keeps a single shared list of what it might look at, built once a turn | done | `GatherCandidates` in `CreatureMindSystem.cpp` |

## Exploring and wandering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land is split into regions; the creature remembers which it has visited and how long it stayed | todo | |
| Each region's highest point and kind (coast, hill, town) are worked out once a land is loaded | todo | |
| Wandering, it goes to the nearest region it doesn't know | todo | |
| It explores the coast | todo | |
| It explores the towns | todo | |
| It goes to the top of a hill and looks about | todo | |
| It goes to a hill and sits | todo | |
| It walks along a ridge | todo | |
| It sits down on the beach | todo | |
| It explores and casts teleport to get about | todo | see [creature casting](creature_casting.md) |
| It hangs around at home, and goes home | todo | see [home and pen](home_and_pen.md) |

## Little things it does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scratches when itchy | done | `Scratch` plan action |
| Shivers when cold, shows it is hot | done | `Shiver`, `ShowHotness` plan actions |
| Sneezes | done | `Sneeze` plan action |
| Pulls silly faces | done | `PullSillyFaces` plan action |
| Waves at the player and at things | partial | waving at the player and at a friend exist; waving at things doesn't |
| Practises throwing | partial | throwing things about exists (`ThrowAround`); practising throws as the game's separate action doesn't |
| Practises dancing | todo | |
| Looks at its reflection in the water | todo | |
| Watches the telly | todo | (unconfirmed what this action shows in the game) |
| Farts | todo | |
| Behaves strangely | todo | |
| Stands frightened on the spot, or is sad | done | `BeFrightenedOnTheSpot`, `BeSad` plan actions |
| Shows off an impressive pose (not too often) | partial | `ShowImpressiveAnimation`; the game's rule against repeating it too soon is missing |
| Rests, scratches and waves only if it hasn't lately | partial | the planner's novelty favours actions not done lately; the game's explicit "not recently" checks are missing |

## Music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nice music cheers it, nasty music sours it, frightening music scares it | todo | |
| It dances to music | todo | |
| Its feelings choose the music that plays | todo | see [../audio/](../audio/) |
