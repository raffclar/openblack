# Refactor progress

How far the code base has moved towards the conventions in [README.md](README.md). The counts come from text searches
over `src` (excluding the tests where noted), so treat them as close, not exact; the percentages are derived from the
two counts on each row.

Measured at the commit this branch was published from. "Before" is the commit the refactor started from.

| What | Before | Now | Done | What is left |
|---|---:|---:|---:|---|
| Mutable globals and file- or function-scope statics in game code | 185 | 20 | ~89% | Mostly log-once warning flags inside functions, the `Game` instance pointer, and three file-scope objects; a handful stay on purpose (see below) |
| `Game::Instance()` call sites | 89 | 2 | ~98% | Two in the debug UI: loading a map and reading the turn delta. Both need `Game` split further |
| Locator services | 29 | 81 | — | New services replaced the globals; the number grows as state moves |
| Plain `enum` declarations | 14 files | 0 | 100% | Bit-flag enums became `constexpr` constants with the same type and values |
| `new` / `delete` in game code | 43 | 7 | ~84% | Required by a C API or a private constructor |
| C arrays | 44 | 8 | ~82% | Required by a C API (`argv`, OS buffers, a union member) |
| Direct file reads (`std::ifstream`, `fopen`) | 20 | 9 | ~55% | All of them out of scope: the file system itself, the crash handler, a user save, streaming video and a bank fallback |
| Comments naming the original binary (`fn_…`) | 5354 | 0 | 100% | Across `src`, `components` and the shaders. The detail lives in the wiki, `docs/bw1-notes` |
| Comments quoting the original's addresses | — | 0 | 100% | The 19 hexadecimal values left in comments are data: colours, masks, flags, file-format offsets |
| Identifiers named after the original binary or unclear | 330 to rename | 41 of 49 batches done | ~84% | Seven batches left, listed in order; 161 names that come from upstream openblack are left alone on purpose |
| Files that fail the CI's clang-format 17 check | ~700 | 0 | 100% | One formatting commit, then every later change formatted on the files it touched |
| Test files | 151 | 165 | — | Grouped into 13 executables; 46 tests gained a synthetic twin. 1487 tests pass; 53 skip when the game data they need is not found |

## What stays on purpose

Each of these carries a comment saying why:

- environment variables read once into a `static const` local, and a `std::once_flag` for a log-once warning;
- the physics bodies, an ordered list owned by `PhysicsObjectsSystem`: components would change the iteration order the
  simulation depends on;
- the few `new` expressions and C arrays a C API or a private constructor requires.

## Efficiency work parked

From the efficiency audit of the temple interior and the debug windows (2026-10-08). Each would keep the output
identical, but carries a risk or touches shared code for a small gain:

- the temple's light glows: one transient mesh and one draw call per sprite; batching them by texture and blend state
  changes what is dropped if the frame's transient buffer runs out;
- `L3DMeshSubmitDesc::instanceDesc` owns its `InstanceDesc`, so every temple draw allocates one; a non-owning pointer
  is an API change in shared code;
- the temple map's mesh is rebuilt every frame: its cell callback could be a template instead of a `std::function`
  (safe), and rebuilding it only when the land changes needs a land change counter (parked);
- the scroll read in front is written and laid out every frame; caching it needs a key with every fact it shows;
- the uniforms that stay the same for a submesh's primitives are set for each primitive; setting them once relies on
  bgfx keeping uniform values between submits;
- the debug windows, only while open: the Consciences message-set table draws all its rows without a list clipper,
  and the outliner formats every entity's label every 30 frames.

## Known gaps

- `Game` is not fully split: the two remaining `Game::Instance()` call sites, and the map load path, still reach into it.
- Seven rename batches are left; until then some names still mirror the original's rather than describing what they do.
- The shader build does not track `#include`d shader files, so a variant that includes a changed shader is only rebuilt
  from scratch; verification deletes the generated shaders first.
- Land 2 to Land 5 and multiplayer have no automatic behaviour check yet; only Land 1 and a Land 1 → Land 2 → Land 1
  cycle are compared (see [TESTING.md](TESTING.md)). Since the player's creature is loaded into Land 1, the Land 1
  runs cover it too: its own part of the state hash, its mind's random draws and the hand resting on it.
- The player's creature does not follow its player to the next land yet: ours belongs to the land's registry.
