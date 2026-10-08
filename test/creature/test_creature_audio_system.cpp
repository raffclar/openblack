/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature audio system on a recording audio service: the shared bank for footsteps and blows and the species' own
// for its voice, the modes that stop a sound or let it run out, the creature as the sound's tracked owner on the ground
// it stands on, the script's switch for other players' voices, muting, and the footsteps leaving prints whether heard
// or not; and the request the audio is asked for, built on its own

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Creature/CreatureAudio.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::audio::AnimAction;
using openblack::audio::SoundAction;
using openblack::audio::SoundSurface;
using openblack::creature_audio::EventKind;
using openblack::creature_audio::SoundEvent;
using openblack::ecs::systems::CreatureAudioSystem;

namespace
{
constexpr audio::BankId k_VoiceBank = 42;

/// Records what the creatures ask of the audio; every species has a voice bank
class RecordingAudio final: public audio::AudioManagerNoOp
{
public:
	struct Played
	{
		audio::Owner owner;
		float distance;
		audio::AnimKey key;
		AnimAction action;
		audio::BankId bank;
		bool track;
	};
	std::vector<Played> played;
	std::vector<std::string> voiceBanks;

	[[nodiscard]] audio::BankId CreatureBank(std::string_view species) override
	{
		voiceBanks.emplace_back(species);
		return k_VoiceBank;
	}
	audio::Channel PlayAnimationEffect(audio::Owner owner, float distance, const audio::AnimKey& key, AnimAction action,
	                                   audio::BankId bank, bool track, float, float) override
	{
		played.push_back({owner, distance, key, action, bank, track});
		return 5;
	}
};

/// Counts the footsteps that reach it
class FakeFootprints final: public ecs::systems::FootprintSystemInterface
{
public:
	std::vector<entt::entity> steps;

	void Update(std::chrono::duration<float, std::milli>) override {}
	void Step(entt::entity creature) override { steps.push_back(creature); }
	void Reset() override {}
	[[nodiscard]] std::span<const creature_footprints::Footprint> GetPrints() const override { return {}; }
	[[nodiscard]] size_t GetDroppedCount() const override { return 0; }
	[[nodiscard]] bool IsShown() const override { return true; }
	void SetShown(bool) override {}
	[[nodiscard]] std::optional<bool> GetAprilFoolsOverride() const override { return std::nullopt; }
	void SetAprilFoolsOverride(std::optional<bool>) override {}
	[[nodiscard]] bool IsAprilFools() const override { return false; }
};

constexpr SoundEvent k_Footstep {.kind = EventKind::Generic, .timeMs = 100, .action = SoundAction::FootstepNormal, .mode = 0};
constexpr SoundEvent k_Roar {.kind = EventKind::Voice, .timeMs = 200, .action = SoundAction::RoarShort, .mode = 0};

TEST(CreatureAudioRequest, TheSharedBankForFootstepsAndTheSpeciesOwnForItsVoice)
{
	const auto footstep =
	    creature_audio::Request(k_Footstep, 1.0f, 0.0f, 3, SoundSurface::Grass, "ape_voice", CreatureType::GiantApe);
	EXPECT_TRUE(footstep.generic);
	EXPECT_TRUE(footstep.voiceStem.empty());
	EXPECT_EQ(footstep.action, AnimAction::Play);
	EXPECT_EQ(footstep.keys, creature_audio::Keys(1.0f, 0.0f, 3, SoundSurface::Grass, SoundAction::FootstepNormal));

	const auto roar = creature_audio::Request(k_Roar, 2.0f, -1.0f, 3, SoundSurface::Mud, "ape_voice", CreatureType::GiantApe);
	EXPECT_FALSE(roar.generic);
	EXPECT_EQ(roar.voiceStem, creature_audio::VoiceBankStem("ape_voice", CreatureType::GiantApe));
	EXPECT_EQ(roar.keys, creature_audio::Keys(2.0f, -1.0f, 3, SoundSurface::Mud, SoundAction::RoarShort));
}

TEST(CreatureAudioRequest, ModeOneStopsTheSoundAndTwoLetsItRunOut)
{
	auto event = k_Footstep;
	event.mode = 1;
	EXPECT_EQ(creature_audio::Request(event, 1.0f, 0.0f, 3, SoundSurface::Grass, "", CreatureType::GiantApe).action,
	          AnimAction::Stop);
	event.mode = 2;
	EXPECT_EQ(creature_audio::Request(event, 1.0f, 0.0f, 3, SoundSurface::Grass, "", CreatureType::GiantApe).action,
	          AnimAction::Release);
}

class CreatureAudioSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the listener at the origin
		audio::GameQueries queries;
		queries.camera = []() -> std::optional<audio::CameraState> { return audio::CameraState {}; };
		audio::Init(std::move(queries));
		// the creature rigs' loader reads the meshes the eyes sit on through the file system, which the game always
		// has; made after the audio's start so that it loads no sample banks
		_fileSystem.emplace();
		_restoreAudio.emplace();
		_audio = &static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
		_restoreFootprints.emplace();
		_footprints = &static_cast<FakeFootprints&>(Locator::footprintSystem::emplace<FakeFootprints>());
	}
	void TearDown() override
	{
		_restoreFootprints.reset();
		_restoreAudio.reset();
		_fileSystem.reset();
		audio::Shutdown();
	}

	/// A Giant Ape whose standing animation has a footstep at 100 ms and a roar at 200 ms
	entt::entity Ape(PlayerNames owner)
	{
		test::creature_block::Block block;
		test::creature_block::Clip stand;
		stand.events = {{.type = 2, .frame = 100, .action = static_cast<uint32_t>(SoundAction::FootstepNormal), .mode = 0},
		                {.type = 0, .frame = 200, .action = static_cast<uint32_t>(SoundAction::RoarShort), .mode = 0}};
		block.clips = {stand};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		return test::creature_world::World::MakeCreature(glm::vec3(30.0f, 0.0f, 40.0f), owner);
	}
	/// The standing animation played at a time, by itself
	static void PlayAt(entt::entity creature, float timeMs)
	{
		auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
		animation.slots = {{.animation = 0, .timeMs = timeMs, .weight = 1.0f, .mirrored = false}};
	}
	/// Two frames: one at 50 ms, then one at 250 ms, which passes both moments
	void TwoFrames(entt::entity creature)
	{
		PlayAt(creature, 50.0f);
		_system.Update(std::chrono::duration<float, std::milli>(50.0f));
		PlayAt(creature, 250.0f);
		_system.Update(std::chrono::duration<float, std::milli>(200.0f));
	}

	std::optional<test::ScopedDefaultFileSystem> _fileSystem;
	test::creature_world::World _world;
	std::optional<test::RestoreService<Locator::audio>> _restoreAudio;
	std::optional<test::RestoreService<Locator::footprintSystem>> _restoreFootprints;
	RecordingAudio* _audio {nullptr};
	FakeFootprints* _footprints {nullptr};
	CreatureAudioSystem _system;
};

TEST_F(CreatureAudioSystemTest, TheLocalCreaturesStepsAndVoicePlayFromItAndItsStepsLeavePrints)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	TwoFrames(creature);

	ASSERT_EQ(_audio->played.size(), 2u);
	const auto& step = _audio->played.at(0);
	const auto& roar = _audio->played.at(1);
	EXPECT_EQ(step.owner.kind, audio::Owner::Kind::Thing);
	EXPECT_EQ(step.owner.thing, creature);
	EXPECT_TRUE(step.track);
	EXPECT_EQ(step.action, AnimAction::Play);
	EXPECT_EQ(step.bank, audio::Bank(audio::SfxBank::Creature));
	EXPECT_FLOAT_EQ(step.distance, 50.0f);
	const glm::vec3 at(30.0f, 0.0f, 40.0f);
	const auto surface = static_cast<SoundSurface>(ecs::sea_cells::GetSurfaceType(at));
	EXPECT_EQ(step.key, creature_audio::Keys(1.0f, 0.0f, 3, surface, SoundAction::FootstepNormal).ToArray());
	EXPECT_EQ(roar.bank, k_VoiceBank);
	EXPECT_EQ(roar.key[4], static_cast<int32_t>(SoundAction::RoarShort));
	ASSERT_EQ(_audio->voiceBanks.size(), 1u);
	EXPECT_EQ(_audio->voiceBanks.front(), creature_audio::VoiceBankStem("ape_voice", CreatureType::GiantApe));
	ASSERT_EQ(_footprints->steps.size(), 1u);
	EXPECT_EQ(_footprints->steps.front(), creature);

	const auto& heard = test::creature_world::World::Registry().Get<CreatureAudio>(creature).recent;
	ASSERT_EQ(heard.size(), 2u);
	EXPECT_TRUE(heard.front().played);
	EXPECT_EQ(heard.front().channel, 5u);
}

TEST_F(CreatureAudioSystemTest, AnotherPlayersCreatureSpeaksOnlyWhenTheScriptLetsIt)
{
	const auto creature = Ape(PlayerNames::PLAYER_TWO);
	_system.SetOtherVoicesEnabled(false);
	EXPECT_FALSE(_system.AreOtherVoicesEnabled());
	EXPECT_EQ(audio::GetScriptAudioState().creatureSound.load(), 0);
	TwoFrames(creature);
	// its footstep is heard, its roar is not
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played.front().key[4], static_cast<int32_t>(SoundAction::FootstepNormal));

	_system.SetOtherVoicesEnabled(true);
	EXPECT_TRUE(_system.AreOtherVoicesEnabled());
	_audio->played.clear();
	PlayAt(creature, 50.0f);
	_system.Update(std::chrono::duration<float, std::milli>(800.0f));
	PlayAt(creature, 250.0f);
	_system.Update(std::chrono::duration<float, std::milli>(200.0f));
	EXPECT_EQ(_audio->played.size(), 2u);
}

TEST_F(CreatureAudioSystemTest, MutedNothingPlaysButTheStepsStillLeavePrints)
{
	const auto creature = Ape(PlayerNames::PLAYER_ONE);
	_system.SetMuted(true);
	TwoFrames(creature);
	EXPECT_TRUE(_audio->played.empty());
	EXPECT_EQ(_footprints->steps.size(), 1u);
}

TEST_F(CreatureAudioSystemTest, PlayingAnEventSoundsItAtOnceAndOnlyCreaturesAreHeard)
{
	const auto creature = Ape(PlayerNames::PLAYER_TWO);
	_system.SetMuted(true);
	auto stop = k_Footstep;
	stop.mode = 1;
	_system.Play(creature, stop);
	ASSERT_EQ(_audio->played.size(), 1u);
	EXPECT_EQ(_audio->played.front().action, AnimAction::Stop);

	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_system.Play(rock, k_Footstep);
	EXPECT_EQ(_audio->played.size(), 1u);
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureAudio>(rock));
}
} // namespace
