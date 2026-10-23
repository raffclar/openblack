/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <string_view>

/// How the game window works with the display scale of Windows. The game tells Windows that it does its own scaling
/// for each monitor. Thus Windows does not stretch the picture, and the picture stays sharp above 100% scale. The size
/// of the window, the back buffer and the pointer all use the real pixels of the monitor.
namespace openblack::windowing
{

/// A setting that SDL reads before it starts its video part
struct SdlHint
{
	std::string_view name;
	std::string_view value;
};

/// The settings for Windows. The game applies them before SDL starts its video part, because SDL reads them only then.
/// Other systems ignore them.
inline constexpr std::array<SdlHint, 2> k_WindowsDisplayScalingHints {{
    // Windows does not stretch the window on a monitor of any scale. When the window goes to a monitor of another
    // scale, the picture keeps its size in pixels and only the frame of the window changes
    {.name = "SDL_WINDOWS_DPI_AWARENESS", .value = "permonitorv2"},
    // One unit of SDL is one pixel of the monitor, for the size of the window and for the pointer
    {.name = "SDL_WINDOWS_DPI_SCALING", .value = "0"},
}};

} // namespace openblack::windowing
