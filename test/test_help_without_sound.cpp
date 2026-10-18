/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' help texts when no sound can play: the game's sound with a fake sound device, or the game's sound with
// no device, and help::DialogueText with the game's clock and texts faked by the test. Without a device, no text has a
// voice, so each text waits for its reading time and the help never stops on a voice that does not play.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManager.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/AudioPlayerInterface.h"
#include "Audio/SilentPlayback.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::help;

namespace
{
/// A sound device that plays nothing. The test can lose it, as the machine can lose its sound device.
class FakeSoundDevice final: public AudioPlayerInterface
{
public:
	FakeSoundDevice() = default;
	explicit FakeSoundDevice(std::shared_ptr<bool> connected)
	    : _connected(std::move(connected))
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
	void StopSource(SourceId) const override {}
	void SetVolume(SourceId, float) override {}
	void SetPitch(SourceId, float) override {}
	void SetPosition(SourceId, glm::vec3) override {}
	void SetDistanceAttenuation(SourceId, float, float, float) override {}
	void SetLooping(SourceId, bool) override {}
	void StartSource(SourceId) override {}
	[[nodiscard]] float GetVolume() const override { return 1.0f; }
	[[nodiscard]] AudioStatus GetStatus(SourceId) const override { return AudioStatus::Stopped; }
	[[nodiscard]] float GetProgress(size_t, SourceId) const override { return 0.0f; }
	std::vector<BufferId> UnqueueProcessedBuffers(SourceId) override { return {}; }
	[[nodiscard]] uint32_t GetSampleOffset(SourceId) const override { return 0; }
	[[nodiscard]] bool IsDeviceConnected() const override { return *_connected; }

private:
	std::shared_ptr<bool> _connected = std::make_shared<bool>(true);
	BufferId _nextBuffer {1};
	SourceId _nextSource {1};
};

/// The game's sound: with the fake device, or with no device, as the game starts when it cannot open one
std::unique_ptr<AudioManagerInterface> MakeSound(bool withDevice)
{
	if (withDevice)
	{
		return std::make_unique<AudioManager>(std::make_unique<FakeSoundDevice>());
	}
	return std::make_unique<AudioManagerNoOp>();
}

/// The temple's room help: advisors' lines that each have a sample in the advisors' bank
struct Fixture
{
	explicit Fixture(bool withDevice)
	    : audio(MakeSound(withDevice))
	{
		texts[4364] = {.narrator = k_NarratorGoodAdvisor, .text = u"one two three"};
		texts[4365] = {.narrator = k_NarratorEvilAdvisor, .text = u"four five"};
	}

	std::unique_ptr<AudioManagerInterface> audio;
	std::map<uint32_t, DialogueText::Text> texts;
	int32_t nowMs {10000};
	/// The advisors talk only while the device plays their line. A fake device plays nothing.
	bool advisorsTalking {false};
	std::vector<uint32_t> advisorSaid;

	[[nodiscard]] DialogueText Make()
	{
		DialogueText::Queries queries;
		queries.text = [this](uint32_t number) {
			const auto it = texts.find(number);
			return it == texts.end() ? DialogueText::Text {} : it->second;
		};
		queries.voice = [this](uint32_t number) { return VoiceFor(texts.contains(number), true, audio->IsInstalled()); };
		queries.turn = []() { return 100u; };
		queries.nowMs = [this]() { return nowMs; };
		queries.inTemple = []() { return true; };
		queries.advisorsTalking = [this]() { return advisorsTalking; };
		queries.narrationSounding = [](uint32_t) { return false; };
		queries.scriptWideScreen = []() { return true; };
		DialogueText::Hooks hooks;
		hooks.advisorSays = [this](int32_t, uint32_t number) { advisorSaid.push_back(number); };
		hooks.narrate = [](uint32_t) {};
		hooks.interruptAdvisors = []() {};
		hooks.stopNarration = []() {};
		return {480, false, {}, std::move(queries), std::move(hooks)};
	}
};
/// The temple's room help said by the advisors' voices, with the fake device that the test loses. The advisors' line
/// plays as the help text system plays it: as the device tells, or by its sample's length once the device is lost.
struct LostDeviceFixture
{
	/// The sample of each line is 4 seconds long, longer than the reading time of its words
	static constexpr float k_LineMs = 4000.0f;

	std::shared_ptr<bool> connected = std::make_shared<bool>(true);
	AudioManager audio {std::make_unique<FakeSoundDevice>(connected)};
	uint32_t nowMs {10000};
	SilentPlayback playback;
	std::vector<uint32_t> started;
	AdvisorVoices voices {MakeAudio()};

	[[nodiscard]] AdvisorVoices::Audio MakeAudio()
	{
		AdvisorVoices::Audio sound;
		sound.lineCount = []() { return 10000u; };
		sound.start = [this](uint32_t line) {
			started.push_back(line);
			playback = {.startMs = nowMs, .lengthMs = k_LineMs};
			// The fake device plays nothing, so only a lost device lets the line start
			return audio.IsDeviceLost();
		};
		sound.isPlaying = [this](uint32_t) { return PlaysOn(false, audio.IsDeviceLost(), playback, nowMs); };
		sound.tickMs = [this]() { return nowMs; };
		return sound;
	}

	[[nodiscard]] DialogueText Make()
	{
		DialogueText::Queries queries;
		queries.text = [](uint32_t number) {
			return DialogueText::Text {.narrator = number == 4364 ? k_NarratorGoodAdvisor : k_NarratorEvilAdvisor,
			                           .text = u"one two three"};
		};
		queries.voice = [this](uint32_t) { return VoiceFor(true, true, audio.IsInstalled()); };
		queries.turn = []() { return 100u; };
		queries.nowMs = [this]() { return static_cast<int32_t>(nowMs); };
		queries.inTemple = []() { return true; };
		queries.advisorsTalking = [this]() { return voices.AnyTalking(); };
		queries.narrationSounding = [](uint32_t) { return false; };
		queries.scriptWideScreen = []() { return true; };
		DialogueText::Hooks hooks;
		hooks.advisorSays = [this](int32_t narrator, uint32_t number) {
			voices.Say(narrator == k_NarratorGoodAdvisor ? 0 : 1, number, false, 0.0f);
		};
		hooks.narrate = [](uint32_t) {};
		hooks.interruptAdvisors = []() {};
		hooks.stopNarration = []() {};
		return {480, false, {}, std::move(queries), std::move(hooks)};
	}
};
} // namespace

TEST(HelpWithoutSound, ASilentLinePlaysForTheLengthOfItsSample)
{
	const SilentPlayback playback {.startMs = 1000, .lengthMs = 2500.0f};
	EXPECT_TRUE(playback.IsPlaying(1000));
	EXPECT_TRUE(playback.IsPlaying(3499));
	EXPECT_FALSE(playback.IsPlaying(3500));
	EXPECT_FLOAT_EQ(playback.Progress(2250), 0.5f);
	EXPECT_FLOAT_EQ(playback.Progress(9000), 1.0f);
	// The clock may wrap
	const SilentPlayback wrapped {.startMs = 0xFFFFFF00u, .lengthMs = 1000.0f};
	EXPECT_TRUE(wrapped.IsPlaying(0x100u));
	// The device tells while it works; the sample's length tells once it is lost
	EXPECT_TRUE(PlaysOn(true, false, playback, 9000));
	EXPECT_FALSE(PlaysOn(false, false, playback, 2000));
	EXPECT_TRUE(PlaysOn(false, true, playback, 2000));
	EXPECT_FALSE(PlaysOn(true, true, playback, 9000));
}

TEST(HelpWithoutSound, ALostDeviceLeavesTheSoundInstalled)
{
	LostDeviceFixture f;
	EXPECT_FALSE(f.audio.IsDeviceLost());
	*f.connected = false;
	EXPECT_TRUE(f.audio.IsDeviceLost());
	EXPECT_TRUE(f.audio.IsInstalled());
}

TEST(HelpWithoutSound, WithTheDeviceLostEachLineKeepsTheLengthOfItsSample)
{
	LostDeviceFixture f;
	*f.connected = false;
	auto dialogue = f.Make();
	dialogue.RunText(true, 4364, 0);
	f.voices.Update();
	EXPECT_EQ(f.started, std::vector<uint32_t> {4364});
	// Past its reading time (2.3 s) the line still plays
	f.nowMs = 10000 + 3000;
	EXPECT_FALSE(dialogue.IsTextRead());
	// The sample ends at 4 s; the advisor counts as talking for 200 ms more after the last frame it talked
	f.nowMs = 10000 + 3999;
	f.voices.Update();
	f.nowMs = 10000 + 4100;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 10000 + 4201;
	EXPECT_TRUE(dialogue.IsTextRead());
	// The next line has its own sample length
	dialogue.RunText(true, 4365, 0);
	f.voices.Update();
	f.nowMs += 3999;
	f.voices.Update();
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs += 202;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(HelpWithoutSound, ALineKeepsTheLengthOfItsSampleWhenTheDeviceIsLostWhileItIsSaid)
{
	LostDeviceFixture f;
	auto dialogue = f.Make();
	// The fake device plays nothing, so the advisors' line counts as played at once while the device works
	dialogue.RunText(true, 4364, 0);
	f.voices.Update();
	// The line starts again on a lost device: the game continues as if the device is there
	*f.connected = false;
	dialogue.RunText(true, 4364, 0);
	f.voices.Update();
	f.nowMs = 10000 + 2000;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 10000 + 4201;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(HelpWithoutSound, ATextHasAVoiceOnlyWithItsSampleAndAWorkingDevice)
{
	EXPECT_TRUE(VoiceFor(true, true, true).has_value());
	EXPECT_TRUE(VoiceFor(true, true, true)->advisorsBank);
	EXPECT_FALSE(VoiceFor(true, false, true)->advisorsBank);
	EXPECT_FALSE(VoiceFor(false, true, true).has_value());
	EXPECT_FALSE(VoiceFor(true, true, false).has_value());
}

TEST(HelpWithoutSound, TheSoundIsInstalledOnlyWithADevice)
{
	EXPECT_TRUE(Fixture(true).audio->IsInstalled());
	EXPECT_FALSE(Fixture(false).audio->IsInstalled());
}

TEST(HelpWithoutSound, WithAWorkingDeviceAnAdvisorsLineIsReadWhenTheAdvisorsStopTalking)
{
	Fixture f(true);
	auto dialogue = f.Make();
	dialogue.RunText(true, 4364, 0);
	EXPECT_EQ(f.advisorSaid, std::vector<uint32_t> {4364});
	f.advisorsTalking = true;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.advisorsTalking = false;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(HelpWithoutSound, WithNoDeviceEachLineWaitsForItsReadingTime)
{
	Fixture f(false);
	auto dialogue = f.Make();
	dialogue.RunText(true, 4364, 0);
	// No advisor says it, and the silent advisors do not end it at once
	EXPECT_TRUE(f.advisorSaid.empty());
	EXPECT_FALSE(dialogue.IsTextRead());
	// Three words: 23 turns of 100 ms, timed in real milliseconds in the temple
	f.nowMs = 12300;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 12301;
	EXPECT_TRUE(dialogue.IsTextRead());
	// The next line has its own reading time: two words, 18 turns
	dialogue.RunText(true, 4365, 0);
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 12301 + 1801;
	EXPECT_TRUE(dialogue.IsTextRead());
}
