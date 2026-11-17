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

namespace openblack
{

/// Counts the frames of a box that the game shows before the first land, such as the new player's name box. A run
/// with a number of frames (-n) stops in the box after that number of frames. A tool run thus ends also when nobody
/// answers the box.
class StartupBoxFrames
{
public:
	/// A frame limit of 0 is no limit, as for -n 0
	explicit constexpr StartupBoxFrames(uint32_t frameLimit) noexcept
	    : _frameLimit(frameLimit)
	{
	}

	/// Counts one more frame of the box. Returns true when this frame is past the limit and the run must stop.
	[[nodiscard]] constexpr bool Step() noexcept
	{
		if (_frameLimit == 0)
		{
			return false;
		}
		++_frames;
		return _frames > _frameLimit;
	}

	[[nodiscard]] constexpr uint32_t FrameLimit() const noexcept { return _frameLimit; }

private:
	uint32_t _frameLimit;
	uint32_t _frames {0};
};

} // namespace openblack
