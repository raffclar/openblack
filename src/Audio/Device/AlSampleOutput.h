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

#include <array>
#include <mutex>
#include <thread>

#include "Audio/Device/RampedStops.h"
#include "Audio/Device/SampleOutput.h"

namespace openblack::audio
{

/// The 16 sample channels on the audio device (Device.h: its one OpenAL context, no second device). Positions are
/// openblack's world (or listener) coordinates (the device swaps x and z for OpenAL). The distance curve up to
/// the max is OpenAL's inverse distance clamped (reference = min, rolloff = scale, as the original mixer) and beyond
/// the max the channel is muted as the original mixer does. A ramped stop fades on a fader thread of its own (see
/// StopRamped); every other call is made on the game thread.
class AlSampleOutput final: public SampleOutput
{
public:
	static constexpr size_t k_Channels = 16;

	AlSampleOutput() = default;
	AlSampleOutput(const AlSampleOutput&) = delete;
	AlSampleOutput& operator=(const AlSampleOutput&) = delete;
	~AlSampleOutput() override;
	bool Play(size_t channel, Sound& sound, const Start& start) override;
	void Stop(size_t channel) override;
	/// The original mixer's 20 ms volume ramp as four gain steps of 5 ms (approximate), then the flush. The channel is
	/// stopped and free at once; its source fades on the fader thread and the call returns without waiting
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

private:
	struct Slot
	{
		uint32_t source {0};
		bool is3D {false};
		bool relative {false};
		bool looping {false};
		float gain {1.0f};
		float maxDistance {9999.0f};
		glm::vec3 position {0.0f};
		LoopCounter loop;
	};
	/// The gain the source has now (the sound switch and the max distance mute applied)
	[[nodiscard]] float AppliedGain(const Slot& slot) const;
	void ApplyGain(Slot& slot) const;
	/// The fades' steps, 5 ms apart, until the output is destroyed
	void FaderMain(const std::stop_token& stop);

	std::array<Slot, k_Channels> _slots;
	glm::vec3 _listener {0.0f};
	bool _soundsOn {true}; ///< the sound switch (OutputSwitches) as the last Update saw it

	/// The fades and the spare sources, shared with the fader thread under _fadeMutex
	RampedStops _stops;
	mutable std::mutex _fadeMutex;
	std::condition_variable_any _fadeWake;
	/// Started at the first ramped stop; last, so that it is joined before the rest goes
	std::jthread _fader;
};

} // namespace openblack::audio
