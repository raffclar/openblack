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
#include <vector>

namespace openblack::audio
{

/// The part of a sample that a voice loops over: from its first frame up to, but not including, its end frame
struct LoopRegion
{
	int32_t first {0};
	int32_t end {0};

	[[nodiscard]] constexpr bool operator==(const LoopRegion&) const = default;
};

/// The loop region of a sample from the first and end frames in its bank header, for a sample of this many frames.
/// The header marks no region with -1, or with a first frame that is not before the end frame. The mixer does not
/// play the end frame in the region, and the region stops at the end of the sample.
[[nodiscard]] constexpr std::optional<LoopRegion> LoopRegionOf(int32_t firstFrame, int32_t endFrame, int32_t frames)
{
	if (firstFrame < 0 || endFrame <= firstFrame || firstFrame >= frames)
	{
		return std::nullopt;
	}
	return LoopRegion {.first = firstFrame, .end = std::min(endFrame, frames)};
}

/// A part of a sample that a voice can queue
enum class SamplePart : uint8_t
{
	/// All of the sample
	Whole,
	/// The frames before the loop region
	BeforeRegion,
	/// The loop region
	Region,
	/// The frames after the loop region, to the end of the sample
	AfterRegion,
};

/// The frames of a part of a sample of this many frames, from the first frame up to, but not including, the end frame
[[nodiscard]] constexpr LoopRegion FramesOf(SamplePart part, LoopRegion region, int32_t frames)
{
	switch (part)
	{
	case SamplePart::BeforeRegion:
		return {.first = 0, .end = region.first};
	case SamplePart::Region:
		return region;
	case SamplePart::AfterRegion:
		return {.first = region.end, .end = frames};
	case SamplePart::Whole:
		break;
	}
	return {.first = 0, .end = frames};
}

/// A part of a sample in the queue of a voice, and the number of times that it follows itself in the queue
struct QueuedPart
{
	SamplePart part {SamplePart::Whole};
	int32_t copies {0};

	[[nodiscard]] constexpr bool operator==(const QueuedPart&) const = default;
};

/// How a voice gives its sample to the mixer
struct SampleFeed
{
	/// The voice holds the sample as its one buffer. Only then does the mixer use the loop region of the sample.
	bool attached {false};
	/// The parts of the sample in the queue of the voice, in the sequence that they play, when the voice does not
	/// hold the sample as one buffer
	std::vector<QueuedPart> queue;

	[[nodiscard]] bool operator==(const SampleFeed&) const = default;
};

/// How a voice gives its sample to the mixer, from its loop count and the loop region of the sample.
///
/// A loop count below 0 loops until the game stops the voice or releases the loop. Such a voice holds the sample as
/// one buffer. The mixer plays the sample from its start, loops over the region, and continues to the end after the
/// game releases the loop.
///
/// For a loop count of 0 or more, the sound library plays these parts in sequence:
/// 1. The frames before the region, one time.
/// 2. The region, loopCount + 1 times.
/// 3. The frames after the region, one time.
///
/// A sample with no region loops over all of its frames, so the sample plays loopCount + 1 times.
[[nodiscard]] inline SampleFeed SampleFeedOf(int32_t loopCount, std::optional<LoopRegion> region, int32_t frames)
{
	if (loopCount < 0)
	{
		return {.attached = true, .queue = {}};
	}
	if (!region || loopCount == 0)
	{
		return {.attached = false, .queue = {{.part = SamplePart::Whole, .copies = loopCount + 1}}};
	}
	SampleFeed feed {.attached = false, .queue = {}};
	if (region->first > 0)
	{
		feed.queue.push_back({.part = SamplePart::BeforeRegion, .copies = 1});
	}
	feed.queue.push_back({.part = SamplePart::Region, .copies = loopCount + 1});
	if (region->end < frames)
	{
		feed.queue.push_back({.part = SamplePart::AfterRegion, .copies = 1});
	}
	return feed;
}

} // namespace openblack::audio
