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

#include <glm/vec3.hpp>

namespace openblack::audio
{
class Sound;

/// What the mixer does with one of the audio library's 16 sample channels: the device side of audio::sample_play.
/// Each channel owns one OpenAL source (made at its first start, deleted by DeleteAll), out of the ECS registry, so a
/// registry reset leaves no orphan source. The logic of the channels (allocation, modes, priorities, volumes, owners)
/// is in SamplePlay; the tests use a fake output.
class SampleOutput
{
public:
	/// How a channel starts a sample
	struct Start
	{
		/// The gain (qmixer::Gain of the channel volume and the sample main volume)
		float gain {1.0f};
		/// The frequency as a ratio of the wave's rate (qmixer::FrequencyRatio of the start pitch)
		float pitch {1.0f};
		/// a 3D channel; 2D: on the listener, no distance mapping
		bool is3D {false};
		/// the position is in the listener's frame: (right, up, ahead) in openblack's axes (the device swaps x <-> z for
		/// OpenAL), i.e. already through qmixer::PolarRelative
		bool relative {false};
		glm::vec3 position {0.0f};
		/// The distance mapping {min, max, scale}
		float minDistance {1.0f};
		float maxDistance {9999.0f};
		float scale {0.3f};
		/// The loops: 0 once, -1 for ever, N > 0 N more passes after the first (inferred: the mixer's "number of times to
		/// loop")
		int loops {0};
	};

	virtual ~SampleOutput() = default;
	/// The channel starts the sample's wave (restarted if it was playing). False when the wave cannot be played.
	virtual bool Play(size_t channel, Sound& sound, const Start& start) = 0;
	/// The channel stops at once
	virtual void Stop(size_t channel) = 0;
	/// A sample stop: a 20 ms ramp to silence, then the channel flushed. The channel counts as stopped at once (Playing
	/// is false and it may start again); the fade goes on in the background and the caller does not wait (the original
	/// waited the 20 ms). Default: Stop.
	virtual void StopRamped(size_t channel) { Stop(channel); }
	/// The fades of the channel's ramped stops still sounding are cut at once (StopAll). Default: nothing.
	virtual void CutFade(size_t /*channel*/) {}
	/// The channel's play position in milliseconds, -1 when it is not playing. Default: -1.
	[[nodiscard]] virtual int64_t PlayPositionMs(size_t /*channel*/) const { return -1; }
	/// The channel's wave is still sounding (cleared by the mixer's end callback)
	[[nodiscard]] virtual bool Playing(size_t channel) const = 0;
	/// The channel's gain
	virtual void SetGain(size_t channel, float gain) = 0;
	/// The channel's frequency ratio
	virtual void SetPitch(size_t channel, float ratio) = 0;
	/// The source position (a 3D channel)
	virtual void SetPosition(size_t channel, glm::vec3 position) = 0;
	/// The remaining loops go to 0, the current pass ends
	virtual void ReleaseLoop(size_t channel) = 0;
	/// The listener position (once a turn): the mixer mutes a channel beyond its max distance
	virtual void SetListener(glm::vec3 position) = 0;
	/// Once a frame: the finite loops are counted
	virtual void Update() = 0;
	/// The OpenAL sources alive (a channel's source is made at its first start)
	[[nodiscard]] virtual size_t Sources() const = 0;
	/// Every channel stopped and its source deleted
	virtual void DeleteAll() = 0;
};

/// The finite loops of a channel (loops = N > 0): the source loops and every wrap of its play offset is
/// one pass; after N wraps it stops looping, so the current pass ends and the wave plays on to its end (N + 1 passes of
/// the loop section, inferred). Pure, for the tests.
struct LoopCounter
{
	/// The wraps still to come before the looping stops (0: nothing to count)
	int remaining {0};
	int64_t lastOffset {0};

	void Start(int loops)
	{
		remaining = loops > 0 ? loops : 0;
		lastOffset = 0;
	}
	/// The play offset of this frame; true when the looping must stop now
	bool Feed(int64_t offset)
	{
		if (remaining <= 0)
		{
			return false;
		}
		if (offset < lastOffset)
		{
			--remaining;
		}
		lastOffset = offset;
		return remaining == 0;
	}
};

/// No device (device::Open failed): nothing ever plays
class NullSampleOutput final: public SampleOutput
{
public:
	bool Play(size_t, Sound&, const Start&) override { return false; }
	void Stop(size_t) override {}
	[[nodiscard]] bool Playing(size_t) const override { return false; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override {}
};

} // namespace openblack::audio
