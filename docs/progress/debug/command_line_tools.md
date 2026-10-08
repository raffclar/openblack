# Command-line tools

The small programs in `apps/` for looking inside the game's files and making new ones, each built on the file-format
component in `components/` that the game reads the same files with.

**Progress: 15/23 done, 0 partial — 65%**

## Tools

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| packtool: list a pack's blocks, view a block's bytes, the info block, meshes, textures, animations and sounds | done | `apps/packtool/packtool.cpp`, `components/pack` |
| packtool: extract a block, and write raw data and mesh packs | done | |
| l3dtool: print a mesh's header, skins, extra points, bones, footprints, names and metrics | done | `apps/l3dtool/l3dtool.cpp`, `components/l3d` |
| l3dtool: write a mesh from glTF and extract a mesh to glTF | done | |
| anmtool: list an animation's keyframes and their contents | done | `apps/anmtool/anmtool.cpp`, `components/anm` |
| anmtool: write an animation from glTF | done | |
| lndtool: print a land's low resolution textures, blocks, countries, materials and the rest | done | `apps/lndtool/lndtool.cpp`, `components/lnd` |
| lndtool: write a land from a height map, noise and bump maps | done | |
| glwtool: list glows and their contents, and read and write them as JSON | done | `apps/glwtool/glwtool.cpp`, `components/glw` |
| camtool: print a camera path's points, and write one from points | done | `apps/camtool/camtool.cpp`, `components/cam` |
| morphtool: print the creatures' morph files, animation sets, hair groups and extra data | done | `apps/morphtool/morphtool.cpp`, `components/morph` |
| morphtool: writing morph files back | todo | read only |
| creaturemindtool: dump creature mind files, with their learning episodes, and check each writes back byte for byte | done | `apps/creaturemindtool/creaturemindtool.cpp`, `components/creaturemind` |
| lhvmtool: read a compiled script program: its scripts, autostarts, global stack and data | done | `apps/lhvmtool/lhvmtool.cpp` |
| lhvmtool: decompile it into source, naming constants from the script headers, on the recorded source lines, with how well it went | done | `components/lhvmdecompiler` |
| lhvmtool: compile source back into a program | done | `apps/lhvmtool/lhvmtool_compile.cpp`, `components/lhvmcompiler` |

## Files without a tool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gesture templates | todo | read by `components/gestures`, no tool |
| Particle effect files | todo | read by `components/psys`, no tool |
| Raw images (the game's textures outside packs) | todo | read by `components/rawimage`, no tool |
| The game's info tables | todo | read by `src/InfoConstants.cpp`, no tool |
| The game's fonts | todo | read by `src/Gui/GameFont.cpp`, no tool |
| Saved games and profiles | todo | not read at all yet |
| Sound banks outside packs, and the dance and footpath files | todo | (unconfirmed which of these need their own tool) |
