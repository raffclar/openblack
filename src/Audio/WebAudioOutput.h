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

#include <atomic>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#include "AudioRing.h"

namespace openblack::audio
{

/// The sound output of the web build. A thread of the game mixes the sound into a ring in the shared memory of the
/// program. An audio worklet of the page plays the ring on the audio thread of the browser. Thus the main thread of
/// the page does no sound work, and a busy main thread does not stop the sound.
///
/// When the browser does not take the frames, for example before the player clicks the page, the thread mixes the
/// sound at the speed of the clock and discards it. Thus the sounds play on in time, as with a sound device.
class WebAudioOutput
{
public:
	/// Mixes this many stereo frames into the samples
	using Mixer = std::function<void(std::span<float> samples, uint32_t frames)>;

	/// Makes the audio context of the page and gives its sample rate, or 0 when the page cannot play sound with an
	/// audio worklet
	[[nodiscard]] static int OpenPageAudio();

	/// Starts the thread that mixes and connects the ring to the audio worklet of the page
	WebAudioOutput(int sampleRate, Mixer mixer);
	~WebAudioOutput();

	WebAudioOutput(const WebAudioOutput&) = delete;
	WebAudioOutput& operator=(const WebAudioOutput&) = delete;

private:
	void Run(const std::stop_token& stop);
	void Discard(uint32_t frames);

	/// The counts of the frames that the thread wrote and that the worklet read, and the number of blocks that the
	/// worklet played with too few frames. The worklet reads and writes these counts with atomic operations.
	struct Counters
	{
		std::atomic<uint32_t> written {0};
		std::atomic<uint32_t> read {0};
		std::atomic<uint32_t> shortBlocks {0};
	};

	int _sampleRate;
	Mixer _mixer;
	AudioRing _ring;
	std::vector<float> _samples;
	std::vector<float> _discarded;
	Counters _counters;
	std::jthread _thread;
};

} // namespace openblack::audio
