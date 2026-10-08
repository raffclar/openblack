# Network play

Playing other people over a local network or the internet: finding and joining a game in a lobby, chatting, and keeping
every player's game in step. How a network game is won and lost, and its end box, are in
[../story/losing_and_game_over.md](../story/losing_and_game_over.md); the end box's Statistics tab and the figures sent
online are in [../interface/statistics.md](../interface/statistics.md).

**Progress: 0/19 done, 2 partial — 5%**

## Finding a game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Join Online Game on the menu | todo | the button logs "not available yet" (`src/Gui/GameMenu.cpp`) |
| Multiplayer setup: choose a network and a lobby | todo | no network code in openblack |
| Games on the local network are found and joined | todo | no network code in openblack |
| Internet games are listed in the online lobby (GameSpy) | n/a | the original's lobby servers are gone |
| Logging in to the internet game with a name and password | n/a | the original's servers are gone |
| A channel can be created, joined, left and locked against newcomers (patch 1.1) | todo | no network code in openblack |
| The host can kick or ban a player from the channel | todo | no network code in openblack |
| Players see each other's ping | todo | no network code in openblack |

## The lobby

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Public chat and private messages between players | todo | no network code in openblack |
| Players pick their colours | todo | no network code in openblack |
| The host picks the island; players who lack it are told | todo | no network code in openblack |
| Missing islands are sent to players who lack them | todo | no network code in openblack |
| Teams: players change team, ask and invite others into theirs | todo | see multiplayer_rules.md |
| Starting: every player says ready, the files are sent, the game starts together; the start can be cancelled | todo | no network code in openblack |

## In the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player's commands are sent to all, and every game runs the same turns | todo | no network code in openblack |
| The game's random numbers are split into a shared stream and a local one | partial | `src/Common/GameRandom.h` keeps a synced and a local seed; nothing is synced over a network |
| Checksums detect a game out of step and the players wait to resync | todo | no network code in openblack |
| The host moves to another player if the host leaves | todo | no network code in openblack |
| Players can talk to each other in the game | todo | (unconfirmed whether the game sends voice or text) |
| Multiplayer help scripts run during network games | todo | see ../engine/script_vm.md |
| Belief grows at the multiplayer rate | partial | the multiplayer belief speed scale is read (`src/ECS/VillagerSpeed.cpp`); nothing sets a game as multiplayer |
