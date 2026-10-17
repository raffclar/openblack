/*******************************************************************************
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

#include <deque>
#include <functional>
#include <map>
#include <vector>

#include "AudioPlayerInterface.h"

namespace openblack::audio
{

/// A silent audio player for tool runs that must go the same way at any speed. Each sound lasts its length by a clock
/// that the player is given, such as the game's machine clock, which moves on by the fixed time of each frame. The
/// sounds that the scripts and the speech wait for then end after the same frames each run, at any speed.
///
/// A source plays the buffers in its queue one after the other, faster or slower with its pitch. A looping source does
/// not stop. A clock that goes back, as when the game starts its clock again, counts as no time.
class ClockedAudioPlayer final: public AudioPlayerInterface
{
public:
	/// The time in milliseconds
	using Clock = std::function<uint32_t()>;

	explicit ClockedAudioPlayer(Clock clock);

	void Initialize() override {}
	void UpdateListener(glm::vec3 /*pos*/, glm::vec3 /*front*/, glm::vec3 /*up*/) const override {}
	[[nodiscard]] BufferId CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate) override;
	void QueueBuffer(SourceId sourceId, BufferId buffer) override;
	void DeleteBuffer(BufferId id) override;
	[[nodiscard]] SourceId CreateSource(float pitch, bool relative) override;
	void DeleteSource(SourceId id) override;
	void UpdateSource(SourceId id, glm::vec3 pos, float volume, bool loop) override;
	void UpdateSource(SourceId id, float volume, bool loop) override;
	[[nodiscard]] float GetDuration(BufferId id) override;
	void PlaySource(SourceId id, glm::vec3 pos, float volume, bool loop) override;
	void PlaySource(SourceId id, float volume, bool loop) override;
	void PauseSource(SourceId id) const override;
	void StopSource(SourceId id) const override;
	void SetVolume(SourceId /*id*/, float /*volume*/) override {}
	void SetPitch(SourceId id, float pitch) override;
	void SetPosition(SourceId /*id*/, glm::vec3 /*position*/) override {}
	void SetDistanceAttenuation(SourceId /*id*/, float /*referenceDistance*/, float /*maxDistance*/, float /*rolloff*/) override
	{
	}
	void SetLooping(SourceId id, bool loop) override;
	void StartSource(SourceId id) override;
	[[nodiscard]] float GetVolume() const override { return 1.0f; }
	[[nodiscard]] AudioStatus GetStatus(SourceId id) const override;
	[[nodiscard]] float GetProgress(size_t sizeInBytes, SourceId sourceId) const override;
	[[nodiscard]] std::vector<BufferId> UnqueueProcessedBuffers(SourceId id) override;
	[[nodiscard]] uint32_t GetSampleOffset(SourceId id) const override;

private:
	struct Buffer
	{
		size_t frames {0};
		size_t channels {1};
		int sampleRate {1};

		[[nodiscard]] double Milliseconds() const;
	};
	struct Source
	{
		std::deque<BufferId> queue;
		AudioStatus status {AudioStatus::Initial};
		/// How far it is through its queue, in milliseconds of sound
		double offsetMs {0.0};
		uint32_t lastTick {0};
		float pitch {1.0f};
		bool loop {false};
	};

	/// The source moves on to now
	void Advance(Source& source) const;
	[[nodiscard]] double QueueMilliseconds(const Source& source) const;
	void Play(SourceId id, bool loop);
	[[nodiscard]] Source* Find(SourceId id) const;

	Clock _clock;
	std::map<BufferId, Buffer> _buffers;
	mutable std::map<SourceId, Source> _sources;
	BufferId _nextBuffer {1};
	SourceId _nextSource {1};
};

} // namespace openblack::audio
