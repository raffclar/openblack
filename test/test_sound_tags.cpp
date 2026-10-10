/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <map>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/Loaders.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "Resources/Resources.h"

using namespace openblack;

namespace
{
constexpr entt::id_type k_Sound = 52;
constexpr float k_Reach = 100.0f;
/// A sound heard in the temple too, by its bank's word
constexpr entt::id_type k_TempleSound = 54;

/// The camera inside the temple, where only the sounds the bank keeps for it are heard
audio::SoundEffectConditions InsideTemple()
{
	return {.insideTemple = true};
}

/// Keeps the emitters asked for and whether each was played, and plays nothing
class FakeAudio final: public audio::AudioManagerInterface
{
public:
	struct Emitter
	{
		entt::id_type sound;
		glm::vec3 position;
		bool played {false};
		bool stopped {false};
	};
	std::map<entt::entity, Emitter> emitters;

	void Stop() override {}
	void Update() override {}
	audio::BufferId CreateBuffer(audio::ChannelLayout, const std::vector<int16_t>&, int) override { return {}; }
	void CreateBuffer(audio::Sound&) override {}
	void PlayEmitter(entt::entity emitter) override { emitters.at(emitter).played = true; }
	void PauseEmitter(entt::entity) override {}
	void StopEmitter(entt::entity emitter) override { emitters.at(emitter).stopped = true; }
	void DestroyEmitter(entt::entity emitter) override { emitters.erase(emitter); }
	entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, audio::PlayType) override
	{
		const auto entity = static_cast<entt::entity>(emitters.size() + 1);
		emitters.emplace(entity, Emitter {.sound = id, .position = worldPosition.value_or(glm::vec3(0.0f))});
		return entity;
	}
	bool EmitterExists(entt::entity emitter) override { return emitters.contains(emitter); }
	[[nodiscard]] float GetProgress(entt::entity) override { return 0.0f; }
	[[nodiscard]] audio::AudioStatus GetStatus(entt::entity emitter) override
	{
		const auto& played = emitters.at(emitter);
		return played.played && !played.stopped ? audio::AudioStatus::Playing : audio::AudioStatus::Stopped;
	}
	void SetGlobalVolume(float) override {}
	void SetSfxVolume(float) override {}
	void SetMusicVolume(float) override {}
	[[nodiscard]] float GetGlobalVolume() override { return 1.0f; }
	[[nodiscard]] float GetSfxVolume() override { return 1.0f; }
	[[nodiscard]] float GetMusicVolume() override { return 1.0f; }
	void PlayMusic(const std::string&, audio::PlayType) override {}
	void StopMusic() override {}
	bool MusicPlay(const std::string&, const audio::MusicPlayOptions&) override { return false; }
	void MusicStop(bool) override {}
	[[nodiscard]] bool MusicIsActive() const override { return false; }
	[[nodiscard]] std::optional<audio::MusicBankInfo> GetMusicBankInfo(const std::string&) override { return std::nullopt; }
	[[nodiscard]] const audio::MusicPlayer* GetMusic() const override { return nullptr; }
	void PlaySound(entt::id_type, audio::PlayType) override {}
	void PlaySoundEffect(entt::id_type, std::optional<glm::vec3>) override {}
	void StopSoundEffect(entt::id_type) override {}
	void StopAllSoundEffects() override {}
	entt::entity StartSoundEffect(entt::id_type, const audio::SoundEffectOptions&) override { return entt::null; }
	void SetEmitterPosition(entt::entity, const glm::vec3&) override {}
	void ReleaseEmitterLoop(entt::entity) override {}
	[[nodiscard]] bool IsEmitterLooping(entt::entity) override { return false; }
	void SetEmitterVolume(entt::entity, uint32_t) override {}
	void SetEmitterPitch(entt::entity, uint32_t) override {}
	[[nodiscard]] uint32_t GetEmitterVolume(entt::entity) override { return 0; }
	void StopOwnedSounds(entt::entity) override {}
	void AddAnimEffects(const std::string&, audio::AnimEffectTable) override {}
	audio::AnimEffectPlay PlayAnimEffect(const std::string&, std::span<const int32_t>, entt::entity, const glm::vec3&) override
	{
		return {};
	}
	const audio::Sound& GetSound(entt::id_type) override { return _sound; }
	void CreateSoundGroup(const std::string&) override {}
	void AddToSoundGroup(const std::string&, entt::id_type) override {}
	const audio::SoundGroup& GetSoundGroup(const std::string&) override { return _group; }
	const std::map<std::string, audio::SoundGroup>& GetSoundGroups() override { return _groups; }
	void AddMusicEntry(const std::string&) override {}
	[[nodiscard]] const std::vector<std::string>& GetMusicTracks() const override { return _tracks; }
	uint32_t AtmosRegisterBank(const std::string&, const std::vector<pack::AudioBankSampleHeader>&, uint16_t) override
	{
		return 0;
	}
	void AtmosReleaseBank(uint32_t) override {}
	void AtmosSetBankVolume(uint32_t, int32_t) override {}
	void AtmosSetGroup(uint32_t, uint32_t) override {}
	void AtmosProcess(bool) override {}
	[[nodiscard]] const audio::AtmosPlayer* GetAtmos() const override { return nullptr; }

private:
	audio::Sound _sound;
	audio::SoundGroup _group;
	std::map<std::string, audio::SoundGroup> _groups;
	std::vector<std::string> _tracks;
};

class SoundTags: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<FakeAudio>();
		Locator::resources::emplace<resources::Resources>();
		Locator::camera::emplace(glm::vec3(0.0f));
		Locator::camera::value().SetOrigin(glm::vec3(0.0f));
		pack::AudioBankSampleHeader header {};
		header.id = static_cast<int32_t>(k_Sound);
		header.maxDist = k_Reach;
		Locator::resources::value().GetSounds().Load(k_Sound, resources::SoundLoader::FromBufferTag {}, header,
		                                             std::vector<std::vector<uint8_t>> {});
		header.id = static_cast<int32_t>(k_TempleSound);
		header.userParam = static_cast<uint16_t>(audio::SoundEffectUse::HeardInTemple);
		Locator::resources::value().GetSounds().Load(k_TempleSound, resources::SoundLoader::FromBufferTag {}, header,
		                                             std::vector<std::vector<uint8_t>> {});
	}
	void TearDown() override
	{
		Locator::camera::reset();
		Locator::resources::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	static FakeAudio& Audio() { return static_cast<FakeAudio&>(Locator::audio::value()); }

	ecs::systems::SoundTagSystem _tags;
};
} // namespace

TEST_F(SoundTags, APointSoundNotDelayedPlaysAtOnce)
{
	const auto point = _tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, false);
	ASSERT_EQ(Audio().emitters.size(), 1u);
	const auto& [emitter, played] = *Audio().emitters.begin();
	EXPECT_TRUE(played.played);
	EXPECT_EQ(played.sound, k_Sound);
	EXPECT_EQ(played.position, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<ecs::components::SoundTag>(point).emitter, emitter);
}

TEST_F(SoundTags, APointSoundGoesOnceItHasPlayed)
{
	const auto point = _tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, false);
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_TRUE(Locator::entitiesRegistry::value().Valid(point));

	Audio().emitters.begin()->second.stopped = true;
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(point));
	EXPECT_TRUE(Audio().emitters.empty());
}

TEST_F(SoundTags, APointSoundBeyondItsReachIsNotHeard)
{
	const auto point = _tags.CreatePointSound(k_Sound, {k_Reach + 1.0f, 0.0f, 0.0f}, false);
	EXPECT_TRUE(Audio().emitters.empty());
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(point));
}

TEST_F(SoundTags, ADelayedPointSoundWaitsToReachTheCamera)
{
	// About 35 units a game turn: 50 away it is heard on the second turn
	_tags.CreatePointSound(k_Sound, {50.0f, 0.0f, 0.0f}, true);
	EXPECT_TRUE(Audio().emitters.empty());
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_TRUE(Audio().emitters.empty());
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);
	EXPECT_TRUE(Audio().emitters.begin()->second.played);
}

TEST_F(SoundTags, InsideTheTempleOnlyTheTemplesSoundsStart)
{
	ecs::systems::SoundTagSystem tags(&InsideTemple);

	// Not heard: it doesn't start, and its point goes on the next turn
	const auto quiet = tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, false);
	EXPECT_TRUE(Audio().emitters.empty());
	tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(quiet));

	// A delayed one is asked about when it reaches the camera
	tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, true);
	tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_TRUE(Audio().emitters.empty());

	tags.CreatePointSound(k_TempleSound, {10.0f, 0.0f, 0.0f}, false);
	ASSERT_EQ(Audio().emitters.size(), 1u);
	EXPECT_EQ(Audio().emitters.begin()->second.sound, k_TempleSound);
}
