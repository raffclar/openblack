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

#include "Video/VideoPlayer.h"

namespace openblack::debug::gui
{

/// The pacing of the Video debug window: how many frames of a film should have been shown, from the time it has been
/// playing and the steps asked for. The frames are due as the game's video player has them (video::FramesDue), counted
/// from the last time it started playing. No clock of its own: the window passes the milliseconds.
class VideoPlayback
{
public:
	/// A film of `frames` frames at `fpsNumerator` / `fpsDenominator`: paused on its first frame
	void Reset(uint32_t frames, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept
	{
		_frames = frames;
		_fpsNumerator = fpsNumerator;
		_fpsDenominator = fpsDenominator;
		Restart();
	}

	/// Plays on from the frame on screen: the next one a frame's time later, as the game's player paces them
	void Play() noexcept
	{
		if (_frames == 0 || _due >= _frames)
		{
			return;
		}
		_playing = true;
		_start = _due > 0 ? _due - 1 : 0;
		_playedMs = 0;
		Advance(0);
	}

	void Pause() noexcept { _playing = false; }

	/// One frame more, paused
	void Step() noexcept
	{
		_playing = false;
		_due = std::min(_frames, _due + 1);
	}

	/// Back to the first frame, paused
	void Restart() noexcept
	{
		_playing = false;
		_playedMs = 0;
		_start = 0;
		_due = std::min(_frames, 1u);
	}

	/// `ms` more of playing (nothing while paused); at the last frame it stops
	void Advance(uint64_t ms) noexcept
	{
		if (!_playing)
		{
			return;
		}
		_playedMs += ms;
		const uint32_t due = video::FramesDue(_playedMs, _fpsNumerator, _fpsDenominator);
		_due = std::min(_frames, _start + due);
		if (_due >= _frames)
		{
			_playing = false;
		}
	}

	/// The frames that should have been shown by now (frame _due - 1 on screen)
	[[nodiscard]] uint32_t Due() const noexcept { return _due; }
	[[nodiscard]] bool Playing() const noexcept { return _playing; }
	/// How long it has played since it last started
	[[nodiscard]] uint64_t PlayedMs() const noexcept { return _playedMs; }
	[[nodiscard]] uint32_t Frames() const noexcept { return _frames; }

private:
	uint32_t _frames {0};
	uint32_t _fpsNumerator {1};
	uint32_t _fpsDenominator {1};
	uint32_t _due {0};
	uint32_t _start {0}; ///< the frame on screen when it last started playing
	uint64_t _playedMs {0};
	bool _playing {false};
};

} // namespace openblack::debug::gui
