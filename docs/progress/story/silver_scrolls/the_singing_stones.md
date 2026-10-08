# The Singing Stones

A silver scroll on the first land: a ring of eight stone holders east of the player's village has only three of its
singing stones left. A hippy who lives in a hut beside it asks for the missing five; the player finds them around the
land and sets each in its own hole so that the ring plays a rising scale. Done, the stones sing for good, a short storm
rolls over the circle and the player is given a food miracle dispenser.

The second land has its own singing stones ([the_singing_stones_land_2.md](./the_singing_stones_land_2.md)); a cut
stone-circle quest, never compiled, is [The Miracle Stones](./the_miracle_stones.md).

**Land:** 1 · **Giver:** the hippy, from his hut beside the stone circle · **Script:** SingingStoneCircle · **Reward:** a food miracle dispenser in the middle of the circle · **Repeatable:** no

Sources: the challenge script source (`SingingStoneCircle.txt`, which matches the shipped `challenge.chl`), the shared
reward, reminder and did-you-know scripts it runs, the land control script, the game's text table and the script sound
list (`Audio/SFX/Script/ScriptSfxEnum.h`). openblack is judged on the physics work tree (`ob-wt-physics`): of the 74
script functions the quest and the scripts it runs need, 56 still only log "not implemented" in `src/CHLApi.cpp`, and
the land's set-up script, which runs before this one, already stops at a missing function (see
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md)), so the circle is never even built. Rows are
partial only where the openblack function the row needs already works. The land as a whole is in
[../land_1.md](../land_1.md); the stones as objects are in [../../nature/one_shot_features.md](../../nature/one_shot_features.md).

**Progress: 0/66 done, 9 partial — 7%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle is set up as the land begins: the land control script starts this quest right after the land's set-up, before the family leads the player home, and it then waits for its start signal | todo | the land control script; the set-up script before it stops at a missing function |
| The silver scroll is offered only after the creature's first lessons (seeing its pen, learning to eat and being punished; just its pen when the creature training is skipped), at the same moment as the Immersion Mushrooms | todo | the land control script raises the start flag; see [../land_1.md](../land_1.md) |
| The scroll (a challenge highlight) stands by the hippy's hut, about 48 from the circle's centre, not at the circle itself | todo | `CreateHighlight` is a stub |
| While the scroll is waiting and the camera is within 100 of it with the scroll in view, the good advisor steps out, points at it and says "Look. Something for you to do here.", at most once every 30 seconds (the first time at once) | todo | the quest's own copy of the shared challenge notice; `GameThingFieldOfView`, `SpiritEject`, `SpiritPointPos`, `RunText` are stubs |
| The notice ends when the scroll is clicked, or when the circle has already been completed; the scroll is then made active | todo | `GameThingClicked`, `SetActive` are stubs |
| The hippy does not exist until the scroll is clicked: he is then made at his hut (a hippy villager) | todo | creating a villager from a script does nothing (`CreateScriptObject` only makes mobile statics) |
| If the player completes the circle before the scroll appears or before it is clicked, there is no introduction and no scroll entry: the hippy is simply made at his hut and the ending and reward follow at once | todo | |

## The circle and its stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle's centre is a fixed spot; eight holders (singing stone bases) stand around it on a ring 17.5 across from the centre, one every eighth of a turn | partial | `Create` makes the bases as mobile statics (`MobileStaticArchetype`), but the script never gets here |
| Stones 1, 2 and 6 already stand in their holders (the first two and the sixth going round the ring) and can't be moved or picked up | partial | `Create`, `SetIdMoveable` and `SetIdPickupable` work; never reached |
| The five missing stones (3, 4, 5, 7 and 8) lie scattered over the land, between about 140 and 1,175 from the circle (stone 7 close by, stones 3 and 8 at the far west) | partial | `Create` makes them; never reached |
| Three wrong stones that look the same lie closer, about 280, 360 and 650 from the circle | partial | as above |
| An influence ring of radius 50 is put round the circle, and each of the five missing stones carries its own small influence, so the player can pick them up wherever they lie; the circle's ring stays for the rest of the land | todo | `InfluencePosition`, `InfluenceObject` are stubs |
| Until the circle is complete, any of the five missing stones that is destroyed is made again where it started, with a new influence; the wrong stones and the three fixed ones are never remade | todo | the script also keeps a "too far away" limit of 200 that it never uses: only destroyed stones come back |
| Until then the five missing stones are also kept turning to face the circle's centre, one stone per script turn | todo | `SetFocus` is a stub |

## The notes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each right stone has its own note, the eight going up a scale round the ring; all three wrong stones play the same sour note | todo | script sounds "stone 1" to "stone 8" and "bad stone 1"; the bank (`Audio/SFX/Script/Scriptsfx.sad`) isn't loaded and `PlaySoundEffect` is a stub |
| Tapping (clicking) any of the eleven stones, wherever it is, plays its note there with a short sparkle | todo | `GameThingClicked`, `ClearClickedObject`, `SpecialEffectObject` are stubs; the sparkle is the "command succeeded" spot effect |
| The tap check looks at the stones in three groups (1 to 4, 5 to 8, the wrong ones), one group per script turn, and stops once the circle is complete | todo | |
| From the moment the land begins the circle plays itself round, one holder every half second with a two-second pause before the first: the stone in each holder sounds its note (a slightly longer sparkle), empty holders are silent | todo | so the three fixed stones are heard from the start, and the order the player has built is heard as it stands |
| On its turn, a holder takes any singing stone (right or wrong) within 2.5 of it, snaps it into the holder and turns it to face the centre; a holder found empty is marked empty | partial | `SetPosition` works; `CallNear` (finding the stone) and `SetFocus` are stubs. Undetermined: whether a stone still in the hand counts |
| Because each holder is looked at once per round, a stone put down takes up to about six seconds to snap in | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cut scene with the fourth generic script theme; the hippy is shown in high detail | partial | `StartMusic` works (`src/Audio/GameMusic.cpp`); `StartCameraControl`, `SetHighGraphicsDetail` are stubs |
| The camera flies over 4 seconds to a spot north-east of the circle, following the hippy as he walks out to its edge; after 2 seconds it closes in on him over 2 more | todo | `MoveCameraPosition`, `SetFocusFollow`, `MoveGameThing`, `HasCameraArrived` are stubs |
| The hippy faces the camera with a gossiping gesture: "Hi, man. Neat stone circle, huh? It's a bummer that some are missing." | todo | `SetFocus`, `OverrideStateAnimation` (the gesture), `RunText` are stubs |
| The camera cuts to a view across the circle; the good advisor steps out: "Sounds like we can help, there."; the evil advisor: "What? Help this deluded sack of burnt-out neurons?" | partial | the cut works (`SetCameraPosition`, `SetCameraFocus`); the advisors are stubs (`SpiritEject`) |
| The hippy faces the camera again: "The Stones have to play a scale. That's what we need." | todo | |
| Both advisors go home; the hippy: "Tap a stone to hear its note." (with the mouse-button picture the text table marks) | todo | `SpiritHome` is a stub |
| The hippy walks back to his hut while the camera rises over 3 seconds to look west across the circle; the dialogue box closes | todo | `GameCloseDialogue` is a stub |
| The scroll's entry is made in the challenge list at no success and no alignment, titled "The Singing Stones", with the reminder "Let's find the stones to put in the stone Circle." (good advisor) | todo | `Snapshot` is a stub; the shared reminder script steps out whichever advisor owns the line |
| The hippy goes back to normal detail and the music stops | partial | `StopMusic` works |

## Wrong order or wrong stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| These checks start only after the introduction | todo | |
| The first time all five empty holders are filled with right stones in the wrong order, the scroll goes to half success (0.5) with the reminder "We've found all the stones but the order is wrong." | todo | `UpdateSnapshot` is a stub |
| With the hippy alive, a cut scene by his hut: he walks up slowly; good advisor: "These are the right stones, but they're in the wrong order."; the advisor goes home and the hippy, unimpressed: "The Stones have to play a scale. That's what we need."; he walks back and the camera returns to where it was | todo | |
| With the hippy dead, the good advisor alone: "Hmm. That doesn't sound right to me." | todo | |
| The first time all five holders are filled and any of them holds a wrong stone, the scroll goes to 0.25 with the reminder "We've found enough stones but some really don't sound right." | todo | |
| With the hippy alive, a cut scene by his hut: he walks up, after 2 seconds plays an unimpressed gesture: "This sounds pretty inharmonious to me." / "Are you sure you've found the right stones?"; he walks back and the camera returns | todo | |
| With the hippy dead, the good advisor: "Hmm. This doesn't seem right. Some of the stones are wrong. Listen." | todo | |
| Each of the two comments is made once only; the progress mark can therefore go from 0.25 to 0.5 or from 0.5 to 0.25 depending on which happens first | todo | |
| Quirk: once the holders have been filled with right stones only, the "filled" check stops for good, so wrong stones put in afterwards are never commented on | todo | |

## Rules for success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The circle is complete when each of the five missing stones is in its own holder (stone 3 in the third holder, and so on), which makes the ring's notes rise in order all the way round | todo | |
| There is no time limit, no failure and no way to abandon it; it stays open until done | todo | |
| The five placed stones are then fixed: they can't be moved or picked up | partial | `SetIdMoveable` and `SetIdPickupable` work |
| The tap check and the stone-remaking stop; the ring plays two more full rounds before the quest counts as finished | todo | |
| Nothing in the scripts looks at who placed the stones: the hand, the creature carrying or throwing them, or anything else that moves them | todo | |

## Ending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From then until the end of the land the singing-stones music is attached to the circle's centre, and every script turn a random one of the eight stones sparkles | todo | `AttachMusic` and `SpecialEffectObject` are stubs; the music (`SingingStonesA.sad`) is named in `src/Audio/GameMusic.cpp` |
| The eight holders are let go by the script (they stay in the world) | todo | `ReleaseFromScript` is a stub |
| With the hippy alive: a cut scene in high detail looks at him as he walks (at half speed) to just outside the circle with the camera following; after 4 seconds he says "Wow. Now this is a good vibe. Amazing." while the camera moves behind him over 4 seconds; on arriving he cheers (the crowd's "won" animation) | todo | |
| The camera then moves over 3 seconds to the start of a recorded camera path and runs it | todo | `ConvertCameraPosition`, `ConvertCameraFocus`, `RunCameraPath` are stubs. Undetermined: which camera path file the path comes from |
| A monsoon is made over the circle: it lasts 50 seconds with a 5-second fade, rain at full, no snow, full overcast, clouds 10 at height 70, sheet lightning 2 to 7 (1 to 5 if the hippy is dead) and practically no forked lightning, a 20 inner and 50 outer radius, not blown by the wind | todo | `ChangeWeatherProperties` and the other weather-property commands, `SetAffectedByWind` are stubs; creating a weather thing does nothing (`CreateScriptObject`) |
| The scroll's entry goes to full success (alignment 0) | todo | the reminder given is the "some really don't sound right" line, a slip in the script; with the scroll finished it should never be heard |
| The good advisor's lines: "Something is happening." / "The stones are singing together again." | todo | |
| With the hippy alive the camera then returns over 4 seconds to where it was and he is let go to live normally | todo | |
| With the hippy dead the same path, storm and lines play without him, and the camera is not taken back afterwards (the scene simply ends) | todo | |
| The scroll is deleted at the very end | todo | `ObjectDelete` is a stub |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A food miracle dispenser (the Norse dispenser building) is built in the middle of the circle at no angle, set to food and switched on | todo | `CreateWithAngleAndScale` doesn't make dispensers (`CreateScriptObject`), `SetMagicProperties`, `SetActive` are stubs; dispensers themselves: [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Quirk: the reward script means to set the refill time only when one is given, but tests the game's clock instead, so it always sets the refill time to the 0 seconds asked for | todo | `SetTimerTime` is a stub. Undetermined: what a zero refill time does to a dispenser in the engine |
| A cut scene plays the reward sting and flies the camera over 4 seconds to look at the dispenser from 21.5 to the side and 14 up | todo | see [../rewards.md](../rewards.md) |
| If it is the first dispenser the player has been given: a did-you-know scroll is placed 5 in front of it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), a signpost is put up beside it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), and the evil advisor points at it: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then at the signpost: "Click on the signpost for more info." | todo | did-you-know scrolls and signposts are stubs (`CreateHighlight`, `HighlightProperties`) |
| Otherwise the evil advisor points at it: "Nice. Another of those cool Miracle Dispensers." | todo | the other land 1 quests that give dispensers decide which comes first |
| After the scene the dispenser's own help lines for its miracle are spoken in turn, the advisors who own them stepping out (or clinging to the screen edge for a two-line group) | todo | `GetFirstHelp`, `GetLastHelp` are stubs. Undetermined: which lines the food dispenser's help range holds |

## The hippy and his hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hippy lives in a hut that is part of the land, about 56 east of the circle | todo | getting the hut from the script (`Call`) is a stub |
| If the hippy dies (his health reaches nothing) the good advisor says "You killed him. How dreadful of you." once, and from then on his comments are replaced by the advisors' | todo | `GetProperty` (health) is a stub |
| The first time his hut drops below 80% health with him alive: a cut scene by the hut, he walks up and faces the camera, unimpressed: "You wrecked my house. Bad karma, guru."; he then wanders slowly round that spot and the camera cuts back | todo | `SetScriptState` (wander) is a stub |
| With him dead (or gone), the evil advisor instead: "Ha. The hippy would be turning in his grave." / "If his body was in a grave. Not being pecked by vultures." | todo | |
| The hut is watched only from the introduction on, and the watch never ends until the hut is damaged, even after the quest is over | todo | |
| Killing the hippy or wrecking his hut changes no alignment and doesn't stop the quest | todo | every success mark gives alignment 0 |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part in the script; it can still carry, throw or knock stones into place, and can kill the hippy or wreck his hut, with the same results as the player | todo | |

## Unused and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text table has no lines 1 to 5 or 11 to 16 for this quest: cut before release | n/a | nothing to play |
| A starting spot is set for stone 6, which is never used (stone 6 is one of the fixed ones) | n/a | |
| A first, commented-out scroll entry in the middle of the introduction (moved to its end) | n/a | |
| The wrong-stone notes "bad stone 2" and "bad stone 3" exist in the sound list, but all three wrong stones play "bad stone 1" | n/a | |
| An older version of the land control script (`LandControl1-steve.txt`, not in the shipped program) started a stone script of another name instead, whose source is not among the shipped scripts | n/a | not shipped; see the shared-and-unused quests |
