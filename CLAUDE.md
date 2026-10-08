# openblack

openblack reimplements Black & White (2001). Follow the conventions below for all C++ code and game design logic.

The goal is to recreate the original game as closely as possible, ensuring vanilla visuals from the original game
are preserved but with modern libraries such as BGFX, EnTT and with C++20 optimisations.

## Token Optimisation

- Always read with explicit offset and limit instead of slurping whole files.
- Avoid tailing logs directly. Tail ands filter for the relevant lines when checking for specific events.

## Comments

- Comments should stay plain English, describing behaviour without referencing Mac symbols, addresses, assembly, or
  vanilla's internal names.
- Avoid decompiled function names. They may change between versions and developer setups.
- Technical notes and research should go to `docs/openblack/*domain*/*feature*.md`

## Modern C++

The code base is C++20. Write modern, idiomatic C++:

- Follow modern game engine patterns and shader techniques. **Do not** directly lift design patterns from the original
  game.
- Implement visual shading code using modern game development patterns.
- Ensure vanilla visuals from the original game are preserved but with modern BGFX, C++ optimisations.
- The Graphics namespace is for renderer code and 3D is for assets code.
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

- Target FPS is 100. Anything below 100 FPS needs investigation. This is extremely important for the game to run
  smoothly.
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

Use these where they fit. A small value type or a pure function doesn't need to be a component or a system.

## Testing

Write unit tests for components and systems. Use mocks for dependencies. Don't test systems through the locator.

- Tests live in `test/`, registered in `test/CMakeLists.txt`.
