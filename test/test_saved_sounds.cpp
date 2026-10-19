/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A saved game keeps no sound. After a load the sounds that played at the save do not come back, and the help box is
// made again from the texts it kept, with the reading time of the newest text started again.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManager.h"
#include "Audio/AudioPlayerInterface.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Registry.h"
#include "Help/DialogueText.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::help;

namespace
{
/// A sound device that plays nothing and counts the sources it is told to stop
class FakeSoundDevice final: public AudioPlayerInterface
{
public:
	explicit FakeSoundDevice(std::shared_ptr<size_t> stopped)
	    : _stopped(std::move(stopped))
	{
	}
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
	void StopSource(SourceId) const override { ++*_stopped; }
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
	std::shared_ptr<size_t> _stopped;
	BufferId _nextBuffer {1};
	SourceId _nextSource {1};
};

class SavedSounds: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("audio") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("audio");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }
};
} // namespace

TEST_F(SavedSounds, TheEmittersThatASaveKeptGoWithoutAStopOfTheirSources)
{
	auto stopped = std::make_shared<size_t>(0);
	AudioManager audio {std::make_unique<FakeSoundDevice>(stopped)};
	auto& registry = Locator::entitiesRegistry::value();
	// An emitter as the load makes it again: its source is a source of the run that saved
	const auto saved = registry.Create();
	registry.Assign<ecs::components::AudioEmitter>(saved, ecs::components::AudioEmitter {
	                                                          .sourceId = 42,
	                                                          .soundId = 7,
	                                                          .state = AudioStatus::Playing,
	                                                      });
	const auto other = registry.Create();
	audio.ForgetSavedEmitters();
	EXPECT_FALSE(registry.Valid(saved));
	EXPECT_FALSE(audio.EmitterExists(saved));
	EXPECT_TRUE(registry.Valid(other));
	EXPECT_EQ(*stopped, 0u);
}

TEST(SavedHelpText, TheBoxIsMadeAgainFromItsTextsAndTheNewestReadsAgain)
{
	uint32_t turn = 100;
	std::map<uint32_t, DialogueText::Text> texts;
	texts[4364] = {.narrator = k_NarratorGoodAdvisor, .text = u"one two three"};
	texts[4365] = {.narrator = k_NarratorGoodAdvisor, .text = u"four five"};
	DialogueText::Queries queries;
	queries.text = [&texts](uint32_t number) {
		const auto it = texts.find(number);
		return it == texts.end() ? DialogueText::Text {} : it->second;
	};
	queries.voice = [](uint32_t) { return VoiceFor(true, true, false); };
	queries.turn = [&turn]() { return turn; };
	queries.nowMs = []() { return 10000; };
	queries.inTemple = []() { return false; };
	queries.advisorsTalking = []() { return false; };
	queries.narrationSounding = [](uint32_t) { return false; };
	queries.scriptWideScreen = []() { return false; };
	DialogueText::Hooks hooks;
	hooks.advisorSays = [](int32_t, uint32_t) {};
	hooks.narrate = [](uint32_t) {};
	hooks.interruptAdvisors = []() {};
	hooks.stopNarration = []() {};
	DialogueText dialogue {480, false, {}, std::move(queries), std::move(hooks)};
	ASSERT_TRUE(dialogue.RunText(false, 4364, 0));
	ASSERT_TRUE(dialogue.RunText(false, 4365, 0));
	const auto readFor = dialogue.GetEndTurn() - turn;

	// The load: the box is empty, the texts stay
	turn = 500;
	dialogue.EmptyBox();
	EXPECT_FALSE(dialogue.GetDisplay().HasNewestText());
	EXPECT_EQ(dialogue.GetCurrentText(), 4365u);

	// Every part has loaded: the texts show again, and the newest reads for its whole time from now
	dialogue.ShowAgain();
	EXPECT_TRUE(dialogue.GetDisplay().HasNewestText());
	EXPECT_EQ(dialogue.GetEndTurn(), turn + readFor);
	EXPECT_EQ(dialogue.Hold().texts[1], 4364u);
}
