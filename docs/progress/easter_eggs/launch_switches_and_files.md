# Launch switches and stray files

The original game's command-line switches, the alternative settings file that can point it at other challenge
scripts, and the files it quietly writes about the player's creature. openblack's own switches are in
[../debug/command_line_tools.md](../debug/command_line_tools.md); online services (the creature web site, the lobby)
are in [../pc_integration/](../pc_integration/) and [../multiplayer/](../multiplayer/).

**Progress: 0/13 done, 1 partial — 4%**

## Command-line switches

Each switch starts with `-` or `/` and is matched by its start, in capitals, anywhere on the command line.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `NEWGAME` skips the main menu and starts a new game at once | partial | openblack's `--start-level` loads a land directly (`src/main.cpp`), but there is no main-menu flow to skip |
| `MULTIPLAYER` goes straight to the online lobby (ignored if `NEWGAME` came first) | todo | |
| `SKIRMISH` goes straight to the skirmish set-up box | todo | `NEWGAME` overrides it, and it overrides `MULTIPLAYER`; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| `PREINTROVIDEO` plays the video before the intro, which otherwise plays only while no player profile exists | todo | see [../video/](../video/) |
| `FORCEINETCONN` and `NOINETCONN` force, or skip, the start-up check for an internet connection | n/a | only the long-gone online extras use the connection; see [../pc_integration/](../pc_integration/) |
| `VERSION` writes `mpver.txt` beside the game holding the program's checksum (the number multiplayer uses to match versions), then carries on | todo | |
| `NOLOADMUSIC` skips loading the music | todo | |
| `SETTINGS:<file>` reads that file (up to the next space) instead of the usual settings file | todo | |
| In that settings file, a line `QUESTDIR <folder>` makes the game load its challenge scripts from another folder, so a modded challenge file can be played without replacing the original | todo | lines starting with `//` are comments; openblack always reads `Scripts/Quests/challenge.chl` |
| `CONVERT` is read when a game starts | todo | what it does is unconfirmed |
| `EDITOR` and `LAND` are recognised but nothing in the released program uses them | n/a | left over from the developers' build |

## Files the game writes about the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| About once a minute of play (every 600 game turns), the game writes a web page about the player's own creature, `html/<profile name>.html` in the profile's folder, from the template `Scripts/creature_html.tmpl` | todo | the same feature is one row in [../creature/lessons_and_help.md](../creature/lessons_and_help.md); the exact conditions (only the local player's creature, not in some states) are partly unconfirmed |
| The page, titled "Creature Cave", has a parchment frame and a floating spirit animation, and lists the creature's name, age, alignment, strength, health, energy, villagers killed and battles won; the template can also show mushrooms eaten, poos done, battles fought, creatures, animals and people killed, amount of poo, itchiness, thirst, tiredness, fatness, its beliefs, its desires and the miracles it has received | todo | the pictures it shows (`creatureshot_N.jpg`) are not made by the game |
| At the same moment the creature's mind and body are saved to `Scripts/CreatureMind/<id>.erc` and `Scripts/CreatureMind/Physique<id>.erc` | todo | |
| The separate Creature Upload utility (`CreatureUpload.exe`) sent the profile's creature to Lionhead's creature site, where it could be viewed at a page of its own name | n/a | the site is gone; see [../pc_integration/](../pc_integration/) |
| Ctrl+Alt+T writes `creaturesnapshot.csn` | todo | see [hidden_keys_and_cheats.md](hidden_keys_and_cheats.md) |
| The program still knows the paths of several developer files: `savedmap.txt` (and `..\newtestbed\savedmap.txt`), `MemoryLeaks.txt`, a saved copy of `Data\PhysicsConstants.txt`, out-of-sync logs and a physics log | n/a | developer leftovers; `MemoryLeaks.txt` even ships in the game folder; which of them a player can trigger is unconfirmed |
