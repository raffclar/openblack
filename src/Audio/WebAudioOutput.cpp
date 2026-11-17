/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#ifdef __EMSCRIPTEN__

#include "WebAudioOutput.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include <emscripten.h>
#include <spdlog/spdlog.h>

using namespace openblack::audio;

namespace
{
/// The number of stereo frames in the ring. It is a power of two, so that the place of a count stays correct when the
/// count wraps round.
constexpr uint32_t k_RingFrames = 8192;
static_assert((k_RingFrames & (k_RingFrames - 1)) == 0);
constexpr uint32_t k_Channels = 2;
/// The time of sound that the thread keeps in the ring before the worklet plays it
constexpr auto k_Ahead = std::chrono::milliseconds(60);
/// The thread mixes in blocks of this many frames
constexpr uint32_t k_BlockFrames = 256;
/// The time that the thread waits when the ring has enough frames
constexpr auto k_Wait = std::chrono::milliseconds(2);
/// When the worklet takes no frames for this time, the thread mixes at the speed of the clock and discards the sound
constexpr auto k_Stalled = std::chrono::milliseconds(250);
/// The most frames that the thread discards in one step
constexpr uint32_t k_MostDiscarded = 4096;
/// The time between two log lines about the blocks that the worklet played with too few frames
constexpr auto k_ReportTime = std::chrono::seconds(10);
} // namespace

// The page code of the sound output is in web/page/post.js and web/page/audio-output-worklet.js
int WebAudioOutput::OpenPageAudio()
{
	// clang-format off
	return MAIN_THREAD_EM_ASM_INT({
		return Module['openblackAudioOpen'] ? Module['openblackAudioOpen']() : 0;
	});
	// clang-format on
}

WebAudioOutput::WebAudioOutput(int sampleRate, Mixer mixer)
    : _sampleRate(sampleRate)
    , _mixer(std::move(mixer))
    , _ring {.capacity = k_RingFrames}
    , _samples(static_cast<size_t>(k_RingFrames) * k_Channels, 0.0f)
    , _discarded(static_cast<size_t>(k_MostDiscarded) * k_Channels, 0.0f)
{
	_thread = std::jthread([this](const std::stop_token& stop) { Run(stop); });
	// The main thread of the page loads the worklet and connects it to the ring. The game thread does not wait.
	// The JavaScript code names the arguments $0 to $3
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
	// clang-format off
	MAIN_THREAD_ASYNC_EM_ASM({
		if (Module['openblackAudioConnect']) {
			Module['openblackAudioConnect']($0, $1, $2, $3);
		}
	}, _samples.data(), k_RingFrames, k_Channels, &_counters);
	// clang-format on
#pragma clang diagnostic pop
}

WebAudioOutput::~WebAudioOutput()
{
	_thread.request_stop();
	_thread.join();
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (Module['openblackAudioClose']) {
			Module['openblackAudioClose']();
		}
	});
	// clang-format on
}

void WebAudioOutput::Discard(uint32_t frames)
{
	while (frames > 0)
	{
		const auto count = std::min(frames, k_MostDiscarded);
		_mixer(std::span(_discarded).first(static_cast<size_t>(count) * k_Channels), count);
		frames -= count;
	}
}

void WebAudioOutput::Run(const std::stop_token& stop)
{
	using Clock = std::chrono::steady_clock;
	const auto ahead = static_cast<uint32_t>(_sampleRate * k_Ahead.count() / 1000);
	const auto target = std::min(ahead, k_RingFrames - k_BlockFrames);
	auto lastRead = _counters.read.load(std::memory_order_acquire);
	auto lastReadTime = Clock::now();
	auto discardTime = lastReadTime;
	auto reportTime = lastReadTime;
	uint32_t reportedShortBlocks = 0;

	while (!stop.stop_requested())
	{
		const auto now = Clock::now();
		const auto written = _counters.written.load(std::memory_order_relaxed);
		const auto read = _counters.read.load(std::memory_order_acquire);
		if (read != lastRead)
		{
			lastRead = read;
			lastReadTime = now;
		}

		if (now - reportTime >= k_ReportTime)
		{
			reportTime = now;
			const auto shortBlocks = _counters.shortBlocks.load(std::memory_order_relaxed);
			if (shortBlocks != reportedShortBlocks)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "The sound output played {} blocks with too few frames",
				                   shortBlocks - reportedShortBlocks);
				reportedShortBlocks = shortBlocks;
			}
		}

		const auto frames = _ring.ToRender(written, read, target, k_BlockFrames);
		if (frames > 0)
		{
			const auto parts = PartsOf(_ring, written, frames);
			const auto samples = std::span(_samples);
			_mixer(samples.subspan(static_cast<size_t>(parts.first) * k_Channels,
			                       static_cast<size_t>(parts.firstCount) * k_Channels),
			       parts.firstCount);
			if (parts.secondCount > 0)
			{
				_mixer(samples.first(static_cast<size_t>(parts.secondCount) * k_Channels), parts.secondCount);
			}
			_counters.written.store(written + frames, std::memory_order_release);
			discardTime = now;
			continue;
		}

		// The ring is full. When the worklet takes no frames, the sounds play on at the speed of the clock.
		if (now - lastReadTime >= k_Stalled)
		{
			const auto seconds = std::chrono::duration<double>(now - discardTime).count();
			const auto count = static_cast<uint32_t>(seconds * _sampleRate);
			if (count > 0)
			{
				Discard(count);
				discardTime += std::chrono::duration_cast<Clock::duration>(
				    std::chrono::duration<double>(static_cast<double>(count) / _sampleRate));
			}
		}
		else
		{
			discardTime = now;
		}
		std::this_thread::sleep_for(k_Wait);
	}
}

#endif // __EMSCRIPTEN__
