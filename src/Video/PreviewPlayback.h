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
#include <chrono>

#include "VideoRules.h"

namespace openblack::video
{

/// The debug video viewer's playback: play, pause, step and restart, keeping a video's own pace by the real clock and
/// stopping on its last frame
class PreviewPlayback
{
public:
	PreviewPlayback(uint32_t frameCount, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept
	    : _frameCount(std::max(frameCount, 1u))
	    , _fpsNumerator(std::max(fpsNumerator, 1u))
	    , _fpsDenominator(std::max(fpsDenominator, 1u))
	{
	}

	void Play() noexcept
	{
		if (_frame + 1 >= _frameCount)
		{
			Restart();
		}
		_playing = true;
	}
	void Pause() noexcept { _playing = false; }
	/// Back to the first frame, paused or playing as it was
	void Restart() noexcept
	{
		_frame = 0;
		_played = {};
	}
	/// One frame on, paused
	void Step() noexcept
	{
		_playing = false;
		_frame = std::min(_frame + 1, _frameCount - 1);
		_played = TimeOf(_frame);
	}

	/// Moves on by `elapsed` real time while playing; returns the frame to show
	uint32_t Advance(std::chrono::microseconds elapsed) noexcept
	{
		if (_playing)
		{
			_played += elapsed;
			_frame = std::min(FramesDue(_played, _fpsNumerator, _fpsDenominator) - 1, _frameCount - 1);
			if (_frame + 1 >= _frameCount)
			{
				_playing = false;
			}
		}
		return _frame;
	}

	[[nodiscard]] uint32_t Frame() const noexcept { return _frame; }
	[[nodiscard]] bool IsPlaying() const noexcept { return _playing; }

private:
	[[nodiscard]] std::chrono::microseconds TimeOf(uint32_t frame) const noexcept
	{
		// Rounded up, so that the frame is due at that time
		const uint64_t numerator = uint64_t {frame} * _fpsDenominator * 1'000'000;
		return std::chrono::microseconds((numerator + _fpsNumerator - 1) / _fpsNumerator);
	}

	uint32_t _frameCount;
	uint32_t _fpsNumerator;
	uint32_t _fpsDenominator;
	uint32_t _frame {0};
	std::chrono::microseconds _played {0};
	bool _playing {false};
};

} // namespace openblack::video
