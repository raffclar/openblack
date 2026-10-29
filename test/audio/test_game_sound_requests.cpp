/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameSoundRequests.h"
#include "Locator.h"
#include "Sound/GameSoundEffects.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
constexpr auto k_Emitter = static_cast<entt::entity>(7);
constexpr entt::id_type k_Sound = 42;

/// An audio manager that keeps a record of the requests that reach it
class RecordingAudio final: public AudioManagerNoOp
{
public:
	std::vector<std::string> calls;

	entt::entity StartSoundEffect(entt::id_type id, const SoundEffectOptions& options) override
	{
		calls.push_back("start " + std::to_string(id) + (options.position.has_value() ? " placed" : ""));
		return k_Emitter;
	}
	void StopEmitter(entt::entity emitter) override { calls.push_back("stop " + std::to_string(entt::to_integral(emitter))); }
	void SetEmitterPitch(entt::entity emitter, uint32_t pitchPercent) override
	{
		calls.push_back("pitch " + std::to_string(entt::to_integral(emitter)) + " " + std::to_string(pitchPercent));
	}
	bool EmitterExists(entt::entity emitter) override { return emitter == k_Emitter; }
	AnimEffectPlay PlayAnimEffect(const std::string& bankName, std::span<const int32_t> keys, entt::entity /*owner*/,
	                              const glm::vec3& /*position*/, const std::optional<SoundEffectConditions>& heardUnder,
	                              AnimEffectMeasure /*measure*/) override
	{
		calls.push_back("anim " + bankName + " " + std::to_string(keys.size()) + (heardUnder.has_value() ? " heard" : ""));
		return {.outcome = AnimEffectPlay::Outcome::Played, .emitter = k_Emitter, .sample = 3};
	}
};

class GameSoundRequestsTest: public ::testing::Test
{
protected:
	void TearDown() override
	{
		Locator::sounds::reset();
		Locator::audio::reset();
	}
};
} // namespace

// With no audio manager, the requests play nothing and get the answers of a computer without sound
TEST_F(GameSoundRequestsTest, WithNoAudioManagerNothingPlays)
{
	GameSoundRequests requests;
	EXPECT_FALSE(requests.IsInstalled());
	EXPECT_TRUE(requests.StartSoundEffect(k_Sound, {}) == entt::null);
	EXPECT_FALSE(requests.EmitterExists(k_Emitter));
	EXPECT_FLOAT_EQ(requests.GetProgress(k_Emitter), 1.0f);
	EXPECT_EQ(requests.PlayAnimEffect("bank", std::array {1, 2}, entt::null, glm::vec3(0.0f)).outcome,
	          AnimEffectPlay::Outcome::NoEffect);
}

// Each request goes to the audio manager at once, in the order of the requests
TEST_F(GameSoundRequestsTest, TheRequestsGoToTheAudioManagerInOrder)
{
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	GameSoundRequests requests;
	const auto emitter = requests.StartSoundEffect(k_Sound, {.position = glm::vec3(1.0f)});
	EXPECT_EQ(emitter, k_Emitter);
	EXPECT_TRUE(requests.EmitterExists(emitter));
	requests.SetEmitterPitch(emitter, 120);
	const auto played =
	    requests.PlayAnimEffect("bank", std::array {1, 2, 3}, entt::null, glm::vec3(0.0f), SoundEffectConditions {});
	EXPECT_EQ(played.outcome, AnimEffectPlay::Outcome::Played);
	EXPECT_EQ(played.sample, 3);
	requests.StopEmitter(emitter);
	EXPECT_EQ(audio.calls, (std::vector<std::string> {"start 42 placed", "pitch 7 120", "anim bank 3 heard", "stop 7"}));
}

// A game sound effect that is not loaded is not heard, so it does not reach the audio manager
TEST_F(GameSoundRequestsTest, AGameSoundEffectThatIsNotLoadedDoesNotPlay)
{
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	Locator::sounds::emplace<GameSoundRequests>();
	EXPECT_TRUE(StartGameSoundEffect(k_Sound, {}) == entt::null);
	PlayGameSoundEffect(k_Sound, std::nullopt);
	EXPECT_TRUE(audio.calls.empty());
}

// With no sound requests, the world's game sound effects do nothing
TEST_F(GameSoundRequestsTest, WithNoSoundRequestsTheWorldsSoundsDoNothing)
{
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	EXPECT_TRUE(StartGameSoundEffect(k_Sound, {}) == entt::null);
	EXPECT_FALSE(GameSoundEffectHeard({}, k_Sound));
	EXPECT_TRUE(audio.calls.empty());
}
