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

namespace openblack::audio
{

/// The rules of a ring of sample frames with one writer and one reader. The writer and the reader each count the frames
/// that they moved since the start. The counts can wrap round, so the difference of the two counts is the number of
/// frames in the ring.
struct AudioRing
{
	/// The number of frames that the ring holds
	uint32_t capacity {0};

	/// The number of frames that the writer put in the ring and the reader did not take yet
	[[nodiscard]] static constexpr uint32_t Filled(uint32_t written, uint32_t read) { return written - read; }

	/// The number of frames that the writer can put in the ring now
	[[nodiscard]] constexpr uint32_t Free(uint32_t written, uint32_t read) const
	{
		return capacity - std::min(Filled(written, read), capacity);
	}

	/// The place in the ring of the frame with this count
	[[nodiscard]] constexpr uint32_t Place(uint32_t count) const { return capacity == 0 ? 0 : count % capacity; }

	/// The number of frames that the writer must render now to keep at least the target in the ring. The writer renders
	/// in blocks of the block size, and never more than the free space.
	[[nodiscard]] constexpr uint32_t ToRender(uint32_t written, uint32_t read, uint32_t target, uint32_t block) const
	{
		const auto filled = Filled(written, read);
		if (block == 0 || filled >= target)
		{
			return 0;
		}
		const auto wanted = (target - filled + block - 1) / block * block;
		const auto free = Free(written, read) / block * block;
		return std::min(wanted, free);
	}
};

/// The two parts of the ring that a run of frames from a place takes: the frames to the end of the ring, then the frames
/// from its start
struct RingParts
{
	uint32_t first {0};
	uint32_t firstCount {0};
	uint32_t secondCount {0};

	[[nodiscard]] constexpr bool operator==(const RingParts&) const = default;
};

/// The parts of the ring that this many frames take from the frame with this count
[[nodiscard]] constexpr RingParts PartsOf(const AudioRing& ring, uint32_t count, uint32_t frames)
{
	const auto first = ring.Place(count);
	const auto firstCount = std::min(frames, ring.capacity - first);
	return {.first = first, .firstCount = firstCount, .secondCount = frames - firstCount};
}

} // namespace openblack::audio
