# The Immersion Mushrooms

A first-land silver scroll that exists only for players with an Immersion force-feedback mouse: a hippy beside his
hut asks the player to find the most powerful of his magic mushrooms, the one that shakes most in the hand, and drop
it in his cauldron. The right one gets a healing over his hut and a compassion creature miracle dispenser; the wrong
one blows his hut up.

**Land:** 1 · **Giver:** a hippy at his hut · **Script:** MagicMushroom · **Reward:** a "Compassion" creature miracle dispenser beside the hut, for the right mushroom only · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable (the puzzle's mushrooms). openblack is judged on the
physics work tree (`ob-wt-physics`): of the 57 script functions the quest and the scripts it starts need, 41 only log
"not implemented" in `src/CHLApi.cpp`, among them every dialogue, advisor, camera move, highlight, snapshot and puzzle
query; creating the puzzle, the hippy or the dispenser does nothing (`CreateScriptObject` only makes mobile statics)
and the game-time read (`DllGettime`) pushes nothing. openblack has no force-feedback support at all (see
[../../pc_integration/online_services.md](../../pc_integration/online_services.md)). The land's control script also
stops long before this quest is started (see [../../scripts/land1_script.md](../../scripts/land1_script.md)). The
puzzle itself is summarised in [../minigames.md](../minigames.md), the mushrooms as things in the world in
[../../resources/poison_and_mushrooms.md](../../resources/poison_and_mushrooms.md), the land in [../land_1.md](../land_1.md).

**Progress: 0/37 done, 5 partial — 7%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest after the trainer's slap-and-stroke lesson (with the singing stones' start), or straight after setup in a game that skips the creature training | todo | the land's control script never gets that far |
| The quest happens only if a force-feedback mouse is plugged in; otherwise the script ends at once, leaving the land as it is | partial | `ImmersionExists` always answers no, which is right for a player without one; the script is never reached |
| With the mouse, every magic mushroom and every small mushroom already standing within 100 of the puzzle's spot is removed | todo | `ObjectDelete` stub; without the mouse they stay |
| The mushroom puzzle is made beside the hippy's hut at 1.2 scale | todo | making a puzzle from a script does nothing |
| A silver challenge scroll stands at the hippy's hut | todo | `CreateHighlight` stub |
| While the camera is within 100 of the scroll and it is on screen, at most every 30 seconds the evil advisor steps out, points at it: "We got ourselves a task down here!" | todo | the shared notice script |
| Clicking the scroll or the hut starts the introduction | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A hippy villager comes out of the hut; he can't be killed, hurt by fire or picked up | partial | the three flags work (`SetIndestructable`, `SetHurtByFire`, `SetIdPickupable`); creating him doesn't |
| Generic script music 4 plays | partial | `StartMusic` plays the track; the scene never runs |
| In high detail he walks to his spot by the cauldron while the camera glides in (4 s); he faces the camera, a second later turns to the mushrooms and talks, pointing | todo | `MoveGameThing`, `SetFocus`, `OverrideStateAnimation` stubs |
| The camera turns to the mushrooms: hippy: "Hello. Have you noticed my mushrooms?" | todo | the man narrator |
| The camera moves; he looks puzzled: "Only they have special properties." | todo | |
| He faces the camera, gossiping: "The more they shake, the more powerful they are." | todo | |
| The camera moves again: "And I'm after the best one to do a little experiment." / "Could you please find it and drop it in my cauldron?" | todo | |
| He walks back into his hut, back to normal detail, as the camera pulls out | todo | |
| The scroll is entered in the challenge list at 0% with the reminder "We need to find the strongest mushroom and put it in the hippy's cauldron."; the music stops | todo | `Snapshot` stub |

## The mushrooms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The puzzle makes 18 mushrooms, scattered round it in a square, each of a slightly different size and turn; the scatter is worked out from each mushroom's number, so the layout is the same every game | todo | game engine; openblack has no mushroom puzzle |
| Held in the hand, each mushroom plays the mushroom-challenge effect on the mouse with its own pair of settings; the right one, the ninth, has the strongest setting (10000) with the smallest other value (13); one other has the same strength but a value of 50, the rest are weaker | todo | the two values are handed to the mouse as gain and frequency (which is which was not confirmed); no force feedback in openblack |
| A mushroom that is destroyed is made again at its spot | todo | |
| A mushroom taken from its spot and let go anywhere but the cauldron is put back at its spot | todo | |
| A mushroom dropped into the cauldron (the middle of the puzzle) is used up and decides the puzzle: the ninth wins, any other loses | todo | the script waits until the puzzle has been played and reads its state (`Played`, `GetObjectState` stubs) |

## The result

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the scroll is removed; the hippy comes out to the cauldron in high detail as the camera closes in; facing it: "Many thanks, Holy One. I hope you got the right one or my experiment could be fatal." | todo | |
| He walks back into the hut as the camera pulls out to a wide view; two seconds later a suspense sound starts; once he is inside he is removed | todo | `PlaySoundEffect` stub |
| Right mushroom: the camera shakes round the hut (radius 40, amplitude 0.1, 5 s); three seconds later the suspense sound stops and a level 2 healing miracle comes down onto the hut from 30 above it, lasting 5 s | partial | a script miracle at a place works (`SpellAtPos`, cast as the neutral player); `ShakeCamera` and the sound stop are stubs |
| Right mushroom: the scroll is completed (100%, alignment 0) | todo | |
| Wrong mushroom: the same shake; three seconds later the hippy is made again at the hut (unkillable), the hut is set on fire (burning speed 0.5) and a level 1 explosion miracle is cast on it from 30 above, 50 across, lasting 30; the suspense sound stops | partial | setting the hut on fire (`SetOnFire`) and the miracle cast work; making the hippy doesn't |
| Wrong mushroom: a second later the blast flings the hippy through the air so that he lands where the camera is two seconds later (thrown, with a random spin), and the scroll is completed (100%, alignment 0) | todo | the script's "set target" throws a thing to land at a place in a given time; `SetTarget` is a stub |
| Wrong mushroom: after the film the hippy can be killed again and is handed back to the game; with the right one he is never seen again | todo | `ReleaseFromScript` stub |
| Right mushroom: the good advisor steps out, points at the reward's spot: "Aha. We got the right mushroom. And there's a reward for us." | todo | |
| Wrong mushroom: two seconds later the evil advisor steps out: "Okay, okay. So we got the wrong mushroom. So sue us." | todo | |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A "Compassion" creature miracle dispenser is built beside the hut | todo | `CreateScriptObject` makes no dispensers; `SetMagicProperties` stub; dispensers: [../rewards.md](../rewards.md) |
| Its recharge time is set to 0 seconds (what a zero time does was not confirmed) | todo | the shared dispenser script checks the game time instead of the given time, so it always sets it |
| A film with the reward sting: the camera glides to the dispenser; the evil advisor steps out | todo | |
| If it is the first dispenser given by a film in the game: a did-you-know scroll ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") is placed by it, a signpost next to it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."); the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | todo | the shared dispenser script |
| Otherwise the evil advisor points at it: "Nice. Another of those cool Miracle Dispensers." | todo | |
| Then the dispenser's own help lines for its miracle are spoken | todo | which lines the game hands out for the compassion miracle was not traced |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The right-mushroom branch also stops a muffled child's crying sound that is never started | n/a | a leftover in the script; nothing to hear |
| The evil advisor's wrong-mushroom line ends with a "stop pointing" though he never points | n/a | harmless script slip |
| An earlier placing of the scroll's challenge-list entry during the film, and a final completion after the reward, are commented out | n/a | cut |
| The engine has a second mushroom puzzle kind that no land uses | n/a | see [../minigames.md](../minigames.md) |
| The end credits show a hippy beside a giant magic mushroom (among the credited firms is Immersion Corp.), and the fifth land's opening reuses the mushroom suspense sound | n/a | not this quest |
| A saved game keeps the puzzle, the hippy and the scroll | todo | openblack has no saved games |
