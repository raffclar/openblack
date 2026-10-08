# Display and frame rate

The game's window and screen: resolution, full screen or windowed, the graphics device, picture settings, vertical sync
and recovering when the device is lost. The game turn rate is in [../engine/](../engine/); frame statistics and the
debug wireframe are in [../debug/](../debug/).

**Progress: 3/9 done, 4 partial — 56%**

## Window and device

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player picks the screen resolution | partial | `--width` and `--height` on the command line (1280 by 1024 by default, `src/main.cpp`); no in-game list of modes |
| Full screen or in a window | done | `--window-mode` windowed, fullscreen or borderless (`src/main.cpp`) |
| 16 or 32 bit colour | n/a | modern displays are always 32 bit |
| The picture is rebuilt at the new size when the window changes size | done | `Renderer::Reset` called from `src/Game.cpp` |
| The game behaves sensibly while minimised | todo | the game checks whether it is minimised (unconfirmed what it does then) |
| The player picks the graphics device | partial | `--backend-type` picks the backend; no choice of adapter |
| Losing the graphics device (switching away, a driver reset) is recovered from, textures loaded again | partial | left to bgfx; a known Vulkan device lost hang after loading the testbed from a land (Direct3D 12 is fine) |
| Anti-aliasing | n/a | the game has none; openblack's off-screen targets multisample where the format allows (`src/Graphics/FrameBuffer.cpp`) |

## Picture and frame rate

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gamma or brightness setting | todo | the game reads the screen's colour scales and shifts (unconfirmed that it offers a brightness option) |
| Taking a screenshot | partial | `--screenshot-frame` and `--screenshot-path` take one at a frame; no key in game (unconfirmed the game has one) |
| Vertical sync | done | `--vsync` (`src/main.cpp`, `EngineConfig`) |
