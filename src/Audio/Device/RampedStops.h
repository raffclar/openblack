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

#include <chrono>
#include <optional>
#include <vector>

namespace openblack::audio
{

/// The sample stops that fade out while the game goes on. A stopped channel hands its source over: the source fades
/// to silence in 20 ms (four gain steps of 5 ms, the last one at 0) and is then flushed, while the channel itself is
/// free at once and takes another source for its next sample. A flushed source is kept as a spare for a later start.
/// Pure: the time and the sources' gain and flush are given by the caller (the device in the game, fakes in the tests).
class RampedStops
{
public:
	using Clock = std::chrono::steady_clock;
	using SourceId = uint32_t;

	static constexpr std::chrono::milliseconds k_Ramp {20};
	static constexpr int k_Steps = 4;
	static constexpr std::chrono::milliseconds k_StepLength {k_Ramp / k_Steps};

	/// What a fade does to its source
	class Target
	{
	public:
		virtual ~Target() = default;
		virtual void SetGain(SourceId source, float gain) = 0;
		/// The source stops and lets its wave go
		virtual void Flush(SourceId source) = 0;
	};

	/// The gain of a fade's step (1 to k_Steps) from the gain it started at: the last step is silence
	[[nodiscard]] static constexpr float StepGain(float gain, int step)
	{
		return gain * static_cast<float>(k_Steps - step) / static_cast<float>(k_Steps);
	}

	/// The channel's source starts fading from `gain` now: its first step is applied at once
	void Begin(size_t channel, SourceId source, float gain, Clock::time_point now, Target& target);
	/// The steps due by `now` (a late call jumps to the step due); the fades that are over are flushed and their
	/// sources become spares. The time the next step is due, none when nothing fades.
	std::optional<Clock::time_point> Advance(Clock::time_point now, Target& target);
	/// The channel's fades, if any, are flushed at once
	void Cut(size_t channel, Target& target);
	/// A flushed source for a new start, none when there is no spare. A source still fading is never given out
	[[nodiscard]] std::optional<SourceId> TakeSpare();
	/// Every source held, fading or spare, handed back for deletion; nothing is left
	[[nodiscard]] std::vector<SourceId> TakeAll();

	[[nodiscard]] size_t Fading() const { return _fades.size(); }
	[[nodiscard]] size_t Spares() const { return _spares.size(); }

private:
	struct Fade
	{
		size_t channel {0};
		SourceId source {0};
		float gain {1.0f};
		Clock::time_point start;
		int step {1};
	};

	std::vector<Fade> _fades;
	std::vector<SourceId> _spares;
};

} // namespace openblack::audio
