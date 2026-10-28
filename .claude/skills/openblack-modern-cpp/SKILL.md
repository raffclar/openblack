---
name: openblack-modern-cpp
description: Use every time you write or edit C++ (or bgfx shader) code in openblack, including ports of vanilla Black & White behaviour and changes pulled from other forks. Enforces modern C++20, modern game-engine and bgfx shading patterns, and EnTT ECS/service-locator/resource-cache structure. Not for bw1-decomp, whose matching decompilation must stay MSVC 6-era C++.
---

# openblack: modern C++20, bgfx and EnTT

Apply this to every piece of C++ and shader code you write in openblack. Vanilla behaviour and visuals are the
target. The original game's code structure is not. Verify what the game does, then express it the way a modern
engine would.

## Before writing

1. Decide where each piece belongs (see **EnTT** below): component, archetype, system, pure module, resource or
   shader.
2. If porting from the exe or another fork, separate the *behaviour* (formulas, thresholds, order of operations,
   precision) from the *original's design* (globals, linked lists, object offsets, singletons, CPU-side per-cell
   loops). Keep the first exactly. Replace the second.

## Modern C++

The code base is C++20. Write modern, idiomatic C++:

- Follow modern game engine patterns and shader techniques. **Do not** directly lift design patterns from the original
  game.
- Implement visual shading code using modern game development patterns.
- Ensure vanilla visuals from the original game are preserved but with modern BGFX, C++ optimisations.
- Own resources with RAII: `std::unique_ptr`/`std::shared_ptr` and value types, never raw `new`/`delete`.
- Use `std::optional` for values that may be absent, `std::span` for views over contiguous data, `std::array` over C
  arrays, `enum class` over plain enums.
- Prefer `<algorithm>` and `std::ranges` over hand-written loops where they read better.
- Use `constexpr` for constants (named `k_PascalCase`), designated initializers for aggregates, and `[[nodiscard]]` on
  getters and pure functions.
- Keep pure logic (formulas, state machines) free of global state so it can be unit tested with fakes, never with the
  real game data.

## EnTT

Game state and services use [EnTT](https://github.com/skypjack/entt):

- **ECS.** Entities live in the registry (`ecs::Registry`, `Locator::entitiesRegistry`). Their data goes in components:
  plain structs in `src/ECS/Components`. Entities are made by archetypes in `src/ECS/Archetypes`. Behaviour goes in
  systems: an interface in `src/ECS/Systems/<Name>SystemInterface.h` and an implementation in
  `src/ECS/Systems/Implementations`, guarded by `LOCATOR_IMPLEMENTATIONS` so that only `Locator.cpp` (and tests that
  build their own locator) include it. Data belonging to an entity, such as a player's alignment, is a component on that
  entity rather than a field of a system.
- **Service locator.** Systems and services are reached through `Locator` (`entt::locator`, declared in `src/Locator.h`
  and emplaced in `src/Locator.cpp`), by their interface. Don't add singletons or globals.
- **Resources.** Assets are loaded once through the resource caches (`Locator::resources`, loaders in
  `src/Resources/Loaders.cpp`) and looked up by `entt::hashed_string` ids. Pass `.value()` of a hashed string to
  `Contains`. Any file loading or asset management should be done through the resource caches.

## Checklist before you finish an edit

- [ ] No new globals, statics holding game state, or singletons; shared state is behind a `<Name>SystemInterface` in
      `Locator`, map-wide shared data in `RegistryContext`, per-entity data in components.
- [ ] New systems: interface header, implementation guarded by `LOCATOR_IMPLEMENTATIONS`, emplaced and reset in
      `Locator.cpp`.
- [ ] Pure formulas live in a small module (free functions or value types) with gtest coverage on fakes.
- [ ] Assets come from the resource caches; generated GPU data (render targets, generated textures) is owned by the
      renderer with RAII and released before `bgfx::shutdown`.
- [ ] GPU work stays on the GPU where it fits (render-to-texture passes, blend equations, vertex/fragment shaders)
      rather than per-frame CPU loops and uploads; integer-exact vanilla maths is reproduced in the shader.
- [ ] Modern C++: RAII, `std::optional`, `std::span`, `std::array`, `enum class`, ranges, `constexpr k_` constants,
      designated initializers, `[[nodiscard]]`.
- [ ] Comments are plain English describing behaviour: no addresses, Mac symbols, assembly or vanilla internal or
      decompiled names.
- [ ] Whole-file clang-format on touched files.
