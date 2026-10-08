# Profiles

Each person who plays has a player profile: their name, symbol, creature's name and tattoo, their saved games, their
controls and what help they have seen. The Players page of the options picks, makes and edits them.

**Progress: 1/17 done, 4 partial — 18%**

## The Players page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists the players, the current one picked | partial | the list is there (`GameMenu.cpp`) with only the one name from the computer's login |
| Shows the player's name, symbol and creature's name | done | `GameMenu.cpp`; test `GameMenu.PlayersNameTheirCreatureAndPickASymbol` |
| The creature's name can be typed in | partial | the edit box works; the name isn't given to the creature or kept |
| The player picks one of sixteen symbols, its picture ringed as it comes up | partial | `SymbolPicture`; test `SymbolPicture.RingComesUpAndGoes`; the symbol chosen isn't used by the game |
| Create New Player asks for a name and makes a profile | todo | logs "not available yet" |
| Delete Player removes a profile, asking first | todo | logs "not available yet" |
| Edit Tattoo opens the tattoo editor for the creature | todo | logs "not available yet"; see ../creature/ for tattoos |
| Start Game starts a new game for the player from the first land | todo | logs "not available yet" |
| Picking another player during a game asks to restart | todo | |

## What a profile keeps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Profiles are kept on disk, one folder each, and the last one played is picked at start | todo | a skirmish loads the profile's creature and writes the player's alignment back to the profile about once a minute: see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Each profile has its own saved games | todo | see ../engine/ for saving |
| Each profile has its own key bindings | todo | see [key_bindings.md](key_bindings.md) |
| The player's symbol is shown in the world (on the temple, the creature's pen and spells) | partial | symbols are drawn by the particle and temple work for player one's fixed symbol; not from a profile |
| The tattoo and its colours are kept with the profile | todo | |
| What help the player has been given, and how often, is kept so it isn't repeated in the next game | todo | see [help_system.md](help_system.md) |
| A picture of the creature is kept with the profile | todo | |
| A web page about the creature is written from the profile for sharing | todo | (unconfirmed whether this is part of the shipping game or only of the creature upload tool) |
| Creatures can be uploaded and downloaded online | n/a | the service no longer exists |
