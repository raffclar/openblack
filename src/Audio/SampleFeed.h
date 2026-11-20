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

/// The frame of the sample that a voice continues from when the game releases its loop, after the voice played this
/// many frames of its queue. The sound library then plays from where the voice is to the end of the sample, with no
/// more passes of the region.
///
/// The value is empty when the voice does not need a change:
/// - The voice holds the sample as one buffer. The mixer plays on to the end when the source stops its loop.
/// - The rest of the queue already plays from where the voice is to the end of the sample.
/// - The voice played all of its queue.
[[nodiscard]] constexpr std::optional<int32_t> ReleasedFrameOf(const SampleFeed& feed, std::optional<LoopRegion> region,
                                                               int32_t frames, int64_t playedFrames)
{
	if (feed.attached || playedFrames < 0)
	{
		return std::nullopt;
	}
	// A sample with no region loops over all of its frames
	const auto loop = region.value_or(LoopRegion {.first = 0, .end = frames});
	int64_t queueFrames = 0;
	std::optional<int32_t> frame;
	for (const auto& queued : feed.queue)
	{
		const auto part = FramesOf(queued.part, loop, frames);
		const int64_t length = part.end - part.first;
		if (length <= 0 || queued.copies <= 0)
		{
			continue;
		}
		const auto partEnd = queueFrames + length * queued.copies;
		if (!frame && playedFrames < partEnd)
		{
			frame = part.first + static_cast<int32_t>((playedFrames - queueFrames) % length);
		}
		queueFrames = partEnd;
	}
	if (!frame)
	{
		return std::nullopt;
	}
	// The voice changes only when the rest of its queue is not the rest of the sample
	if (queueFrames - playedFrames == static_cast<int64_t>(frames) - *frame)
	{
		return std::nullopt;
	}
	return frame;
}

} // namespace openblack::audio
