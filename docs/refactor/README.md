# Refactor guide

How the code base is organised after the refactor, and how to add code that fits the project's conventions.

See [PROGRESS.md](PROGRESS.md) for how much of the code base already follows these rules and what is left.

## Goals

- Reproduce Black & White's behaviour and visuals faithfully, with modern C++20 and a modern BGFX renderer. The design
  patterns of the original game are not copied.
- Keep game state out of globals and singletons, so that logic can be unit tested with fakes.
- Load every asset once, through the resource caches.

## Where state lives

| Kind of state | Where it goes | Example |
|---|---|---|
| Data that belongs to one entity | A component, i.e. a plain struct in `src/ECS/Components` | a villager's fire state, a town centre's belief symbols |
| World or game state shared by many systems | A Locator service | the map cells, the spell list, the weather |
| Anything loaded from a file | A resource cache entry, through a loader in `src/Resources/Loaders.cpp` | fonts, meshes, gesture templates, `info.dat` |
| Pure logic | A free function with no state | distance formulas, the spell grid's decay |
| Debug and test-hook state | `Locator::debugHooks` | trace throttles, scripted shot lists |
| What only changes how a thing is drawn | Its draw-pose component, never its `Transform` (the state hash covers `Transform`) | `CreatureDrawPose::scale`, a creature drawn smaller in its temple's pen |
| A value the original reads from the registry or the player's profile | `EngineConfig` through `Locator::config`, set by a command-line option with a fixed default, so every run is the same | `--creature-file`, the profile's creature |

- **A system's member that is really one entity's data** moves into a component on that entity, made when the state is
  first written and read through the const registry, so lands without such an entity get no new storage (the fight's
  held press `CreatureFightPress` on the fighter; the leash's last refusal `PlayerLeashRefusal` on the player's
  entity). The local player comes from `PlayerSystemInterface::LocalPlayer()`, not from the audio queries.

### Services

A service has:

1. an interface in `src/ECS/Systems/<Name>Interface.h`, with `[[nodiscard]]` on the methods that return a value;
2. an implementation in `src/ECS/Systems/Implementations/<Name>.{h,cpp}`, included only under
   `LOCATOR_IMPLEMENTATIONS` (that is, by `Locator.cpp` and by tests that build their own locator);
3. a `using` line in `src/Locator.h`, and its emplace and reset in `src/Locator.cpp`.

Lifetime rules:

- **Order.** Services are emplaced in the order of today's initialisation and reset in the order of today's teardown.
  A service that entities use while they are destroyed is reset after the entity registry.
- **Keep what tests set up.** Services that replace a former global keep a fake that a test set up before the game was
  created (`if (!has_value())`).
- **Fail clearly.** Release builds have no `ENTT_ASSERT`, so code that reaches a missing service fails with a clear
  message instead of a null dereference.
- **Free functions.** Many existing free functions now forward to their service, so call sites did not have to change.
  New code may call the service directly.

### Events

One-way notifications that used to be setter hooks are plain structs in `src/ECS/Events`, published through
`Locator::events`, for example a villager's death, help text or decision steps. The event manager calls handlers at
once, so the order inside a turn is unchanged. A handler must not publish an event of the same type it is handling.

### Resources

- Use `Locator::resources` and an `entt::hashed_string` id, and pass `.value()` to `Contains`.
- Files whose owners parse them themselves go to the **byte cache**. The owner parses from memory, through a span stream
  where needed.
- Do not read files with `std::ifstream` or `fopen` in game code. The only exceptions are user files (settings, saves,
  screenshots), crash logs and streaming audio or video.

## Coding conventions (summary)

- **Comments:** plain English that describes behaviour. Never mention decompiled function names, Mac symbols, exe
  addresses or assembly; that detail belongs in `docs/bw1-notes`.
- **Modern C++:**
  - `std::unique_ptr`, `std::shared_ptr` and value types; never raw `new`/`delete`;
  - `std::optional`, `std::span`, `std::array` and `enum class`;
  - `constexpr` constants named `k_PascalCase`;
  - designated initialisers.
- **Naming:** inclusive language (main/replica, allowlist/denylist).
- **Drawn poses:** what moves once a turn is drawn between turns from a draw-only component (`DrawPosition`,
  `PhysicsDrawPose`, `CreatureDrawPose`); the logic and the state hash read only the Transform. An object's attached
  parts (eyes, hair, footprints, what it holds, the hand's touch) take the one matrix its body is drawn with
  (`ecs::DrawnModel` / `DrawnBodyModel`, which decides the drawn pose per row with const lookups and makes no storage),
  never a placement of their own from the Transform; what is sized apart from that matrix is scaled by the share the
  body is drawn at (`creature_pose::DrawnSizeShare`).

## Following raffclar's tree

raffclar's stack (`raffclar/openblack`, `stack/81-creature-mode`) is the architecture base, and his code is brought in
step by step (the merge plan). Where both trees have the same thing, ours takes his layout and names:

- **Names follow his code, comments stay plain English.** Identifiers take his names, including the ones he kept from
  the original game (`map_coords::JustMapXZ`, `gutils::LHArcTan`, `gutils::ConvertGameAngleToScawenAngle`). A name
  that doesn't say what it does gets a one-line comment that does. The comment rule above is unchanged.
- **Where shared code lives:**
  - map coordinates in `src/3D/MapCoords.h` (`openblack::map_coords`);
  - angle and distance utilities in `src/Common/GUtilsAngle.h` and `GUtilsDistance.h` (`openblack::gutils`);
  - the transparency sort in `src/Graphics/ZSort.h` (`graphics::zsort`);
  - the player's alignment in `components::Alignment`;
  - the influence components in `src/ECS/Components/Influence.h`.
- **A Locator slot takes his name together with his interface,** never alone. A slot renamed while it still holds our
  interface would let code ported from his tree compile against the wrong type.
- **Debug windows** are `debug::gui::Window` classes listed under Debug > Windows, one file pair each, with their logic
  in a `<Window>Model.{h,cpp}` of pure functions that a test covers. A window that acts on the land overrides
  `TakesEvent`, so the game doesn't also get the click. A switch that must hold with the window closed is kept in
  `UpdateAlways`, and gives back what it changed when it is turned off. Closed with every switch off, a window reads and
  writes nothing.
- **The conformance pass** brings each area to his shape after the renames: his interface in front of our code, state
  holders turned into behaviour behind their interface, per-entity data into components. Every step keeps the output
  identical. A step that cannot (an unordered walk that no ordered structure reproduces, entities where we have none)
  is parked, and the user decides whether to take his behaviour.
- **Script natives with logic:** `CHLApi.cpp` only pops, turns ids into entities and pushes; the body is a free
  function over the injected service interfaces and the registry (`ECS/PlayerCreature`), tested with a recording fake.
- **Renames are scripted** (`git mv` plus a list of replacements), so that they can be re-run on a new tip instead of
  rebased.
- **Audio service:** his `Locator::audio` slot and `AudioManagerInterface` (with a virtual destructor) front
  our engine. `AudioManager` is the engine, `AudioManagerNoOp` only a base for test fakes, never a device
  fallback. The free functions of `Audio/Audio.h` are thin wrappers over the slot (`Audio/AudioApi.cpp`); a test
  injects a fake through the slot with `RestoreService`. The slot is reset after the registry.
- **Gui, the files taken from his tree:** `Dialog`, `TextDatabase` and `CreatureCaveScreen` are his text. `Canvas`,
  `DialogPainter` and `Controls` are his text with our deltas put back, each needed for the same pixels:
  - `Canvas`: his view and `Blend`/`SetBlend`, plus our coverage batches and the `InterfaceText` program that draws
    `graphics::GameFont`'s one-channel atlas;
  - `DialogPainter`: his `DrawPointer(Canvas&)`, plus `enum class Edges` with `HasEdge`, `MidTextSize()` and
    `SmallTextSize()` (one bigger when the help system asks for bigger text), and `DrawString` through
    `graphics::GameFont`;
  - `Controls`: the message box's fit rule, the label and edit box sizes through the text size functions,
    `IsCaretShown` as a tested pure function, `std::accumulate` for a list's height, and our `Zoomer` calls;
  - `CreatureCaveScreen`: with no creature the cave's window is not drawn at all, as in the original's empty cave.
- **Gui, the files that stay ours** (his versions would change what is drawn or when):
  - `GameFont` adapts the exact port in `graphics::GameFont`, read through the font cache. His parses the font files
    himself, with other glyph metrics and line breaks, so every line of text would move.
  - `ToolTips` is a handle over the help system's tooltips (`Help/ToolTips`). His is a separate state machine whose
    timing and priority differ, which would change the tooltips and the hand demos.
  - `GameInterface` keeps the SkipBox, which the deterministic runs answer, and loads its textures through the
    resource cache. His newer panels (tooltip glow and arrows, creature status and fight panels, cinema bars) are to
    come later as behaviour changes; his reads files directly, outside the loaders.
  - `GameMenu` keeps the escape menu's states and the pure functions behind them (`EscapeState`, `EscapeOverQuestion`,
    `PageAfterAnswer`, the video change prompt, the help speed slider), all covered by tests. His later edits are to be
    compared against it one by one.
- **Land drawing:**
  - `LandNormal` is his text.
  - `BlockTexture` takes his names (`k_OpenSeaFlag`, `k_MapTexels`, `coneWeights`) and keeps our code: the rebuild of
    a box of cells for the land morph, the altitude bits, the bounds-checked `Materials` and the coast alpha in
    `CoastAlpha`.
  - `LandLightTable` takes his shape: the palette is a `LandLightPalette` resource (`weather/palette`, loaded through
    `LandLightPaletteLoader` into `GetLandLightPalettes()`), `Build` takes it, and the accessors take his names
    (`GetLandColour`, `GetWarmColour`, `GetMoonColour`). Our maths stay, because our sky type runs the original's way
    (0 by day, 2 at night) and his the other: the time column `(2 - T) * 15`, the overcast cap in floats, the storm haze
    and the lightning's whole steps of 256. Our colours stay 0xAARRGGBB, with `ToColour` for 0 to 1.
  - There is no global table. The renderer hands each table it builds to `renderFrameSystem`
    (`SetLandLightTable`), and the game reads the last one through `land_light::CurrentTable()`, at the same moment
    in the frame as before.
  - `renderFrameSystem` and `skyFrameSystem` are behavioural services, with no `GetState()`. Their state is private to
    the implementation. The `land_light::`, `model_light::`, `mists::` and `sky_type::` free functions stay as thin
    wrappers over them. `SkyFrameSystemInterface::GetCurrentSkyType` keeps our scale.

## Exceptions that remain

Some state stays where it is, each with a comment that says why:

- a lazily read environment variable that never changes, kept as a `static const` local;
- a `std::once_flag` for a warning that is logged once;
- the physics bodies, which stay an ordered list owned by `PhysicsObjectsSystem`, because components would change the
  iteration order the simulation depends on;
- a few C arrays and `new` expressions required by a C API or a private constructor.

## Still to do

Counts are in [PROGRESS.md](PROGRESS.md).

- Remove the `Game::Instance()` singleton. The map script globals, the day/night clock and the screen fade are already
  in progress.
- Move the exe addresses that production tables keep as data into the docs. Function names now follow raffclar's
  tree (see "Following raffclar's tree").
- The Locator slots with a counterpart in raffclar's tree now take his names (`time`, `particleSystem`,
  `villageLightSystem`, with their interfaces and implementations). Their shapes follow his in the conformance pass.
- Build a mod SDK once the game is fully rebuilt (see [SDK_FOUNDATION.md](SDK_FOUNDATION.md)).
