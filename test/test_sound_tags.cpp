/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
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
		bool released {false};
		audio::PlayType playType {audio::PlayType::Once};
		entt::entity owner {entt::null};
		audio::Follows follows {audio::Follows::Nothing};
	};
	std::map<entt::entity, Emitter> emitters;
	/// The number of times a system moved an emitter
	int moves {0};

	void Stop() override {}
	void Update() override {}
	void StopOutOfRangeEmitters() override {}
	audio::BufferId CreateBuffer(audio::ChannelLayout, const std::vector<int16_t>&, int) override { return {}; }
	void CreateBuffer(audio::Sound&) override {}
	void PlayEmitter(entt::entity emitter) override { emitters.at(emitter).played = true; }
	void PauseEmitter(entt::entity) override {}
	void StopEmitter(entt::entity emitter) override { emitters.at(emitter).stopped = true; }
	void DestroyEmitter(entt::entity emitter) override { emitters.erase(emitter); }
	entt::entity CreateEmitter(entt::id_type id, std::optional<glm::vec3> worldPosition, audio::PlayType playType,
	                           entt::entity owner) override
	{
		const auto entity = static_cast<entt::entity>(++_made);
		emitters.emplace(
		    entity,
		    Emitter {.sound = id, .position = worldPosition.value_or(glm::vec3(0.0f)), .playType = playType, .owner = owner});
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
	void MusicStopBank(const std::string&, bool) override {}
	[[nodiscard]] bool MusicIsActive() const override { return false; }
	[[nodiscard]] std::optional<audio::MusicBankInfo> GetMusicBankInfo(const std::string&) override { return std::nullopt; }
	[[nodiscard]] const audio::MusicPlayer* GetMusic() const override { return nullptr; }
	void PlaySound(entt::id_type, audio::PlayType) override {}
	void PlaySoundEffect(entt::id_type, std::optional<glm::vec3>) override {}
	void StopSoundEffect(entt::id_type) override {}
	[[nodiscard]] bool IsSoundEffectPlaying(entt::id_type) const override { return false; }
	[[nodiscard]] bool IsInstalled() const override { return true; }
	void StopAllSoundEffects() override {}
	entt::entity StartSoundEffect(entt::id_type, const audio::SoundEffectOptions&) override { return entt::null; }
	void SetEmitterPosition(entt::entity emitter, const glm::vec3& position) override
	{
		emitters.at(emitter).position = position;
		++moves;
	}
	void SetEmitterFollows(entt::entity emitter, audio::Follows follows) override { emitters.at(emitter).follows = follows; }
	void ReleaseEmitterLoop(entt::entity emitter) override { emitters.at(emitter).released = true; }
	[[nodiscard]] bool IsEmitterLooping(entt::entity) override { return false; }
	void SetEmitterVolume(entt::entity, uint32_t) override {}
	void SetEmitterPitch(entt::entity, uint32_t) override {}
	[[nodiscard]] uint32_t GetEmitterVolume(entt::entity) override { return 0; }
	void StopOwnedSounds(entt::entity) override {}
	void AddAnimEffects(const std::string&, audio::AnimEffectTable) override {}
	using audio::AudioManagerInterface::PlayAnimEffect;
	audio::AnimEffectPlay PlayAnimEffect(const std::string&, std::span<const int32_t>, entt::entity, const glm::vec3&,
	                                     const std::optional<audio::SoundEffectConditions>&, audio::AnimEffectMeasure) override
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
	[[nodiscard]] audio::Listener GetListener() const override { return {}; }
	void SetListenerOverride(std::optional<audio::Listener>) override {}

private:
	uint32_t _made {0};
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

TEST_F(SoundTags, ADelayedPointSoundAtItsReachNeverStarts)
{
	// Arrived at the camera, a delayed sound starts only nearer than its reach
	const auto point = _tags.CreatePointSound(k_Sound, {k_Reach, 0.0f, 0.0f}, true);
	for (int turn = 0; turn < 20; ++turn)
	{
		_tags.ProcessTurn(glm::vec3(0.0f));
	}
	EXPECT_TRUE(Audio().emitters.empty());
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(point));
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

namespace
{
entt::entity CreateObjectAt(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}
} // namespace

namespace
{
/// The emitters that play a sound
std::vector<FakeAudio::Emitter> PlayingOf(const FakeAudio& audio, entt::id_type sound)
{
	std::vector<FakeAudio::Emitter> playing;
	for (const auto& [entity, emitter] : audio.emitters)
	{
		if (emitter.sound == sound && emitter.played && !emitter.stopped)
		{
			playing.push_back(emitter);
		}
	}
	return playing;
}
} // namespace

// The phone box's ring: a script attaches a sound to an object, heard from it while the camera is in range. Each start
// plays it one time, and the next turn starts it again when it is finished.
TEST_F(SoundTags, AScriptsSoundOnAnObjectPlaysFromIt)
{
	const auto box = CreateObjectAt({20.0f, 0.0f, 0.0f});
	const auto tag = _tags.AttachObjectSound(box, k_Sound, false);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<ecs::components::SoundTag>(tag).owner, box);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);
	auto& played = Audio().emitters.begin()->second;
	EXPECT_TRUE(played.played);
	EXPECT_EQ(played.sound, k_Sound);
	EXPECT_EQ(played.position, glm::vec3(20.0f, 0.0f, 0.0f));
	EXPECT_EQ(played.playType, audio::PlayType::Once);
	// The tag owns its sound, not the object
	EXPECT_EQ(played.owner, tag);
	// One start is enough while it plays
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(Audio().emitters.size(), 1u);
	// When it is finished, the next turn starts it again
	played.stopped = true;
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(PlayingOf(Audio(), k_Sound).size(), 1u);
}

// A creature with a script's sound starts its creed hum: both play, each from its own tag
TEST_F(SoundTags, AnObjectKeepsUpManySounds)
{
	const auto creature = CreateObjectAt({20.0f, 0.0f, 0.0f});
	const auto script = _tags.AttachObjectSound(creature, k_Sound, false);
	const auto hum = _tags.AttachObjectSound(creature, k_TempleSound, true);
	EXPECT_NE(script, hum);
	_tags.ProcessTurn(glm::vec3(0.0f));
	const auto scriptSounds = PlayingOf(Audio(), k_Sound);
	const auto humSounds = PlayingOf(Audio(), k_TempleSound);
	ASSERT_EQ(scriptSounds.size(), 1u);
	ASSERT_EQ(humSounds.size(), 1u);
	EXPECT_EQ(scriptSounds[0].owner, script);
	EXPECT_EQ(humSounds[0].owner, hum);
	EXPECT_EQ(humSounds[0].playType, audio::PlayType::Repeat);
}

// The same sound attached two times is two tags, and taking the sound off removes both
TEST_F(SoundTags, TheSameSoundTwiceIsTwoTags)
{
	const auto box = CreateObjectAt({20.0f, 0.0f, 0.0f});
	_tags.AttachObjectSound(box, k_Sound, true);
	_tags.AttachObjectSound(box, k_Sound, true);
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(PlayingOf(Audio(), k_Sound).size(), 2u);

	_tags.DetachObjectSound(box, k_Sound);
	size_t tags = 0;
	Locator::entitiesRegistry::value().Each<const ecs::components::SoundTag>(
	    [&tags](entt::entity, const ecs::components::SoundTag&) { ++tags; });
	EXPECT_EQ(tags, 0u);
	EXPECT_TRUE(std::ranges::all_of(Audio().emitters, [](const auto& entry) { return entry.second.released; }));
}

// Taken off, a looping sound finishes the loop it is in and does not start again. The other sounds stay.
TEST_F(SoundTags, ASoundTakenOffAnObjectPlaysOutItsPass)
{
	const auto creature = CreateObjectAt({20.0f, 0.0f, 0.0f});
	_tags.AttachObjectSound(creature, k_Sound, false);
	_tags.AttachObjectSound(creature, k_TempleSound, true);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 2u);

	_tags.DetachObjectSound(creature, k_TempleSound);
	ASSERT_EQ(Audio().emitters.size(), 2u);
	for (const auto& [entity, emitter] : Audio().emitters)
	{
		// The hum is released and plays on; the script's sound is not touched
		EXPECT_EQ(emitter.released, emitter.sound == k_TempleSound);
		EXPECT_FALSE(emitter.stopped);
	}

	// Its tag is gone, so it does not start again; the script's sound does
	for (auto& [entity, emitter] : Audio().emitters)
	{
		emitter.stopped = true;
	}
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_TRUE(PlayingOf(Audio(), k_TempleSound).empty());
	EXPECT_EQ(PlayingOf(Audio(), k_Sound).size(), 1u);

	// A sound that plays one time is not released: it plays to its end
	_tags.DetachObjectSound(creature, k_Sound);
	const auto script = PlayingOf(Audio(), k_Sound);
	ASSERT_EQ(script.size(), 1u);
	EXPECT_FALSE(script[0].released);
}

// Taking a sound off an object also removes the object's own tag of that sound
TEST_F(SoundTags, AnObjectsOwnTagGoesWithItsSound)
{
	const auto lantern = CreateObjectAt({20.0f, 0.0f, 0.0f});
	Locator::entitiesRegistry::value().Assign<ecs::components::SoundTag>(lantern, k_Sound, glm::vec3(0.0f), true);
	_tags.DetachObjectSound(lantern, k_TempleSound);
	EXPECT_TRUE(Locator::entitiesRegistry::value().AllOf<ecs::components::SoundTag>(lantern));
	_tags.DetachObjectSound(lantern, k_Sound);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<ecs::components::SoundTag>(lantern));
}

// When the object is gone, its attached tags go, and a looping sound finishes its loop
TEST_F(SoundTags, TheTagsOfAGoneObjectGo)
{
	const auto creature = CreateObjectAt({20.0f, 0.0f, 0.0f});
	const auto hum = _tags.AttachObjectSound(creature, k_Sound, true);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);

	Locator::entitiesRegistry::value().Destroy(creature);
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_FALSE(Locator::entitiesRegistry::value().Valid(hum));
	ASSERT_EQ(Audio().emitters.size(), 1u);
	EXPECT_TRUE(Audio().emitters.begin()->second.released);
}

// The creed hum on a creature that walks: each game turn its sound moves to where the creature is now
TEST_F(SoundTags, AnObjectsSoundFollowsItEachTurn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = CreateObjectAt({20.0f, 0.0f, 0.0f});
	_tags.AttachObjectSound(creature, k_Sound, true);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);
	EXPECT_EQ(Audio().moves, 0);

	registry.Get<ecs::components::Transform>(creature).position = {30.0f, 2.0f, -5.0f};
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);
	const auto& played = Audio().emitters.begin()->second;
	EXPECT_EQ(played.position, glm::vec3(30.0f, 2.0f, -5.0f));
	EXPECT_EQ(Audio().moves, 1);
	// It moves the sound that plays, and does not start a new one
	EXPECT_FALSE(played.stopped);
	// The audio turn does not move it again, but it stops the sound when the camera goes too far
	EXPECT_EQ(played.follows, audio::Follows::Place);
}

// A point's sound stays at its point, and the audio turn stops it when the camera goes too far
TEST_F(SoundTags, APointSoundStopsWhenTheCameraGoesTooFar)
{
	_tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, false);
	ASSERT_EQ(Audio().emitters.size(), 1u);
	EXPECT_EQ(Audio().emitters.begin()->second.follows, audio::Follows::Place);
}

// A sound of an object in the hand comes from the hand, not from where the object hangs below it
TEST_F(SoundTags, AnObjectInTheHandSoundsFromTheHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto box = CreateObjectAt({20.0f, 0.0f, 0.0f});
	const auto hand = CreateObjectAt({40.0f, 10.0f, 0.0f});
	_tags.AttachObjectSound(box, k_Sound, true);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_EQ(Audio().emitters.size(), 1u);

	registry.Assign<ecs::components::InHand>(box, ecs::components::InHand {.hand = hand});
	registry.Get<ecs::components::Transform>(box).position = {40.0f, 8.0f, 0.0f};
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(Audio().emitters.begin()->second.position, glm::vec3(40.0f, 10.0f, 0.0f));

	// Let go, it sounds from its own place again
	registry.Remove<ecs::components::InHand>(box);
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(Audio().emitters.begin()->second.position, glm::vec3(40.0f, 8.0f, 0.0f));
}

// A tag's offset stays above the object as the object moves
TEST_F(SoundTags, AMovedSoundKeepsItsOffset)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto box = CreateObjectAt({20.0f, 0.0f, 0.0f});
	const auto tag = _tags.AttachObjectSound(box, k_Sound, true);
	registry.Get<ecs::components::SoundTag>(tag).offset = {0.0f, 3.0f, 0.0f};
	_tags.ProcessTurn(glm::vec3(0.0f));
	registry.Get<ecs::components::Transform>(box).position = {25.0f, 1.0f, 0.0f};
	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(Audio().emitters.begin()->second.position, glm::vec3(25.0f, 4.0f, 0.0f));
}

// A saved game loads: the sounds of points and the sounds attached to objects do not come back, and an object's own
// sound starts again on the next turn
TEST_F(SoundTags, AfterALoadOnlyTheObjectsOwnSoundsComeBack)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto lantern = CreateObjectAt({20.0f, 0.0f, 0.0f});
	registry.Assign<ecs::components::SoundTag>(lantern, ecs::components::SoundTag {
	                                                        .sound = k_Sound,
	                                                        .offset = glm::vec3(0.0f),
	                                                        .active = true,
	                                                    });
	const auto box = CreateObjectAt({30.0f, 0.0f, 0.0f});
	const auto attached = _tags.AttachObjectSound(box, k_Sound, false);
	const auto point = _tags.CreatePointSound(k_Sound, {10.0f, 0.0f, 0.0f}, false);
	_tags.ProcessTurn(glm::vec3(0.0f));
	ASSERT_TRUE(registry.Get<ecs::components::SoundTag>(lantern).emitter != entt::null);
	const auto before = Audio().emitters.size();

	_tags.ForgetSavedSounds();
	EXPECT_FALSE(registry.Valid(attached));
	EXPECT_FALSE(registry.Valid(point));
	EXPECT_TRUE(registry.Valid(box));
	EXPECT_TRUE(registry.Get<ecs::components::SoundTag>(lantern).emitter == entt::null);

	_tags.ProcessTurn(glm::vec3(0.0f));
	EXPECT_EQ(Audio().emitters.size(), before + 1);
	EXPECT_TRUE(registry.Get<ecs::components::SoundTag>(lantern).emitter != entt::null);
}
