/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <string>
#include <vector>

namespace openblack::frame_stats
{

/// How long a run of frames took, in milliseconds
struct Summary
{
	size_t count {0};
	float average {0.0f};
	/// Half of the frames took this long or less
	float p50 {0.0f};
	/// 95 frames in 100 took this long or less
	float p95 {0.0f};
	/// 99 frames in 100 took this long or less
	float p99 {0.0f};
	float max {0.0f};
	/// How many frames took the slow time or more
	size_t slow {0};
};

/// A frame that takes this time or more is too slow for 100 frames a second
constexpr float k_SlowMilliseconds = 10.0f;

/// One frame of a run: how long it took, its time on the processor, the game turn at its end, and its work time
struct FrameRecord
{
	float milliseconds {0.0f};
	float processorMilliseconds {0.0f};
	uint32_t turn {0};
	/// The frame played a game turn
	bool played {false};
	/// The time that the game thread did the work of the frame, without a wait for the display frame or for the pace
	float workMilliseconds {0.0f};
};

/// The frames of a run as CSV text: a line of names, then a line for each frame, numbered from 1
[[nodiscard]] std::string ToCsv(std::span<const FrameRecord> frames);

/// Sums up a run of frame times; an empty run gives an empty summary
[[nodiscard]] Summary Summarise(std::span<const float> milliseconds);

/// Collects the times of the frames since it was last summed up
class Window
{
public:
	void Add(float milliseconds) { _times.push_back(milliseconds); }
	[[nodiscard]] size_t Count() const { return _times.size(); }
	/// Sums up the frames collected so far and starts again
	[[nodiscard]] Summary Take();
	/// Sums up the frames collected so far and keeps them
	[[nodiscard]] Summary Peek() const;

private:
	std::vector<float> _times;
};

} // namespace openblack::frame_stats
