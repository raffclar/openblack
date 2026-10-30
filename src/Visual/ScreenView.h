/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace openblack::graphics
{

/// A frame as the renderer reads it back: BGRA8 pixels, rows pitch bytes apart, the bottom row first when yflip. The
/// pixels stay valid only while the function that gets the view runs.
struct ScreenView
{
	uint32_t width {0};
	uint32_t height {0};
	uint32_t pitch {0};
	bool yflip {false};
	std::span<const uint8_t> bgra;
};

/// Makes a picture from a frame. The renderer calls it on its own thread, so it must not use the game's data.
using ScreenPicturer = std::function<std::vector<uint8_t>(const ScreenView& screen)>;

/// The renderer keeps a picture in memory when it reads the frame for a name that starts with this
inline constexpr std::string_view k_ScreenPicturePrefix = "memory:";

} // namespace openblack::graphics
