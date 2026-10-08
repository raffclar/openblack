# File system and packs

How the game finds its files in the install, how it handles the paths its own scripts write, and the pack files that
bundle many meshes, animations, textures or sounds into one. Each file format is listed in
[asset_formats.md](asset_formats.md).

**Progress: 11/19 done, 5 partial — 71%**

## The install

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's files are found under its install folder: `Data`, `Scripts`, `Audio`, `Profiles`, `Plug Ins` and their sub-folders | done | `src/FileSystem/FileSystemInterface.h` (named game folders) |
| The install folder comes from the registry or the command line | done | `src/main.cpp`; see options_and_settings.md |
| A folder's files can be listed, to load every creature mesh, texture, temple camera or land in it | done | `FileSystemInterface::Iterate`, used in `Game::Initialize` |
| The story's lands are the land scripts in `Scripts` | partial | found by listing the folder (`Game::Initialize`); port notes: the lands should come from the challenge script |
| Extra lands put in `Scripts/Playgrounds` can be played as playgrounds | partial | loaded into the level list; starting one is only through the debug tools (see ../debug/) |
| Downloaded multiplayer lands go into `Online Maps` | todo | see ../multiplayer/ |
| The game writes only into the player's profile folder (saves, screenshots, statistics) | todo | openblack writes nothing to the install yet; see saving_and_loading.md |

## Paths and letter case

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Paths in the game's scripts (backslashes, a leading `.\`, any mix of upper and lower case) still find their file | partial | `FileSystemInterface::FixPath` fixes backslashes and the case of a few known folder names only |
| Any file is found whatever the case of its name, on file systems where case matters | partial | `DefaultFileSystem::FindPath` checks the path as given; there is no general case-insensitive lookup |
| Names looked up in the game's tables (abodes, features, statics) ignore case, as the game's do | done | port notes (case-insensitive info lookups) |

## Packs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is a file of named blocks, each read on its own | done | `components/pack`; `test/pack` |
| The mesh pack holds every object mesh, with a texture block of its own | done | `PackFile::GetMeshes`, `GetTextures` |
| The animation pack holds every animation, numbered | done | `PackFile::GetAnimations` |
| Sound banks are packs of sample headers and sample data, with tables tying animations to sounds | done | `Game::Initialize` reads every bank in `Audio`; see ../audio/ |
| A sound bank's samples can be streamed from its headers rather than read whole | todo | port notes: header streaming waits for the audio banks work |
| Zipped files are unzipped as they are read | done | `src/Common/Zip.cpp`, `Loaders.cpp` |
| The info table file is a pack whose blocks are the tables | done | `src/Parsers/InfoFile.cpp`; see info_tables.md |

## Patches and languages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The patch's text script is read after the game's, its texts replacing the game's of the same name | done | `src/Gui/TextDatabase.cpp` |
| The game's text and speech are in the language of the install | partial | whatever text the install has is used; choosing a language in the map script is todo |
| Player-made data overrides (mods) | n/a | the game has none; openblack left its mod support out on purpose |
