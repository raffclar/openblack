# Mod SDK: the foundation

openblack had a mod loader: Lua and native mod hosts, a mod API, asset replacements, engine switches, mod settings and
a Mods menu, plus a set of mods that shipped with the game. It was a proof of concept, and it has been **removed** from
the code base: an unfinished mod system spreads hooks, global state and compatibility code through an engine that is
still being written.

What stays is this document, which says how to build a mod system well, and a **minimal mod loader**: the seam a
real SDK will plug into. The real SDK is written once the game itself is reconstructed.

## The minimal mod loader

`ModLoader.dll` is a shared library of its own, built from `src/ModLoader/`, with no scripting dependency and nothing
from the engine. The game is not linked against it.

```cpp
constexpr int k_ModLoaderVersion = 1;
using ModLoaderLogFn = void (*)(const char* line, void* context);
extern "C" int ModLoader_Version() noexcept;
extern "C" int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept;
```

`ModLoader_LoadMods` reads `<modsFolder>/version.txt`, which holds one integer. It calls `log` exactly once and returns
0 when the version matches (nothing is loaded yet), 1 when the file is missing, 2 when it cannot be read and 3 when its
version is not the loader's; in every case but 0, no mod is read.

The game has one call site. At start-up it looks for `ModLoader.dll` next to the executable; without it, it logs one
line and carries on exactly as before. With it, it checks the library's version, asks it to read the `Mods` folder,
logs the result and unloads the library. Only then does the debug menu show a **Mods** entry, with the loader's version
and that result. There are no other hooks, switches or settings in the engine.

## Rules for the SDK

### A versioned handshake, before anything else

The library and the exe agree on a version before a single mod is read. The C API across the seam is versioned per
interface, so that an old library and a new exe fail cleanly with a message instead of crashing.

### One service, by interface

Everything the mod system owns — the mod list, load order, dependencies, the published interfaces, the log, the restart
request — lives in one Locator service (a `ModsInterface` and its implementation), as members. No file-scope statics, no singletons. The
engine and the debug UI reach mods only through the interface.

### Hosts behind an interface

A `HostInterface` with `Start`, `Stop` and `Running`, and one implementation per language, each holding its state as
members. The service creates the hosts in a fixed order, and a test with a mock host checks that order, because the
order in which hosts see an event is part of the game's behaviour.

### Game events, not calls inside the loops

The engine publishes `TurnEnded`, `FrameEnded` and `LandLoaded` as plain event structs through the Locator's event
manager, and the hosts subscribe. The game loop never names a mod host. The event manager calls handlers synchronously,
so the order of work inside a turn does not change, and it is not reentrant: a handler must not publish the event it is
handling. A debug assert enforces that.

### Mod assets through the resource loaders

An asset replacement is a lookup that the resource loaders ask, not a parallel table consulted from the engine's load
code. A replaced file is then loaded by the same loader as the original. With no mods installed there are no
replacements, so the order and number of file reads must be unchanged — compare the load log before and after.

### Settings as value types

A mod's state is a value type with pure `Parse` and `Format` functions, unit tested. The engine switches are a value
table owned by the service. Nothing reads a settings file from inside game code.

### Test hooks are not part of the mod system

No test-only environment variable or hook belongs in the mod code. Test hooks live on the debug side, in the debug hooks
service, and drive a mod through the service's interface exactly as the UI does.

### Built-in mods are mods

Anything that ships with the game, such as graphics tweaks or extra foliage, is a built-in mod that reads its options
through the interface and gives its overrides to the resource loaders. It does not keep state inside the engine's
subsystems.

### A small API that only exposes interfaces

The mod API exposes Locator interfaces (camera, terrain, clock, magic, audio) and nothing else, with one `constexpr`
version that the hosts check, instead of version numbers repeated in each language binding.

### Tests with mocks

The mod system's own tests build their own instance and never touch the game's locator. Tests that need a scripting
runtime are integration tests and skip themselves when it is absent.

## Verifying a mod system against the game

A mod system must not change the game when no mod is installed. Verify with an empty mod folder, as every other change
is verified (see [TESTING.md](TESTING.md)): the per-turn state hash and the random trace over Land 1 and a land cycle,
the land view by day and by night, the intro, the scripted hand demos and the tests in both modes.

## Later

- A stable C ABI per interface, with its own header package.
- The scripting API generated from the C++ interfaces instead of written by hand.
- The Mods window as an ECS/ImGui system a mod can extend.
- More events, as event types, when a mod needs them.
- A manifest schema that drops the legacy data, plus hot reload, signing and a repository.
