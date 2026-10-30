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
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "Audio/AudioManagerNoOp.h"
#include "Audio/GameSoundRequests.h"
#include "Audio/MusicPlayer.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Sound/GameSoundEffects.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
constexpr auto k_Emitter = static_cast<entt::entity>(7);
constexpr entt::id_type k_Sound = 42;
constexpr auto k_Placed = static_cast<entt::entity>(3);
constexpr auto k_NotPlaced = static_cast<entt::entity>(4);

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
	bool MusicPlay(const std::string& bankPath, const MusicPlayOptions& options) override
	{
		calls.push_back("music " + bankPath + " " + std::to_string(options.startChunk) + (options.fadeIn ? " fade" : ""));
		return true;
	}
	void MusicStop(bool fadeOut) override { calls.push_back(fadeOut ? "music stop fade" : "music stop"); }
	void MusicStopBank(const std::string& bankPath, bool /*fadeOut*/) override { calls.push_back("music stop " + bankPath); }
	void Stop() override { calls.push_back("stop all"); }
	void AtmosProcess(bool active) override { calls.push_back(active ? "atmosphere" : "atmosphere silent"); }
	[[nodiscard]] ecs::Registry* Emitters() override { return &emitters; }

	ecs::Registry emitters;
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

// With no audio manager, the music, the banks and the atmosphere ask for nothing and get empty answers
TEST_F(GameSoundRequestsTest, WithNoAudioManagerTheMusicAndTheBanksAreEmpty)
{
	GameSoundRequests requests;
	EXPECT_FALSE(requests.MusicPlay("bank.sad", {}));
	requests.MusicStop(true);
	requests.MusicStopBank("bank.sad", false);
	requests.StopAllSounds();
	requests.ProcessAtmosphere(false);
	requests.CreateSoundGroup("group");
	requests.AddToSoundGroup("group", k_Sound);
	EXPECT_FALSE(requests.GetMusicBankInfo("bank.sad").has_value());
	EXPECT_TRUE(requests.GetPlayingMusicChannels().empty());
	EXPECT_EQ(requests.GetCustomMusic(), nullptr);
	EXPECT_TRUE(requests.GetSoundGroups().empty());
	EXPECT_FLOAT_EQ(requests.GetMusicVolume(), 0.0f);
	requests.PlaceOwnedEmitters([](entt::entity /*owner*/) { return std::optional(glm::vec3(1.0f)); });
}

// The music, stop and atmosphere requests go to the audio manager at once, in the order of the requests
TEST_F(GameSoundRequestsTest, TheMusicRequestsGoToTheAudioManagerInOrder)
{
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	GameSoundRequests requests;
	EXPECT_TRUE(requests.MusicPlay("land.sad", {.startChunk = 4, .fadeIn = true}));
	requests.MusicStopBank("land.sad", false);
	requests.ProcessAtmosphere(false);
	requests.MusicStop(true);
	requests.StopAllSounds();
	EXPECT_EQ(audio.calls, (std::vector<std::string> {"music land.sad 4 fade", "music stop land.sad", "atmosphere silent",
	                                                  "music stop fade", "stop all"}));
}

// The music player gets each value of a music request, and no bank
TEST(MusicRequest, TheMusicPlayerGetsEachValue)
{
	bool finished = false;
	const auto options = ToMusicPlayOptions({
	    .volume = 90,
	    .startChunk = 3,
	    .startFrame = 11,
	    .loops = -1,
	    .sync = true,
	    .fadeIn = true,
	    .pitchPercent = 120,
	    .position = glm::vec3(1.0f, 2.0f, 3.0f),
	    .minDistance = 5.0f,
	    .maxDistance = 50.0f,
	    .distanceScale = 2.0f,
	    .onFinished = [&finished]() { finished = true; },
	});
	EXPECT_EQ(options.bank, nullptr);
	EXPECT_EQ(options.volume, 90);
	EXPECT_EQ(options.startChunk, 3u);
	EXPECT_EQ(options.startFrame, 11u);
	EXPECT_EQ(options.loops, -1);
	EXPECT_TRUE(options.sync);
	EXPECT_TRUE(options.fadeIn);
	EXPECT_EQ(options.pitchPercent, 120);
	EXPECT_EQ(options.position, std::optional(glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_FLOAT_EQ(options.minDistance, 5.0f);
	EXPECT_FLOAT_EQ(options.maxDistance, 50.0f);
	EXPECT_FLOAT_EQ(options.distanceScale, 2.0f);
	ASSERT_TRUE(options.onFinished);
	options.onFinished();
	EXPECT_TRUE(finished);
}

// An emitter with an owner goes to the place of its owner. An emitter with no owner, or with an owner that has no
// place, stays where it is.
TEST_F(GameSoundRequestsTest, TheOwnedEmittersGoToThePlaceOfTheirOwner)
{
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	const auto follows = audio.emitters.Create();
	audio.emitters.Assign<ecs::components::AudioEmitter>(follows).owner = k_Placed;
	const auto stays = audio.emitters.Create();
	audio.emitters.Assign<ecs::components::AudioEmitter>(stays).owner = k_NotPlaced;
	const auto ownerless = audio.emitters.Create();
	audio.emitters.Assign<ecs::components::AudioEmitter>(ownerless);
	std::vector<entt::entity> asked;
	GameSoundRequests requests;
	requests.PlaceOwnedEmitters([&asked](entt::entity owner) -> std::optional<glm::vec3> {
		asked.push_back(owner);
		return owner == k_Placed ? std::optional(glm::vec3(4.0f, 5.0f, 6.0f)) : std::nullopt;
	});
	EXPECT_EQ(asked.size(), 2u);
	EXPECT_EQ(audio.emitters.Get<ecs::components::AudioEmitter>(follows).position, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(audio.emitters.Get<ecs::components::AudioEmitter>(stays).position, glm::vec3(0.0f));
	EXPECT_EQ(audio.emitters.Get<ecs::components::AudioEmitter>(ownerless).position, glm::vec3(0.0f));
}

// The distance to the listener comes from the audio's own listener. With no audio manager, there is no distance.
TEST_F(GameSoundRequestsTest, TheListenerDistanceComesFromTheAudiosListener)
{
	GameSoundRequests requests;
	EXPECT_FALSE(requests.ListenerDistance(glm::vec3(1.0f)).has_value());
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	audio.SetListenerOverride(Listener {.origin = glm::vec3(1.0f, 2.0f, 3.0f)});
	const auto distance = requests.ListenerDistance(glm::vec3(4.0f, 6.0f, 3.0f));
	ASSERT_TRUE(distance.has_value());
	EXPECT_FLOAT_EQ(*distance, 5.0f);
	EXPECT_TRUE(audio.calls.empty());
}

// An owner of sounds is an entity of the audio. It goes when it is destroyed, and a second destroy does nothing.
TEST_F(GameSoundRequestsTest, AnOwnerOfSoundsIsAnEntityOfTheAudio)
{
	GameSoundRequests requests;
	EXPECT_TRUE(requests.CreateSoundOwner() == entt::null);
	requests.DestroySoundOwner(k_Placed);
	auto& audio = static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
	const auto owner = requests.CreateSoundOwner();
	ASSERT_TRUE(owner != entt::null);
	EXPECT_TRUE(audio.emitters.Valid(owner));
	requests.DestroySoundOwner(owner);
	EXPECT_FALSE(audio.emitters.Valid(owner));
	requests.DestroySoundOwner(owner);
	requests.DestroySoundOwner(entt::null);
	EXPECT_TRUE(audio.calls.empty());
}
