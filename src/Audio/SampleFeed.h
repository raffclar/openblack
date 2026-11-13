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

namespace openblack::audio
{

/// The part of a sample that a voice loops over: from its first frame up to, but not including, its end frame
struct LoopRegion
{
	int32_t first {0};
	int32_t end {0};

	[[nodiscard]] constexpr bool operator==(const LoopRegion&) const = default;
};

/// The loop region of a sample from the first and last frames in its bank header, for a sample of this many frames.
/// The header marks no region with -1, or with a first frame that is not before the last frame. The region stops at the
/// end of the sample.
[[nodiscard]] constexpr std::optional<LoopRegion> LoopRegionOf(int32_t firstFrame, int32_t lastFrame, int32_t frames)
{
	if (firstFrame < 0 || lastFrame <= firstFrame || firstFrame >= frames)
	{
		return std::nullopt;
	}
	return LoopRegion {.first = firstFrame, .end = std::min(lastFrame + 1, frames)};
}

/// How a voice gives its sample to the mixer
struct SampleFeed
{
	/// The voice holds the sample as its one buffer. Only then does the mixer use the loop region of the sample.
	bool attached {false};
	/// The number of copies of the sample in the queue of the voice, when the voice does not hold it as one buffer
	int32_t queued {0};

	[[nodiscard]] constexpr bool operator==(const SampleFeed&) const = default;
};

/// How a voice gives its sample to the mixer, from its loop count. The sound library plays a sample loopCount + 1
/// times, and a loop count below 0 loops the sample until the game stops the voice or releases the loop.
///
/// A voice that loops until it stops holds the sample as one buffer. The mixer then plays the sample from its start,
/// loops over the loop region of the sample, and continues to the end when the game releases the loop. A queue of
/// buffers loops back to the start of the sample each time, and the mixer does not use the loop region.
[[nodiscard]] constexpr SampleFeed SampleFeedOf(int32_t loopCount)
{
	if (loopCount < 0)
	{
		return {.attached = true, .queued = 0};
	}
	return {.attached = false, .queued = loopCount + 1};
}

} // namespace openblack::audio
