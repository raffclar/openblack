# Changelog

What this fork of openblack changes. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Every change was checked against the build before it, with a fixed time step:

- the full test suite;
- a per-turn hash of the world state and the full random number trace, over Land 1 and a Land 1 → Land 2 → Land 1
  cycle;
- pixel comparisons of the land view by day and by night, and of the temple;
- scripted hand demos.

A change that alters the game on purpose is its own commit, measured and described, and the references are remade
after it. How to run the checks is in [docs/refactor/TESTING.md](docs/refactor/TESTING.md).

## [Unreleased] — raffclar's architecture, the temple and the creature

The fork now follows the architecture of raffclar's openblack tree: his layout, names, Locator slots and
`<X>SystemInterface` / `Implementations/<X>System` pairs, and his test layout. Where both trees did the same job, his
version was taken only when it was better or behaved the same bit for bit; otherwise ours stays, behind his names. The
patterns are written down in [docs/refactor/README.md](docs/refactor/README.md).

### Added

- **The temple interior** from raffclar's tree: its rooms, scrolls and signs, with the world paused while you are
  inside. The door is entered with the hand, and the temple's outside follows the player's alignment.
- **The player's creature in Land 1.** It is loaded from the player's profile, and runs on raffclar's creature systems:
  body, mind, movement, fights, hair, skin and sounds. The hand can hold it, the land scripts' creature commands work,
  it lives at its temple's pen, it is drawn smaller in the pen as in the original, and it moves smoothly between turns.
- **The storage pit as in the original.** The hand gives what it holds to the store under it. Wood joins the pit's
  total, fences count as wood, animals are taken as food, and the worship site keeps what it is given.
- **The editor (F2) and creature mode** from raffclar's tree.
- **Debug windows:** the creature spawner, camera paths, weather, gestures, key bindings, the magic tabs and a
  vortices window.
- **raffclar's progress notes** in [docs/progress](docs/progress/README.md): every feature of the original game, row
  by row, with its status, plus the original's own bugs. His pull request and issue templates are in `.github`.

### Changed

- Names follow raffclar's tree, for example `map_coords`, `gutils`, `zsort`, `Alignment`, and the `time`,
  `particleSystem` and `villageLightSystem` services.
- The tests are in the top-level `test/` folder, in his folders and with his test macros.
- Dialogs, the text database and the creature's cave screen use raffclar's code.
- The land light palette is a resource, and the sky and render frame services are behavioural.
- Making a texture or a mesh mid-game no longer stalls the frame.

### Fixed

- The camera no longer clips things up to twice its near distance, so the consciences are no longer cut off close to
  the camera.
- The "Did you know?" signs open their bubble when tapped.
- The temple door can be entered, the temple window no longer crashes, and the miracle dispensers make their miracles.
- The creature's hair, eyes and whiskers follow its drawn size, and the temple no longer covers its hair.
- The worship site wears its temple's texture.

## Code base refactor

The code follows the project's conventions, summarised in [docs/refactor/README.md](docs/refactor/README.md). The
simulation and the visuals are unchanged, except where noted under Removed.

### Changed

- **No mutable globals for game logic.** About 400 globals and statics moved into Locator services or into components
  on the entity they belong to. Tests inject fakes through the services instead of using test-only setters.
- **Assets load once**, through the resource caches, instead of being read from disk in about 35 places.
- **Modern C++:** `std::array`, `std::unique_ptr`, `enum class`, `constexpr` constants and `[[nodiscard]]`.
- **Comments** are plain English that describe behaviour. Names and addresses from the original game's binary moved to
  the research wiki, so no research was lost, and identifiers were renamed after what they do.
- **Formatting:** the whole code base passes the CI's clang-format 17 check.
- **Faster builds:** precompiled headers and 13 grouped test executables. A clean build takes about 7 minutes instead
  of 31.
- **Our own audio decoders** replace dr_wav and dr_mp3, with the same samples on every sound and music segment of the
  game.
- `glwtool`'s JSON keys use the field names from raffclar's tree. JSON written with the old keys must be renamed.

### Added

- **openblack's own Bink video decoder.** The game's films play, frame-identical to the original's Bink library,
  without FFmpeg. Its tables and coefficient order follow FFmpeg's Bink decoder (Konstantin Shishkov, Peter Ross;
  LGPL-2.1-or-later, used under GPL-3).
- **Debug windows** for audio (play any sound or music, silence the game's sounds or music) and video (play any film,
  turn the films off).
- **The script VM's debugger** from raffclar's tree: breakpoints, stepping, and editing variables.
- **The research wiki, curated:** [docs/bw1-notes](docs/bw1-notes/README.md), with a Pending section on every page,
  and an HTML copy with search in `docs/wiki-html`.
- **The refactor's documentation** in `docs/refactor/`.
- `OPENBLACK_IGNORE_REAL_INPUT=1`, for test runs: the real mouse is ignored.

### Fixed

- MS-ADPCM sounds end at their real length instead of playing the padding of their last block.
- The hand's grip flag starts as false instead of uninitialised.
- A broken house's scaffold was drawn as a huge dome. The same bug is in upstream openblack.

### Removed

- **The mod loader and the built-in mods**, with their Lua, sol2 and JSON dependencies and the `--mod`, `--msaa`,
  `--mipmaps`, `--anisotropic` and `--enhanced-graphics` options. A minimal, versioned `ModLoader.dll` remains; the mod
  SDK's design is in [docs/refactor/SDK_FOUNDATION.md](docs/refactor/SDK_FOUNDATION.md).
- Because the intro-skipping mod is gone, the game plays the intro and asks the tutorial question, as the original
  does.
- FFmpeg and drlibs.

## Known issues

- Tree foliage sway can differ between runs while assets load, so the start menu screenshot is not pixel-stable. The
  land view is.
- The first night screenshot after a fresh build can differ from later ones.
- The shader build does not track `#include`d shader files: after changing `vs_object.sc`, delete the generated shaders
  to rebuild the instanced ones.
- The player's creature does not follow its player to the next land yet.
