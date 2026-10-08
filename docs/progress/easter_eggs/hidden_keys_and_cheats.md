# Hidden keys and cheats

Keys the game reacts to that are not in its list of bindable actions, the developers' own key menus and cheats that were
left in the program but cut off from the player, and what remains of cheating in the online lobby. The ordinary,
rebindable keys are owned by [../interface/key_bindings.md](../interface/key_bindings.md); camera bookmarks on the
number keys by [../camera/places_and_bookmarks.md](../camera/places_and_bookmarks.md). Date-based surprises (special
days, the night voices, real weather, villagers named from the address book) are in [../pc_integration/](../pc_integration/).

**Progress: 1/5 done, 0 partial — 20%**

## Keys outside the bindings

These are tested only after a key has failed to match any bindable action, so rebinding an action onto one of them
hides it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| **Ctrl+Alt+T** saves a "creature snapshot": a file `creaturesnapshot.csn` in the current profile's folder, then plays a confirmation sound and a force-feedback jolt on a force-feedback mouse | todo | not in openblack; works in single player and online alike |
| The snapshot holds the camera's position and focus, the time of day and the sky it gives, the camera lens, whether the game is online (and the online game's name), then for each of up to eight human players who has a creature: their online player record, the creature's body and size, its head size, its colours, its whole 3D state, and the animation it is playing with its frame | todo | nothing in the game reads the file back; it matches the stills the creature web page expects (`creatureshot_N.jpg`, see [launch_switches_and_files.md](launch_switches_and_files.md)), so it was most likely for Lionhead's creature web site (unconfirmed) |
| **R** (when unbound) repeats the last miracle, if the hand is free to take it | todo | one of the two keyboard miracle keys; the pairing of R and M with "repeat" and "choose" is from the two keyboard miracle routines (unconfirmed which is which) |
| **M** (when unbound) starts choosing a miracle with the keyboard, if the hand is free to take it | todo | see the row above (unconfirmed) |
| **P** with no Shift, Ctrl or Alt held pauses and unpauses the game | done | `src/Game.cpp` (openblack also pauses with a modifier held) |
| There are no typed cheat codes: the game keeps a buffer of typed keys, but it only replays them, one by one, into the normal key handling each frame; nothing matches sequences | n/a | checked in the key-buffer handling; nothing to port |

## Developer key menus left in the program

Four developers had their own debug key menus, each with a help page. The code is still in the released program, but
nothing calls it: no pointer to it and no call to it exists anywhere in the executable, so no profile name, switch or
key reaches it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Jeremy's menu, H for its help page: K "Switch QuickClick Interface", C "Cheat", I "SlowDown Game", O "SpeedUp Game", M "Advance Night Time", F "Generating Footpaths", R "Free Move in Slow Motion", W "Divers (Help My Current Test)", Z "Go As Fast As Possible" | n/a | unreachable in the release; each key prints a "Jeremy - ..." message |
| What Jeremy's keys actually do in the release code: R toggles a free-movement flag, C sets it on, Z toggles "as fast as possible", and W (despite its label) toggles a spell cheat for the local player; I, O, M and F only print their message, because the routine they pass on to is empty; K does nothing | n/a | unreachable |
| Tim's menu: "O - Force an Out of Sync" (forces a multiplayer desynchronisation for testing) | n/a | only its help text is left; unreachable |
| George's menu: Shift+P reloads the weather system, W shows weather information | n/a | unreachable |
| Tom's menu: F generates the footpaths of every land in turn (lands 1 to 5, the three multiplayer maps and the Two, Three and Four Gods playgrounds), W turns on the wall-hugging (path-finding) debug view | n/a | unreachable |
| Nine named key layouts: "Default Key Config", "Jeremy Key Config", "Mark Webley Key Config", "Giles Key Config", "Jean-Claude Key Config", "Daniel's Key Config mofo, dont mess", "George Backer Conifg", "Tim's Conifg" and "TBL's Conifg" (sic) | n/a | the table of names is never read |

## Cheats compiled out of the release

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The interface's own cheat routine is empty in the released program | n/a | nothing to port |
| Creature cheats: learn everything, learn ordinary things, go to the next stage of growing up, make it fight, clear its help timers, and agree to every request | n/a | present in the program but no key or command reaches them in the release (unconfirmed for every build); openblack's debug spawner does similar things, see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| Player, worship site and temple cheats, and a cheat that hands a town to a player | n/a | present but unreachable (unconfirmed for every build); see [../worship/prayer_power.md](../worship/prayer_power.md) and [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| The computer gods' "find any old ..." helpers let them pick any town, field, tree, villager, creature or miracle anywhere, without having to know about it | n/a | part of the computer god, not a player cheat; see [../rival_gods/ai.md](../rival_gods/ai.md) (unconfirmed which of them the rival gods use) |

## The online lobby

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the internet lobby, a player whose name cannot be read is listed as "Cheater" | n/a | the lobby service is gone; see [../multiplayer/](../multiplayer/) |
| A hidden setting, "LeetLamer", in the game's setup registry key is read each time the list of internet game channels is built and passed on with every channel | n/a | what it changes is unconfirmed; the lobby service is gone |
