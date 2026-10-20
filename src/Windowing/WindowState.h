/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "WindowingInterface.h"

namespace openblack::windowing
{

/// Whether a window is minimised. SDL's own flag can't be trusted alone: a window the system starts minimised (a game
/// launched minimised) is flagged shown and not minimised until it is first restored or moved, while Vulkan already has
/// nothing of it to draw into. So the system's own answer, where the platform gives one, counts too.
[[nodiscard]] constexpr bool WindowMinimised(bool flaggedMinimised, std::optional<bool> systemMinimised) noexcept
{
	return flaggedMinimised || systemMinimised.value_or(false);
}

/// The display mode a window really opens in. A background window stays in a window, so that it never covers the desktop.
[[nodiscard]] constexpr DisplayMode BackgroundDisplayMode(DisplayMode requested, bool background) noexcept
{
	return background ? DisplayMode::Windowed : requested;
}

} // namespace openblack::windowing
