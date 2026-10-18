/*******************************************************************************
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

namespace openblack::audio
{

/// The time of a sound that the game started: when it started, in real milliseconds, and how long its sample is. When
/// the machine loses its sound device, the game continues as if the device is still there: the sound lasts the length
/// of its sample from its start, with no sound.
struct SilentPlayback
{
	uint32_t startMs {0};
	float lengthMs {0.0f};

	/// The sample has not come to its end at this time. The clock is compared as unsigned, so that it may wrap.
	[[nodiscard]] constexpr bool IsPlaying(uint32_t nowMs) const { return static_cast<float>(nowMs - startMs) < lengthMs; }
	/// How far through its sample the sound is at this time, from 0 to 1
	[[nodiscard]] constexpr float Progress(uint32_t nowMs) const
	{
		if (lengthMs <= 0.0f)
		{
			return 1.0f;
		}
		return std::clamp(static_cast<float>(nowMs - startMs) / lengthMs, 0.0f, 1.0f);
	}
};

/// Whether a sound plays: as the device tells while the device works, or by its sample's length once the device is lost
[[nodiscard]] constexpr bool PlaysOn(bool sounding, bool deviceLost, const SilentPlayback& playback, uint32_t nowMs)
{
	return deviceLost ? playback.IsPlaying(nowMs) : sounding;
}

} // namespace openblack::audio
