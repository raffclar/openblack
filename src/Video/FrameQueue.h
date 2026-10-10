/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>

#include <array>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include <BinkDecoder.h>

namespace openblack::video
{

/// Where a video's frames are decoded
enum class DecodeMode : uint8_t
{
	/// On a thread of its own, a few frames ahead of the one shown, so that playing never holds up a game frame
	Worker,
	/// On the caller's thread, when the frame is asked for: for tests, which need it decoded at once
	Inline,
};

/// One decoded frame's planes, copied out of the decoder
struct DecodedFrame
{
	uint32_t frame {0};
	/// The frames decoded well up to and including this one; 0 while none has, and the picture is then black. A frame
	/// that fails keeps the picture before it and the same count
	uint32_t goodFrames {0};
	std::array<std::vector<uint8_t>, 3> planes;
	std::array<uint32_t, 3> strides {};
};

/// A video's frames decoded in order, ahead of the clock: the player asks for the newest frame due, and is given the
/// newest decoded at or before it without waiting. The decoded frames live in a few buffers made once for the video's
/// size, reused as the frames shown move on
class FrameQueue
{
public:
	/// Frames held at once: the one shown and those decoded ahead of it
	static constexpr size_t k_Slots = 4;

	FrameQueue(bink::FrameReader reader, DecodeMode mode);
	~FrameQueue();
	FrameQueue(const FrameQueue&) = delete;
	FrameQueue& operator=(const FrameQueue&) = delete;
	FrameQueue(FrameQueue&&) = delete;
	FrameQueue& operator=(FrameQueue&&) = delete;

	/// The newest decoded frame at or before `frame`, kept until the next call; the frames before it are let go. None
	/// until the first frame is decoded. Inline, every frame up to `frame` is decoded first
	[[nodiscard]] const DecodedFrame* Latest(uint32_t frame);

	/// Waits until every frame up to `frame` (or the last one) is decoded or held; for tests
	void WaitFor(uint32_t frame);

private:
	enum class SlotState : uint8_t
	{
		Free,
		Writing,
		Ready,
	};

	/// The worker: decodes the next frame whenever a buffer is free, until stopped or past the last frame
	void Run();
	/// Decodes the next frame into a slot reserved for it
	void DecodeInto(DecodedFrame& slot);

	bink::FrameReader _reader;
	const uint32_t _frameCount;
	std::array<DecodedFrame, k_Slots> _slots;
	std::array<SlotState, k_Slots> _states {};
	/// The slot last given out by Latest, held until the next call
	std::optional<size_t> _shown;
	/// The next frame to decode and the good ones so far; only the decoding thread touches them
	uint32_t _next {0};
	uint32_t _goodFrames {0};
	/// Frames decoded or held so far: up to and excluding this one; guarded by the mutex
	uint32_t _published {0};
	std::mutex _mutex;
	std::condition_variable _changed;
	bool _stop {false};
	std::thread _worker;
};

} // namespace openblack::video
