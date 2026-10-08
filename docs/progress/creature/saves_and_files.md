# Creature saves and files

A creature outlives any one game: its mind (everything it has learnt), its body and its tattoos are kept in creature
files that belong to the player's profile, so the same creature can be carried from land to land and into new games,
taken into skirmish and network games, and uploaded to the game's website. The game's own computer gods and skirmish
opponents also come from creature files.

**Progress: 15/36 done, 4 partial — 47%**

## Creature mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mind is saved in its own file in the game's creature mind folder, named for the creature | partial | files are read from and written to `Scripts/CreatureMind` (`components/creaturemind`, debug spawner "Save mind as…"); the game names and writes them by itself, openblack only on request |
| A small companion file keeps the creature's physique beside its mind file | todo | the companion files are not read |
| The file keeps the creature's species, name, the name it was saved as and the profile it belonged to, the last two lightly encrypted | done | `MindFile`; test `EncryptionRoundTrips` |
| It keeps the 40 desires with their sources and thresholds | done | `MindFile`, `CreatureMindModel` |
| It keeps the examples its decision trees learnt from | done | `MindFile`, `CreatureMindModel::Load` |
| It keeps its opinion of each action, how often it has seen actions and miracles, and which it knows | done | `MindFile` |
| It keeps how it sees the player, the desires it thinks the player has, and what towns want | done | `MindFile` (kept; partly used, see [beliefs_and_opinions.md](beliefs_and_opinions.md)) |
| It keeps its alignment, stage of growing up, body (age, energy, poo, size and so on) and tattoos | done | `CreatureMindFileBody`; tests `TakesTheBodyAFileKeeps`, `TattooWordsRoundTrip` |
| Every version from the earliest the game wrote to the newest (33) is read, each with its own fields | done | tests `OlderVersionsLeaveOutTheirFields`, `ReferenceSavesParseAndRoundTrip` |
| A file read and written again is the same byte for byte | done | test `CurrentVersionRoundTrips`, `KeepsTrailingBytes` |
| Files that aren't minds, or are newer than known, are refused, and the creature starts with a fresh mind of its species | done | test `RejectsWhatIsNotAMind`; `creature::CreatureMind::Loaded` |
| Files whose species row is unknown give no creature | done | test `UnknownSpeciesIsNoCreature` |
| Values out of range in a file are kept within bounds | done | test `OutOfRangeValuesAreKeptInBounds` |
| A fresh mind is written as a current file | done | test `FreshMindWritesACurrentFile` |
| Mind files are loaded once through the resource cache | done | `resources::CreatureMindLoader` |
| The game checks the mind and body loaded agree, and fixes or refuses them | todo | |
| The game keeps a copy of the mind in memory to restore | todo | |
| The game deletes a profile's creature files when the profile or creature is deleted | todo | |

## Carrying the creature on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is saved as the profile's creature, so a new game can start with it | todo | `LOAD_MY_CREATURE`, `CURRENT_PROFILE_HAS_CREATURE`, `IS_KEEPING_OLD_CREATURE` are stubs |
| Moving from one land to the next, the player's creature comes along as it was | todo | it is saved to its own files when the land is cleared, not carried through the vortex: [../story/portals.md](../story/portals.md#the-creature) |
| Scripts can load a creature from a file into the land | partial | land scripts' create-from-file works (`FeatureScriptCommands::CreateCreatureFromFile`); the challenge scripts' `LOAD_CREATURE` is a stub |
| Scripts can wipe a creature's mind | todo | `CLEAR_ACTOR_MIND` is a stub; the debug tools can (`CreatureMindSystem::ClearLearning`) |
| A saved creature can be spawned with its species, name, alignment, strength, size and tattoos | done | debug spawner's saved-creature list (`CreatureSpawnerMindFiles.cpp`) |
| A mind can be loaded into an existing creature | done | `CreatureMindSystem::LoadMind`, debug spawner |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature, its mind, body, spells, leash and what it is doing are saved and loaded with the game | todo | no saved games; see [../engine](../engine/) |
| Loading a saved game puts the creature back mid-action | todo | |

## Sharing creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game makes a web page about the creature from a template: its name, age, alignment, health, energy and strength, with pictures | todo | |
| The Creature Cave can take a picture of the creature for that page | todo | see [creature_cave.md](creature_cave.md) |
| The creature upload tool sends a creature file to the game's website under the player's account | n/a | the service no longer exists |
| A packed creature file is sent between machines | todo | see [../multiplayer](../multiplayer/) |

## Network and skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In a network game each player brings their own creature from their profile | todo | see [../multiplayer](../multiplayer/); a skirmish loads the profile's creature too: [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| A creature's state is added to the game's checksum, so the machines can tell when they drift apart | todo | |
| Selecting another player's creature waits for the network to agree | n/a | covered under network play |
| In skirmish, computer players' creatures load from the template minds | todo | |
| A computer player balances its creature's knowledge to its difficulty | todo | unconfirmed: ../rival_gods/skirmish_opponents.md found no difficulty setting anywhere; needs checking what "difficulty" this reads |

## Debug output

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game can print a creature's information and statistics | partial | the debug spawner shows a creature's body, mind and file (`CreatureSpawner*.cpp`); the game's own print-out isn't reproduced |
| A land script command lists all creatures | todo | `OUTPUT_CREATURES` throws "not implemented" (`MapScriptCommands.cpp`) |
| Cheats: learn everything, learn ordinary things, next stage of growing up, make it fight | partial | the debug spawner can teach and start fights; see [development_phases.md](development_phases.md) |
