# Asset formats

Every kind of file Black & White reads from its install, and whether openblack reads it and uses what is in it. A
format only counts as done when the game's use of it works too; reading the file alone is partial.

**Progress: 21/42 done, 5 partial — 56%**

## World and lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Landscapes (`.lnd`): heights, blocks, countries and their materials, noise and bump maps | done | `components/lnd`; drawing and use: see ../terrain/ |
| Footpath files (`.fot`) that come with each land | partial | `src/Serializer/FotFile.cpp` reads the paths; walking them: see ../villager/ |
| Land scripts (`Land1.txt` …) that build a land's towns, villagers, forests and features | partial | `src/LHScriptX`; about 60 of 106 commands not done, see script_vm.md |
| Map scripts that set up the game (player count, date, time, language, which land and scripts to load) | todo | `MapScriptCommands.cpp` throws for every command and is never run |
| Camera exclusion zone files (`Data/Zones/*.exc`) | todo | not read; see ../camera/ |
| Multiplayer and online maps (`Online Maps/*.map`, `.thm`) | todo | see ../multiplayer/ |

## Models and animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh pack (`AllMeshes.g3d`) with all object meshes and their textures | done | `components/pack`, `Game::Initialize` |
| Loose meshes (`.l3d`): creatures, the hand, temple rooms, misc objects, sun and moon | done | `components/l3d`, `L3DLoader` |
| Zipped meshes and files (`.zzz`) in the temple and spell folders | done | `src/Common/Zip.cpp`, `Loaders.cpp` |
| The animation pack (`AllAnims.anm`) and loose animations (`.anm`) | done | `components/anm`, `L3DAnimLoader`; how each is played: see ../creature/, ../villager/, ../animal/ |
| Creature body files (`.cbn`) and their morph between good/evil and fat/thin | partial | `components/morph`; see ../creature/ for appearance |
| Creature animation spec (`ctrspec27.txt`) naming the mind's animations | done | `Game::LoadCreatureRigs` |
| The hand's animation bank (`hh.hbn`) and its spec (`hndspec5.txt`) | done | `Game::LoadHandAnimation`, `src/3D/HandAnimation.h` |
| The mesh and sound-action name headers (`AllMeshes.h`, `SoundAction.h`) the particle files refer to | done | `components/psys/include/EnumHeader.h` |
| Recorded hand demonstrations (`Data/HandDemo/*.hnd`) played by the tutorial | todo | not read |
| Dance and letter shape files (`.DAN`, `Scripts/Dance/*.dat`) | todo | not read (unconfirmed what draws the letter shapes) |
| The falling-spell camera file (`.cm2`) | todo | not read (unconfirmed use) |

## Images and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Raw textures (`.raw`, with a separate alpha file) | done | `components/rawimage`, `Texture2DLoader` |
| 16-bit images (`.16b`) used by the temple outside and sky | done | `src/Common/Bitmap16B.cpp` |
| Sky images in 555 colour (`.555`) | done | `src/3D/Implementations/Sky.cpp`; see ../sky/ |
| Player symbols and tattoo palette (`PlayersSymbols.raw`, `tattoocols.raw`) | done | `CreatureSkinArtLoader` |
| Creature portrait sets (`.cps`) and the colour lookup and contrast tables (`.clut`, `.cnv`) | todo | not read (unconfirmed use) |
| Light glows (`.glw`) placed in the temple and lands | partial | `src/3D/Light.cpp`; temple glows drawn, see ../rendering/ |
| The game font (`.fnt` with its metrics `.met`) | done | `src/Gui/GameFont.cpp` |
| The other fonts (`.fff`, `.srf`, `Font0-*.bmp`, `HelpFont.bmp`) | todo | not read (unconfirmed which screens use them) |
| Help sprites (`Data/HelpSprite`) | todo | not read; see ../interface/ |
| Save pictures and screenshots kept in the profile (`.raw`) | todo | see saving_and_loading.md |

## Effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Particle effect files (`Data/Spells/ZSpellFiles`) | done | `components/psys`; `test_particle_file`; effects themselves: see ../rendering/ |
| Gesture templates (`Gestures.jty`) | done | `components/gestures`; `test_gesture_file`; recognition: see ../gesture/ |
| Camera paths (`.cam`): temple rooms, symbols, fly-bys | done | `components/cam`, `CameraPathLoader`; playing them: see ../camera/ |
| Force-feedback mouse effects (`Data/Immersion/*.ifr`) | n/a | hardware no longer made |

## Sound and video

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sound banks (`.sad`) with their sample tables | done | `components/pack`, `SoundLoader`; behaviour: see ../audio/ |
| Music stored in banks as MPEG audio | partial | detected by file name in `Game::Initialize`; see ../audio/ |
| Loose wave files (`.wav`) | todo | not read |
| The small sounds bank (`SmallSounds.SAS`) | todo | not read (unconfirmed use) |
| Films (`.bik`): intro, the falling spell and others | todo | no film player; port notes wait for FFmpeg |

## Scripts, text and minds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The compiled challenge script (`challenge.chl`) | done | `components/ScriptLibrary`; running it: see script_vm.md |
| The game's text in UTF-16 info scripts (`InfoScript2.txt` and its patch and multiplayer versions) | done | `src/Gui/TextDatabase.cpp` |
| The info table file (`info.dat`) | done | `src/Parsers/InfoFile.cpp`; see info_tables.md |
| Creature mind files (no extension in `Scripts/CreatureMind`, `.erc` for creatures saved between lands) | done | `components/creaturemind`; see saving_and_loading.md |
| Profile files: help statistics (`helpstats.dat`), the player's creature (`creature.lhp`) | todo | see options_and_settings.md |
| Weather and country tables (`weatherinfo.lhw`, `country.lhw`) | todo | not read; openblack takes its climates from `info.dat` (unconfirmed whether the game reads these at all) |
| Speech files (`Data/Language/blackhal.*`) | todo | not read (unconfirmed what they drive) |
| The script library and language plug-ins (`Plug Ins/*.dll`) | n/a | openblack builds the script VM in |
