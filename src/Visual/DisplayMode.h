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

namespace openblack::windowing
{

/// How the game window shows on the screen
enum class DisplayMode : std::uint8_t
{
	Windowed,
	Fullscreen,
	Borderless
};

} // namespace openblack::windowing
