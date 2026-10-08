# The Creature Breeder

A standing silver scroll over a breeder's kennels on Land 1 and Land 4: the breeder shows whichever of the five special
creatures (leopard, horse, mandrill, gorilla, rhino) the player has unlocked, plus the creature they last swapped away,
and lets the player swap their creature's body for one of them. Without unlocked creatures and before any breeder swap,
he only apologises. The swap itself is in [creature_swaps.md](./creature_swaps.md).

**Land:** 1 and 4 (also prepared for 2 and 5, never started there) · **Giver:** the creature breeder, a man standing by the kennels · **Script:** CreatureBreeder · **Reward:** a special creature's body for the player's creature · **Repeatable:** yes (the scroll comes back forever)

**Progress: 0/28 done, 1 partial — 2%**

Sources: the breeder script and its land launcher (the original source text, matching the PC game's compiled
`challenge.chl`), the two land control scripts that start it, the land map scripts, the game's text table and the
executable's creature-availability check. openblack's state is judged on the physics work tree (`ob-wt-physics`): Land
1's control script stops at missing natives long before the creature is chosen, and Land 4's control script never runs
(the land-loading native does nothing), so the breeder never appears; the villager, highlight, dialogue, camera and swap
commands it needs are stubs in `src/CHLApi.cpp`. Every row is todo unless it says otherwise.

## Where and when it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1 starts the breeder as soon as the player's creature has been chosen (or straight away when skipping to creature selection), alongside the explorers' quest | todo | the land's control script stops long before; see [../land_1.md](../land_1.md) |
| Land 4 starts it at the beginning of the land, with the fish puzzle and before the ogre | todo | Land 4's control script never runs (see [../land_4.md](../land_4.md)) |
| Land 1's kennels are a Celtic house; Land 4's are a Norse house in the neutral Norse village in the far east | todo | positions in the launcher; houses come from `Land1.txt`/`Land4.txt` (made by openblack) |
| Land 2 and Land 5 positions are prepared too (each exactly on a Celtic house in a neutral Celtic village) but neither land's control script starts the breeder; only test launchers outside the compiled file do | n/a | never started by the game on those lands |
| A breeder (an ordinary man) is made at his start spot; he can't be hurt or picked up | todo | `SetIndestructable`, `SetIdPickupable` work but the villager is never made |
| The breeder's script runs for as long as the land does | todo | |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ten seconds after the breeder starts (and after every visit) a silver scroll appears 5 above the kennels | todo | `CreateHighlight`, `SetProperty` are stubs |
| While the camera is within 100 of it and it is on screen, at most every 30 seconds the evil advisor pops out, points at it and says "There's a Silver Reward Scroll down here, Boss." | todo | the shared notify script; `SpiritEject`, `RunText` stubs |
| Clicking the scroll or the kennels opens it; the scroll is removed | todo | |
| No challenge record is made: the line that would have opened one is commented out, and the title it named is "The Lost Brother" (another quest's title, with the Lost Brother's reminder), so the breeder never shows in the challenge log | todo | script quirk to keep |

## The breeder's show

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The current species of the player's creature is recorded | todo | |
| A cut scene: the breeder walks to his soap box and the camera glides to the kennels over 2 seconds | todo | `MoveCameraPosition`, `MoveCameraFocus` stubs |
| Each special creature that is unlocked and is not the player's current species is shown, in the order leopard, horse, mandrill, gorilla, rhino: a target sparkle for 3 seconds and the creature appears; each is placed about 6 further along a line from the last | todo | `IsCreatureAvailable` (always false in openblack) and creature creation are stubs |
| Whether a special creature is unlocked comes from a stored code in the game's settings (a value and its checksum); if it is missing or doesn't match, none are unlocked; every other species always counts as available | partial | `IsCreatureAvailable` is a stub that answers "no", which matches a game with no unlocks but never reads one; how the player got the codes is outside the game files |
| The player's previous creature is shown too, if a breeder swap has happened, it differs from the current species and it isn't already among the special creatures shown | todo | |
| With nothing to show, the breeder says "Sorry. I don't have any special Creatures for you." and walks back; this is what every player without unlocked creatures sees before any breeder swap | todo | |
| With one creature: a two-subject camera on it and the breeder, and "Hello. I have this special Creature for you." | todo | `StartDualCamera` stub |
| With several: the camera frames the first and last, and "Look. I have these wondrous Creatures you can choose from." | todo | |
| Each shown creature gets its own swap scroll and offer (see [creature_swaps.md](./creature_swaps.md#the-breeders-offer)) | todo | |
| The breeder walks back to his start; the scroll stays away until the offer closes | todo | |

## The advisors' chat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cut scene where both advisors discuss swapping: evil "So Boss, you can swap your Creature if you like." good "Yes, but do we really want to switch to a new Creature after all the training we've done?" evil "Nah, you got it wrong. Our Creature's mind will get transferred across." good "So it's just the body that's changing?" evil "Yeah. You got it. Kinda like cosmetic surgery." | todo | |
| It plays only when something is shown and the old and current species are both the ape (after swapping an ape away at the breeder and back to an ape), or for a "previous species" value that never occurs; in normal play it is almost never heard | todo | script bug to keep: the check looks meant for "never swapped" but compares against the wrong values |

## The swap at the kennels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1: the creatures are walked to two spots about 100 from the kennels, on ground some 35 higher; the cut scene's three camera spots are set but not used by the swap | todo | the camera parameters are passed along but the swap uses its own two-subject camera |
| Land 4: the creatures are walked to two spots about 30 to 50 from the kennels, at the same height | todo | |
| After the swap the old body walks back to the kennel spot, then vanishes with the others when the offer closes; the old species is kept to be offered next time | todo | |
| An offer that is left alone for its 60 to 120 seconds closes, and every creature on show vanishes in sparkles and smoke | todo | |
| Clicking a second creature while one is chosen cancels the choice | todo | |
| After the offer closes, the scroll returns 10 seconds later and the show repeats with the current unlocks | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The swap at the breeder records both species; the swap offered by quests only records the new one, so a creature swapped away by a quest is not offered back at the breeder | todo | |
| The comment above the breeder's swap asks people not to touch it because "it works and it was tricky to get to work" | n/a | developer note; nothing to do |
| The launcher for Land 1 and Land 4 finds the kennels as a house; the Land 2 and 5 entries use a plain marker | n/a | never started by the game on those lands |
