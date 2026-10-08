# Skirmish

A game against computer gods on one of the skirmish lands, started from the in-game menu (or a launch switch) without
a network. The lands are the land scripts in `Scripts/Playgrounds`. The setup box only picks the land: everything else
(opponents, their creatures, towns, miracles) comes from that land's script. Skirmish has been in the game since 1.00
(the box is in the 1.00 program). Patch 1.2's winning conditions and time limit apply only to network games. It is not
the Gods' Playground, the tutorial island the story reaches with F2 (see
[../story/tutorial_island.md](../story/tutorial_island.md)).

**Progress: 2/57 done, 11 partial — 13%**

See [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) for the computer gods,
[multiplayer_rules.md](multiplayer_rules.md) for the network rules and
[../scripts/playground_scripts.md](../scripts/playground_scripts.md) for what each land script builds.

## Reaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu Escape brings up has a Start Skirmish Game button, second of five | partial | the button is there (`src/Gui/GameMenu.cpp`) but logs "not available yet" (`Game::HandleInterfaceAction`); see [../interface/main_menu.md](../interface/main_menu.md) |
| Start Skirmish Game ends the current game at once, without asking, and opens the skirmish box | todo | the game's text has "Starting a Skirmish game end the current game. Do you wish to do this?", but version 1.2 never shows it |
| Leaving a story game this way quick-saves the story to its reserved slot, as any exit from the story does (except from the Gods' Playground island) | todo | see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The Start Skirmish Game button is greyed out during a network game | todo | |
| During a skirmish the button reads Leave Skirmish Game, and Join Online Game is greyed out | todo | noted in `GameMenu.cpp`, not done |
| During a skirmish (and a network game) the menu's five buttons are spaced further apart and start lower | todo | |
| The `SKIRMISH` launch switch opens the skirmish box straight away; `NEWGAME` overrides it, and it overrides `MULTIPLAYER` | todo | see [../easter_eggs/launch_switches_and_files.md](../easter_eggs/launch_switches_and_files.md) |
| With no player profile yet, the new-player box comes first and a new story game starts instead of the skirmish | todo | see [../interface/profiles.md](../interface/profiles.md) |

## The skirmish box

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A box titled "Choose a map for skirmish game." with a list of lands, Back on the left and Start Game on the right | todo | |
| The list holds every `.txt` file in `Scripts/Playgrounds`, in the order the folder gives them, so an added land shows too | partial | openblack reads the same folder into its levels (`Game.cpp`) and lists them, sorted and titled by each land's start message, in the debug menu's "Playground Islands" (`src/Debug/Gui.cpp`) |
| A land is shown by its file name without `.txt`. The exceptions are Two, Three and Four Gods, which read "Defeat another god to control this realm.", "Defeat two gods to control this realm." and "Defeat three gods to control this realm." | todo | so openblack's own test land, `construct.txt` (not a game file), would show as "construct" if copied into the folder; see [maps/construct.md](maps/construct.md) |
| Start Game is greyed out until a land is picked | todo | |
| There are no other options: no opponent count, difficulty, teams, colours, tribes, creature or time limit | todo | all of these come from the land script; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| Start Game loads `Scripts/Playgrounds/<name>.txt` | todo | |
| Back closes the box and returns to the story: its automatic save is reloaded and the main menu shown | todo | |

## Starting a skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The usual loading screen is shown while the land loads | todo | see [../interface/](../interface/) |
| Every script is reset first. The story's new-game steps (its control script, the challenge and save-room files, the skip-tutorial questions) are not run | todo | `Game::Run` starts the story's control script on any land; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| The land script is read and builds the land, its towns, temples, creatures and computer gods | partial | openblack loads the lands from the debug menu (`Game::LoadMap`). It stops at the first command it doesn't know, so "Death Comes To Those That Wait" stops at line 83 ([../scripts/land_script_commands.md](../scripts/land_script_commands.md)) |
| A land whose start camera is left at the origin (Four Gods, Death Comes To Those That Wait) starts with the camera zoomed onto the player's temple | todo | `StartCameraPos` always looks at the point given |
| When the land script reaches its start message, the land info box pops up. Its heading is the land's title, or for a number from 2 to 4 the matching "Defeat ... to control this realm." line; the description lines follow | todo | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty. Island Wars and Firestorm have no start message, so they show no box |
| The player's own creature is loaded from their profile's creature file and placed at their temple's creature spot | todo | see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The player starts with the alignment kept in their profile (limited to fully good or fully evil) | todo | see [../interface/profiles.md](../interface/profiles.md) |
| The game's random numbers start from the same fixed seed in every game, so the same moves play out the same way (unconfirmed whether anything reseeds it later) | todo | |
| The player's statistics start afresh | todo | see [../interface/statistics.md](../interface/statistics.md) |

## The lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| [Two Gods](maps/two_gods.md): two temples, the player and one computer god | partial | loads from the debug menu (`Game.cpp`, `src/Debug/Gui.cpp`), without a god's mind |
| [Three Gods](maps/three_gods.md): three temples, two computer gods | partial | as above |
| [Four Gods](maps/four_gods.md): four temples, three computer gods (its description says "Battle four other gods") | partial | as above |
| [Island Wars](maps/island_wars.md): four temples, three computer gods with creatures from Nemesis's mind file | partial | as above |
| [Firestorm](maps/firestorm.md): three temples, but no computer god is switched on, so the other two temples stand idle (as far as found) | partial | as above |
| [Death Comes To Those That Wait](maps/death_comes_to_those_that_wait.md), "By Fat Omen": temples for players one, three and four, two computer gods, and creatures for the temple-less players two and five to eight | partial | as above, and it stops early (see above) |
| [Construct](maps/construct.md): openblack's own test land (added in 2021), not one of the game's, with only the player's temple; copied into the playground folder, the box would list it like any other land | partial | loads from the debug menu like the others; see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |
| Each land's towns belong to their players, with their tribe | partial | towns load with their owner (land scripts, [../engine/script_vm.md](../engine/script_vm.md)) |
| Each land sets its players' and towns' influence multipliers | done | see [../engine/script_vm.md](../engine/script_vm.md) (land scripts) |
| Each land sets its own day length | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`); see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |

## Playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only miracles on offer are those the land gives: town miracles, dispensers, one-shot seeds and firefly rewards | todo | see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md); fireflies appear on every map through the nightly top-up, whatever its script places, and each map's reward table: [../nature/fireflies.md](../nature/fireflies.md) |
| No challenge or story script runs, so there are no scrolls, rewards or story advisor lines | todo | |
| Whether the advisors still give their general hints on a skirmish land | todo | undetermined: the help system is set up as in any game, but nothing found says which hints fire |
| The story's game-over script (when the temple's heart is destroyed) doesn't run in a skirmish | todo | |
| A skirmish runs on one machine, so pausing, the menu and game speed work as in the story | todo | see [../interface/main_menu.md](../interface/main_menu.md) |
| The automatic save still runs (it skips only network games and the Gods' Playground island). A save records that it is a skirmish, so a loaded one carries on as a skirmish | todo | see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| About once a minute the player's alignment is written back to their profile, so a skirmish changes the alignment the story and the next skirmish start from | todo | see [../interface/profiles.md](../interface/profiles.md) |
| Every ten turns each player's share of a running total is recorded for the statistics | todo | see [../interface/statistics.md](../interface/statistics.md) |
| There are no winning conditions and no time limit: patch 1.2 sets them up only for network games, and the time-limit check is skipped in a skirmish | todo | see [multiplayer_rules.md](multiplayer_rules.md) |
| Music | todo | undetermined: nothing skirmish-specific was found, so presumably the same music as on any land |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god among players one to four is out of the game once it has no temple (how a temple is lost is unconfirmed here) | todo | players five to eight can never be out; how a temple is damaged and destroyed: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A god that is out stops thinking and its creature is taken off the land | todo | see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| When another god is out, "<name> is out of the game." is shown | todo | |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" | todo | |
| When the player is out and only one god is left, the game ends and the end box says "You have lost the game." | todo | |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game; Watch Game closes it and play goes on, so the player can watch | todo | the game's text also has "You have lost the game. Click YES to watch the other players, NO to quit the Skirmish Game.", but version 1.2 never shows it; see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| The end box is edged in the player's colour and has tabs for the statistics and (unconfirmed) the winning conditions | todo | see [../interface/statistics.md](../interface/statistics.md) |
| Nothing is uploaded and no points are given (that is only for internet games) | todo | see [online_services.md](online_services.md) |
| Whether a skirmish can end without a winner (Firestorm's idle temples, players with no temple) | todo | undetermined |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Leave Skirmish Game asks "This will leave the current Skirmish game. Do you wish to do this?" | todo | |
| Yes ends the skirmish and opens the skirmish box again, for another land or Back to the story | todo | |
| Leaving a skirmish, or quitting the game from one, skips the quick-save the story makes on exit | todo | see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The player's creature is not saved back to their profile at the end of a skirmish (nothing found does it) | todo | |
