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

#include <memory>
#include <vector>

#include "SampleOutput.h"

namespace openblack::audio
{

/// (openblack, test runs) With OPENBLACK_FIXED_FRAME_MS the game's clocks repeat, but the device plays its samples in
/// real time, and the help / advisor logic waits on them: HelpSystem's isPlaying and advisorsTalking
/// (TalkingOrJustStopped, refreshed by PercentageDone).
/// This output forwards every call to the real one (the sound is still heard) but answers Playing and PlayPositionMs
/// from a timeline of its own on the fixed clock (device::TickCount): a sample sounds for its decoded length divided by
/// its pitch, times its passes. device::Open wraps the real output in it only while fixed_clock::Enabled(); off, nothing
/// changes. (approximate) a wave's loop section (Sound::loopStart / loopEnd) counts as the whole wave for each pass,
/// and QMixer's end callback lag after the last buffer is not modelled: the tests only need the runs to repeat
class FixedClockSampleOutput final: public SampleOutput
{
public:
	explicit FixedClockSampleOutput(std::unique_ptr<SampleOutput> real);

	bool Play(size_t channel, Sound& sound, const Start& start) override;
	void Stop(size_t channel) override;
	void StopRamped(size_t channel) override;
	void CutFade(size_t channel) override;
	[[nodiscard]] int64_t PlayPositionMs(size_t channel) const override;
	[[nodiscard]] bool Playing(size_t channel) const override;
	void SetGain(size_t channel, float gain) override;
	void SetPitch(size_t channel, float ratio) override;
	void SetPosition(size_t channel, glm::vec3 position) override;
	void ReleaseLoop(size_t channel) override;
	void SetListener(glm::vec3 position) override;
	void Update() override;
	[[nodiscard]] size_t Sources() const override;
	void DeleteAll() override;

	/// One channel's timeline (public for the tests)
	struct Timeline
	{
		bool playing {false};
		uint32_t startTick {0}; ///< device::TickCount at the start, or at the last pitch change
		double playedMs {0.0};  ///< the wave ms played before startTick (pitch changes fold into it)
		double lengthMs {0.0};  ///< one pass, in the wave's ms
		float pitch {1.0f};
		int loops {0}; ///< 0 once, -1 for ever, N > 0 N more passes (SampleOutput::Start::loops)

		void Start(uint32_t now, double length, float ratio, int passes);
		[[nodiscard]] double Elapsed(uint32_t now) const;
		[[nodiscard]] bool Sounding(uint32_t now) const;
		[[nodiscard]] int64_t PositionMs(uint32_t now) const;
		void ChangePitch(uint32_t now, float ratio);
		void ReleaseLoop(uint32_t now);
	};

private:
	Timeline* At(size_t channel);
	[[nodiscard]] const Timeline* At(size_t channel) const;

	std::unique_ptr<SampleOutput> _real;
	std::vector<Timeline> _channels;
};

} // namespace openblack::audio
