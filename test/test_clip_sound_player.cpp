/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A living thing's clip sounds, played in a world of their own: a villager's sounds by its size from where it stands,
// its home's banter from its home, nothing once it is dead or once a clip played once has finished, and the ordinary
// ones not inside the temple.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <SASFile.h>
#include <gtest/gtest.h>

#include "Audio/AnimEffectKeys.h"
#include "Audio/ClipSounds.h"
#include "ECS/ClipSoundPlayer.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace clip_sounds = openblack::audio::clip_sounds;

namespace
{
constexpr AnimId k_Walk = static_cast<AnimId>(7);
constexpr AnimId k_Thrown = static_cast<AnimId>(clip_sounds::k_ThrownClip);
constexpr int32_t k_Step = 5;
constexpr int32_t k_Sigh = 9;
constexpr int32_t k_Grass = static_cast<int32_t>(audio::SoundSurface::Grass);

class FakeWorld final: public clip_sound_player::World
{
public:
	struct Played
	{
		std::string bank;
		std::vector<int32_t> keys;
		entt::entity owner;
		bool bySampleRules;
	};

	[[nodiscard]] const Registry& Entities() const override { return registry; }
	[[nodiscard]] const sas::ClipSounds* SoundsOf(AnimId clip) const override
	{
		const auto found = clips.find(clip);
		return found != clips.end() ? &found->second : nullptr;
	}
	[[nodiscard]] std::optional<creature_audio::Ground> GroundAt(const glm::vec3& /*position*/) const override
	{
		return creature_audio::Ground {.water = false, .materialSurface = k_Grass};
	}
	[[nodiscard]] bool InsideTemple() const override { return insideTemple; }
	[[nodiscard]] float LifeOf(entt::entity /*object*/) const override { return life; }
	void PlaySound(std::string_view bank, std::span<const int32_t> keys, entt::entity owner, const glm::vec3& /*position*/,
	               bool bySampleRules) override
	{
		played.push_back(
		    {.bank = std::string(bank), .keys = {keys.begin(), keys.end()}, .owner = owner, .bySampleRules = bySampleRules});
	}

	/// A woman with a home, standing still
	entt::entity MakeVillager()
	{
		const auto villager = registry.Create();
		auto& person = registry.Assign<Villager>(villager);
		person.lifeStage = Villager::LifeStage::Adult;
		person.sex = Villager::Sex::FEMALE;
		home = registry.Create();
		person.abode = home;
		registry.Assign<LivingAction>(villager, VillagerStates::MoveToPos, static_cast<uint16_t>(0));
		return villager;
	}

	Registry registry;
	std::map<AnimId, sas::ClipSounds> clips;
	bool insideTemple {false};
	float life {1.0f};
	entt::entity home {entt::null};
	std::vector<Played> played;
};

struct Fixture
{
	Fixture()
	{
		// A step played the ordinary way, the home's banter, then a sigh
		world.clips[k_Walk] = {.clip = "walk",
		                       .soundType = clip_sounds::k_PeopleSounds,
		                       .sounds = {{.time = 100, .action = k_Step, .mode = 0},
		                                  {.time = 200, .action = clip_sounds::k_HomeBanter, .mode = 1},
		                                  {.time = 300, .action = k_Sigh, .mode = 0}}};
		world.clips[k_Thrown] = {
		    .clip = "thrown", .soundType = clip_sounds::k_PeopleSounds, .sounds = {{.time = 10, .action = k_Sigh, .mode = 1}}};
		villager = world.MakeVillager();
	}

	void Play(AnimId clip, uint32_t place, uint32_t played, clip_sound_player::ClipTiming timing = {.duration = 1000})
	{
		clip_sound_player::Play(world, villager, clip, timing, place, played, glm::vec3(0.0f));
	}

	FakeWorld world;
	entt::entity villager {entt::null};
};

std::vector<int32_t> Keys(audio::SoundSize size, int32_t action)
{
	return {static_cast<int32_t>(size), 2, clip_sounds::k_PeopleSounds, k_Grass, action};
}
} // namespace

TEST(ClipSoundPlayer, AVillagersSoundsPlayByItsSizeAndItsHomesBanterFromItsHome)
{
	Fixture f;
	f.Play(k_Walk, 50, 300);
	ASSERT_EQ(f.world.played.size(), 3U);
	EXPECT_EQ(f.world.played[0].bank, "editor.sad");
	EXPECT_EQ(f.world.played[0].keys, Keys(audio::SoundSize::Medium, k_Step));
	EXPECT_EQ(f.world.played[0].owner, f.villager);
	EXPECT_EQ(f.world.played[1].bank, "VillagersBanter.sad");
	EXPECT_EQ(f.world.played[1].owner, f.world.home);
	EXPECT_EQ(f.world.played[2].keys, Keys(audio::SoundSize::Medium, k_Sigh));
}

TEST(ClipSoundPlayer, OnlyTheSoundsTheClipPassesPlay)
{
	Fixture f;
	f.Play(k_Walk, 150, 100);
	ASSERT_EQ(f.world.played.size(), 1U);
	EXPECT_EQ(f.world.played[0].bank, "VillagersBanter.sad");
	f.world.played.clear();
	f.Play(k_Walk, 150, 0);
	EXPECT_TRUE(f.world.played.empty());
}

TEST(ClipSoundPlayer, AManIsHeardLargeAndAChildSmall)
{
	Fixture f;
	auto& person = f.world.registry.Get<Villager>(f.villager);
	person.sex = Villager::Sex::MALE;
	f.Play(k_Walk, 50, 100);
	person.lifeStage = Villager::LifeStage::Child;
	f.Play(k_Walk, 50, 100);
	ASSERT_EQ(f.world.played.size(), 2U);
	EXPECT_EQ(f.world.played[0].keys, Keys(audio::SoundSize::Large, k_Step));
	EXPECT_EQ(f.world.played[1].keys, Keys(audio::SoundSize::Small, k_Step));
}

TEST(ClipSoundPlayer, ADeadPersonsClipIsQuiet)
{
	Fixture f;
	f.world.life = 0.0f;
	f.Play(k_Walk, 50, 300);
	EXPECT_TRUE(f.world.played.empty());
}

TEST(ClipSoundPlayer, InsideTheTempleOnlySoundsNotPlayedTheOrdinaryWayAreHeard)
{
	Fixture f;
	f.world.insideTemple = true;
	f.Play(k_Walk, 50, 300);
	ASSERT_EQ(f.world.played.size(), 1U);
	EXPECT_EQ(f.world.played[0].bank, "VillagersBanter.sad");
}

TEST(ClipSoundPlayer, OnlySoundsPlayedTheOrdinaryWayAreHeardAsTheirSamplesAllow)
{
	Fixture f;
	f.Play(k_Walk, 50, 300);
	ASSERT_EQ(f.world.played.size(), 3U);
	EXPECT_TRUE(f.world.played[0].bySampleRules);
	EXPECT_FALSE(f.world.played[1].bySampleRules);
	EXPECT_TRUE(f.world.played[2].bySampleRules);
}

TEST(ClipSoundPlayer, AClipPlayedOnceIsQuietOnceFinishedAndALoopingOneComesRound)
{
	Fixture f;
	f.Play(k_Walk, 1000, 100);
	EXPECT_TRUE(f.world.played.empty());
	// Round the end of a looping clip into its first step
	f.Play(k_Walk, 950, 200, {.duration = 1000, .looping = true});
	ASSERT_EQ(f.world.played.size(), 1U);
	EXPECT_EQ(f.world.played[0].keys, Keys(audio::SoundSize::Medium, k_Step));
}

TEST(ClipSoundPlayer, AThrownPersonScreamsOnlyEarlyInItsFlight)
{
	Fixture f;
	f.Play(k_Thrown, 0, 50);
	EXPECT_EQ(f.world.played.size(), 1U);
	f.world.registry.Get<LivingAction>(f.villager).turnsSinceStateChange = clip_sounds::k_ThrownSoundTurns;
	f.Play(k_Thrown, 0, 50);
	EXPECT_EQ(f.world.played.size(), 1U);
}

TEST(ClipSoundPlayer, AnAnimalHasNoHomeToBanterFrom)
{
	Fixture f;
	const auto animal = f.world.registry.Create();
	f.world.clips[k_Walk].soundType = 3;
	clip_sound_player::Play(f.world, animal, k_Walk, {.duration = 1000}, 50, 300, glm::vec3(0.0f));
	ASSERT_EQ(f.world.played.size(), 2U);
	EXPECT_EQ(f.world.played[0].keys,
	          (std::vector<int32_t> {static_cast<int32_t>(audio::SoundSize::Medium), 2, 3, k_Grass, k_Step}));
	EXPECT_EQ(f.world.played[0].owner, animal);
	EXPECT_EQ(f.world.played[1].keys[4], k_Sigh);
}
