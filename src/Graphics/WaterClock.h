/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the clocks of the sea and the sky

#pragma once

#include <cstdint>

#include <chrono>

namespace openblack::graphics::underwater
{

/// A clock that counts whole milliseconds and never goes back.
///
/// The count is an integer, so that many small steps do not add errors. The clock does not wrap: a wrap makes all the
/// waves, the jellyfish and the clouds jump to a different time. The time in seconds is a float for the shaders. A
/// float keeps one millisecond for about four hours and 8 milliseconds after one day.
class WaterClock
{
public:
	/// Moves the clock on by the step. A step of no time (a paused game) keeps the clock where it is. The clock ignores
	/// a step that goes back.
	constexpr void Advance(std::chrono::milliseconds step)
	{
		if (step.count() > 0)
		{
			_milliseconds += static_cast<uint64_t>(step.count());
		}
	}
	[[nodiscard]] constexpr uint64_t GetMilliseconds() const { return _milliseconds; }
	/// The time in seconds. A larger count never gives a smaller time.
	[[nodiscard]] constexpr float GetSeconds() const { return static_cast<float>(static_cast<double>(_milliseconds) / 1000.0); }

private:
	uint64_t _milliseconds {0};
};

} // namespace openblack::graphics::underwater
