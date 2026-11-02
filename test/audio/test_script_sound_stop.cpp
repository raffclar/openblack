/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A script that stops a sound stops only the copy that a script started. A script's sound has its sample number as
// its owner, so a copy of the same sample that the game plays continues.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <memory>
#include <vector>

#include <PackFile.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManager.h"
#include "Audio/AudioPlayerInterface.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The sample number of the sound in its bank
constexpr uint32_t k_Sample = 170;
constexpr entt::id_type k_Sound = entt::hashed_string("InGame.sad/170").value();

/// A sound device that plays nothing. Each source plays until the game stops it.
class FakeSoundDevice final: public AudioPlayerInterface
{
public:
	void Initialize() override {}
	void UpdateListener(glm::vec3, glm::vec3, glm::vec3) const override {}
	BufferId CreateBuffer(ChannelLayout, const std::vector<int16_t>&, int) override { return _nextBuffer++; }
	void QueueBuffer(SourceId, BufferId) override {}
	void DeleteBuffer(BufferId) override {}
	SourceId CreateSource(float, bool) override { return _nextSource++; }
	void DeleteSource(SourceId) override {}
	void UpdateSource(SourceId, glm::vec3, float, bool) override {}
	void UpdateSource(SourceId, float, bool) override {}
	float GetDuration(BufferId) override { return 0.0f; }
	void PlaySource(SourceId, glm::vec3, float, bool) override {}
	void PlaySource(SourceId, float, bool) override {}
	void PauseSource(SourceId) const override {}
	void StopSource(SourceId) const override {}
	void SetVolume(SourceId, float) override {}
	void SetPitch(SourceId, float) override {}
	void SetPosition(SourceId, glm::vec3) override {}
	void SetDistanceAttenuation(SourceId, float, float, float) override {}
	void SetLooping(SourceId, bool) override {}
	void StartSource(SourceId) override {}
	[[nodiscard]] float GetVolume() const override { return 1.0f; }
	[[nodiscard]] AudioStatus GetStatus(SourceId) const override { return AudioStatus::Playing; }
	[[nodiscard]] float GetProgress(size_t, SourceId) const override { return 0.0f; }
	std::vector<BufferId> UnqueueProcessedBuffers(SourceId) override { return {}; }
	[[nodiscard]] uint32_t GetSampleOffset(SourceId) const override { return 0; }
	[[nodiscard]] bool IsDeviceConnected() const override { return true; }

private:
	BufferId _nextBuffer {1};
	SourceId _nextSource {1};
};

class ScriptSoundStop: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("audio") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("audio");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::resources::emplace<resources::Resources>();
		pack::AudioBankSampleHeader header {};
		header.id = static_cast<int32_t>(k_Sample);
		header.priority = 400;
		Locator::resources::value().GetSounds().Load(k_Sound, resources::SoundLoader::FromBufferTag {}, header,
		                                             std::vector<std::vector<uint8_t>> {});
		_audio = std::make_unique<AudioManager>(std::make_unique<FakeSoundDevice>());
	}
	void TearDown() override
	{
		// The sound stops its emitters before the sounds go
		_audio.reset();
		Locator::resources::reset();
		Locator::entitiesRegistry::reset();
	}

	/// A copy of the sample as a script starts it: the sample number is its owner
	entt::entity StartAsScript()
	{
		return Audio().StartSoundEffect(k_Sound, {.voiceOwner = VoiceOwner::Number, .ownerNumber = k_Sample});
	}

	[[nodiscard]] AudioManager& Audio() { return *_audio; }

private:
	std::unique_ptr<AudioManager> _audio;
};
} // namespace

TEST_F(ScriptSoundStop, TheStopLeavesTheCopyThatTheWorldPlays)
{
	const auto world = Audio().StartSoundEffect(k_Sound, {});
	const auto script = StartAsScript();
	ASSERT_TRUE(world != entt::null);
	ASSERT_TRUE(script != entt::null);

	Audio().StopNumberedSoundEffect(k_Sound, k_Sample);
	EXPECT_FALSE(Audio().EmitterExists(script));
	EXPECT_TRUE(Audio().EmitterExists(world));
}

TEST_F(ScriptSoundStop, OnlyTheCopyThatAScriptStartedReadsAsPlaying)
{
	const auto world = Audio().StartSoundEffect(k_Sound, {});
	ASSERT_TRUE(world != entt::null);
	EXPECT_FALSE(Audio().IsNumberedSoundEffectPlaying(k_Sound, k_Sample));

	ASSERT_TRUE(StartAsScript() != entt::null);
	EXPECT_TRUE(Audio().IsNumberedSoundEffectPlaying(k_Sound, k_Sample));
	Audio().StopNumberedSoundEffect(k_Sound, k_Sample);
	EXPECT_FALSE(Audio().IsNumberedSoundEffectPlaying(k_Sound, k_Sample));
}

TEST_F(ScriptSoundStop, AStopWithNoScriptCopyStopsNothing)
{
	const auto world = Audio().StartSoundEffect(k_Sound, {});
	ASSERT_TRUE(world != entt::null);
	Audio().StopNumberedSoundEffect(k_Sound, k_Sample);
	EXPECT_TRUE(Audio().EmitterExists(world));
}
