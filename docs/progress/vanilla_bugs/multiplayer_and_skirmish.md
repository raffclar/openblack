# Vanilla bugs: multiplayer and skirmish

Bugs in the original game's skirmish and network play and in the shipped skirmish land scripts
(`Scripts/Playgrounds/`).

**Bugs: 9 (soft-locks and lost progress: 1)**

## Soft-locks and lost progress

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Start Skirmish Game ends the current game at once without the confirmation the text table has for it ("Starting a Skirmish game end the current game. Do you wish to do this?") | The game in progress is thrown away without a question | Version 1.2 never shows the line | [skirmish.md](../multiplayer/skirmish.md) | undecided |

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The online winning conditions switch off the building goals (houses, wonders, new towns, converts) by default, with a developers' note that a bug was still to be fixed | Those goals are not offered for online games | The game's condition list | [firestorm.md](../multiplayer/maps/firestorm.md) | undecided |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| A god the computer takes over is named with a random line from the first 99 of the text table, which are villagers' grumbles, not names (unconfirmed that players saw it) | A rival god named something like "Work, work, work. I'm sick of it." | The naming reads the text table by index | [skirmish_opponents.md](../rival_gods/skirmish_opponents.md) | undecided |

## Data and text errors

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| *Death Comes To Those That Wait* writes its 119 named villagers with a misspelt command (`CREATE_SPECIAL_TOWNVILLAGER`), which the game skips | None of the land's named villagers is ever made | 119 lines in the shipped file, none with the correct spelling (checked in the game data) | [land_script_commands.md](../scripts/land_script_commands.md), [playground_scripts.md](../scripts/playground_scripts.md), [death_comes_to_those_that_wait.md](../multiplayer/maps/death_comes_to_those_that_wait.md) | differs: openblack stops loading the land at the first of these lines instead of skipping it |
| *Death Comes To Those That Wait* gives town 21 a misspelt miracle (`EXPLSION_PU1`) | That town never offers the miracle | One line in the shipped file (checked in the game data) | [land_script_commands.md](../scripts/land_script_commands.md) | undecided (the progress file says to ignore it as the game does) |
| *Death Comes To Those That Wait* gives two towns the number 15 (a Greek town and the Tibetan town in the middle) | Undetermined what the game does with the repeated number | Two town lines in the shipped file (checked in the game data) | [death_comes_to_those_that_wait.md](../multiplayer/maps/death_comes_to_those_that_wait.md) | undecided |
| *Death Comes To Those That Wait* places 86 fireflies in two lists that partly repeat Three Gods' spots, and only 35 of them stand on one of the map's trees; its last firefly reward table is Land 2's | The other 51 are removed at the first nightfall and the land tops up to 50 instead | 86 firefly lines in the shipped file (count checked in the game data); spots compared with the map's trees | [fireflies.md](../nature/fireflies.md), [death_comes_to_those_that_wait.md](../multiplayer/maps/death_comes_to_those_that_wait.md) | undecided |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| One firefly reward line in *Death Comes To Those That Wait* lacks its closing bracket | nothing visible: the game reads it anyway | One line in the shipped file (checked in the game data) | [land_script_commands.md](../scripts/land_script_commands.md) | undecided |
| Each clan in the clan list carries a yes/no flag that the game stores but never reads | nothing visible | Checked in the Windows and Mac versions | [clans.md](../multiplayer/clans.md) | undecided |
