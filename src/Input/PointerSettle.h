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

#include <algorithm>
#include <optional>

namespace openblack::input::pointer_settle
{

/// A scripted press must come this many frames after the pointer was last moved. The game reads the pointer one frame
/// after it is moved, and what the pointer is over is known one frame after that. A press made sooner acts on what the
/// pointer was over before, as a hand too fast for the screen would.
constexpr uint64_t k_FramesBeforePress = 2;

/// The first frame a press asked for at a frame can be made at, given the frame the pointer was last moved at
[[nodiscard]] constexpr uint64_t EarliestPressFrame(uint64_t wanted, std::optional<uint64_t> pointerMovedAt) noexcept
{
	return pointerMovedAt.has_value() ? std::max(wanted, *pointerMovedAt + k_FramesBeforePress) : wanted;
}

/// Whether a press may be made now, given the frames since the pointer last moved
[[nodiscard]] constexpr bool PressMayStart(uint64_t framesSincePointerMoved) noexcept
{
	return framesSincePointerMoved >= k_FramesBeforePress;
}

} // namespace openblack::input::pointer_settle
