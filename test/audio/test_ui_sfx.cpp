/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Audio/Audio.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/Sound.h"
#include "Audio/GameQueries.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "support/WorldSystems.h"

// The interface and hand sounds. The samples come from the installation's InGame.sad, so the records the sites rely on
// (their user parameters) are the real ones. Without OPENBLACK_TEST_BW_ROOT the tests skip. The *Synthetic tests run
// the same checks on records built by the test (LoadSyntheticInGame) with the fields the real ones have.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::ecs::components;

namespace
{
class FakeOutput final: public SampleOutput
{
public:
	std::array<bool, 16> playing {};
	std::array<Start, 16> starts {};
	std::array<entt::id_type, 16> sounds {};
	int plays {0};

	bool Play(size_t channel, Sound& sound, const Start& start) override
	{
		playing[channel] = true;
		starts[channel] = start;
		sounds[channel] = entt::hashed_string(fmt::format("InGame.sad/{}", sound.id).c_str()).value();
		++plays;
		return true;
	}
	void Stop(size_t channel) override { playing[channel] = false; }
	[[nodiscard]] bool Playing(size_t channel) const override { return playing[channel]; }
	void SetGain(size_t, float) override {}
	void SetPitch(size_t, float) override {}
	void SetPosition(size_t, glm::vec3) override {}
	void ReleaseLoop(size_t) override {}
	void SetListener(glm::vec3) override {}
	void Update() override {}
	[[nodiscard]] size_t Sources() const override { return 0; }
	void DeleteAll() override { playing.fill(false); }
	[[nodiscard]] int Channels() const
	{
		int count = 0;
		for (const bool on : playing)
		{
			count += on ? 1 : 0;
		}
		return count;
	}
};

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

class UiSfxTest: public ::testing::Test
{
protected:
	FakeOutput output;
	std::map<entt::id_type, Sound> sounds;
	static inline bool s_InsideCitadel = false;
	static inline int s_InterfaceState = 0;
	BankId inGame {k_NoBank};

	void SetUp() override
	{
		s_InsideCitadel = false;
		s_InterfaceState = 0;
		GameQueries queries;
		queries.insideCitadel = []() { return s_InsideCitadel; };
		queries.interfaceState = []() { return s_InterfaceState; };
		// the 3D cull of PlaySoundEffect measures from the camera: the origin here
		queries.camera = []() -> std::optional<CameraState> { return CameraState {}; };
		audio::Init(std::move(queries));
		inGame = RegisterBank("audio/sfx/game/InGame.sad", "InGame.sad");
		sample_play::Backend backend;
		backend.output = &output;
		backend.sound = [this](entt::id_type id) -> Sound* {
			const auto found = sounds.find(id);
			return found != sounds.end() ? &found->second : nullptr;
		};
		backend.rand = []() { return 16383; };
		backend.camera = []() -> std::optional<glm::vec3> { return glm::vec3(0.0f); };
		backend.ownerPosition = [](const Owner& owner) { return OwnerSoundPosition(owner); };
		sample_play::SetBackend(std::move(backend));
		sample_play::SetMainVolume(127);
		audio::ClearMap();
		output = FakeOutput {};
		// a house (living quarters) and a town centre (civic) of the info records, for ecs::abodes
		auto info = std::make_unique<InfoConstants>();
		info->abode.at(0).abodeNumber = AbodeNumber::A;
		info->abode.at(0).abodeType = AbodeType::LivingQuarters;
		info->abode.at(1).abodeNumber = AbodeNumber::TownCentre;
		info->abode.at(1).abodeType = AbodeType::TownCentre;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		influence::detail::ResetHandCrossing();
	}
	void TearDown() override
	{
		audio::ClearMap();
		sample_play::SetMainVolume(127); // back to its default (QMixer's maximum)
		sample_play::SetBackend({});
		audio::Shutdown();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeAbode(AbodeNumber number)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto abode = registry.Create();
		registry.Assign<Abode>(abode, number, 0u, 0u, 0u);
		registry.Assign<Transform>(abode, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return abode;
	}

	static entt::entity MakeTown(const glm::vec3& position, float radius, PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<Town>(town, 0u).owner = owner;
		registry.Assign<Transform>(town, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<TownInfluence>(town).radius = radius;
		return town;
	}

	/// The real InGame.sad records of the installation, keyed as SampleId(InGame, n)
	bool LoadInGame()
	{
		const auto root = GameRoot();
		if (!root)
		{
			return false;
		}
		pack::PackFile pack;
		if (pack.Open(*root / "Audio/SFX/Game/InGame.sad") != pack::PackResult::Success)
		{
			return false;
		}
		const auto& headers = pack.GetAudioSampleHeaders();
		for (const auto& header : headers)
		{
			std::vector<std::vector<uint8_t>> buffer;
			const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, header, buffer);
			sound->bank = inGame;
			sounds.insert_or_assign(SampleId(inGame, header.id), *sound);
		}
		return !headers.empty();
	}

	/// InGame records built here with the fields the interface relies on, loaded like the real ones:
	/// 46 user parameter 2; 52 user parameter 1, volume 40, play mode 1, min / max 100 / 300;
	/// 110..118 user parameter 1, 5 % pitch spread, min / max 100 / 150; 159 user parameter 0
	void LoadSyntheticInGame()
	{
		const auto add = [this](int id, uint16_t userParam, uint16_t overrides, uint8_t volume, float minDistance,
		                        float maxDistance, pack::AudioBankLoop mode, uint16_t pitchDeviation) {
			pack::AudioBankSampleHeader header;
			std::memset(&header, 0, sizeof(header));
			header.id = id;
			header.priority = 100;
			header.sampleRate = 22050;
			header.userParam = userParam;
			header.unknown10 = overrides;
			header.volume = volume;
			header.minDist = minDistance;
			header.maxDist = maxDistance;
			header.loopType = mode;
			header.pitchDeviation = pitchDeviation;
			std::vector<std::vector<uint8_t>> buffer;
			const auto sound = resources::SoundLoader {}(resources::SoundLoader::FromBufferTag {}, header, buffer);
			sound->bank = inGame;
			sounds.insert_or_assign(SampleId(inGame, header.id), *sound);
		};
		// the volume (0x20), min distance (0x80), max distance (0x100) and play mode (0x400) flags
		add(46, 2, 0, 0, 0.0f, 0.0f, pack::AudioBankLoop::None, 0);
		add(52, 1, 0x20 | 0x80 | 0x100 | 0x400, 40, 100.0f, 300.0f, pack::AudioBankLoop::Restart, 0);
		for (int id = 110; id <= 118; ++id)
		{
			add(id, 1, 0x80 | 0x100, 0, 100.0f, 150.0f, pack::AudioBankLoop::None, 5);
		}
		add(159, 0, 0, 0, 0.0f, 0.0f, pack::AudioBankLoop::None, 0);
	}

	[[nodiscard]] const Sound* Record(int number) const
	{
		const auto found = sounds.find(SampleId(inGame, number));
		return found != sounds.end() ? &found->second : nullptr;
	}

	/// A tap on a house: ecs::abodes::InterfaceTap itself
	void Knock(const glm::vec3& handPosition)
	{
		if (!house)
		{
			house = MakeAbode(AbodeNumber::A);
		}
		ecs::abodes::InterfaceTap(*house, handPosition, false, handPosition);
	}
	std::optional<entt::entity> house;
};
} // namespace

// The user parameters that decide what the interface plays inside the citadel
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, InGameRecordsOfTheInterface)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// 0 = it does not play inside the citadel (only user parameter 2 does)
	for (const int sample : {42, 134, 159, 160, 173, 205, 40, 41, 119})
	{
		const auto* record = Record(sample);
		ASSERT_NE(record, nullptr) << sample;
		EXPECT_EQ(static_cast<uint16_t>(record->userParam), 0) << sample;
	}
	// 1 = silent while a script holds the wide screen: the knock, the acknowledgement and the influence crossing
	for (int sample = 110; sample <= 118; ++sample)
	{
		const auto* record = Record(sample);
		ASSERT_NE(record, nullptr) << sample;
		EXPECT_EQ(static_cast<uint16_t>(record->userParam), 1) << sample;
	}
	EXPECT_EQ(static_cast<uint16_t>(Record(1)->userParam), 1);  // G_AcknowledgeCommand
	EXPECT_EQ(static_cast<uint16_t>(Record(52)->userParam), 1); // G_HandThroughInfluence_01
	// 2 = the only class that plays inside the citadel; 4 = silent in the interface states 0x10 / 0x16 / 0x17
	EXPECT_EQ(static_cast<uint16_t>(Record(46)->userParam), 2);  // G_Woosh_01
	EXPECT_EQ(static_cast<uint16_t>(Record(129)->userParam), 4); // G_VirtualInfluence_04
	// G_HandThroughInfluence_01: volume 40, mode 1 (a new channel every time), min / max 100 / 300
	EXPECT_EQ(Record(52)->volume127, 40);
	EXPECT_EQ(Record(52)->playMode, 1);
	EXPECT_FLOAT_EQ(Record(52)->minDistance, 100.0f);
	EXPECT_FLOAT_EQ(Record(52)->maxDistance, 300.0f);
	// G_KnockRoofMulti: 5 % pitch spread, min / max 100 / 150
	EXPECT_EQ(Record(110)->pitchDeviation, 5);
	EXPECT_FLOAT_EQ(Record(110)->minDistance, 100.0f);
	EXPECT_FLOAT_EQ(Record(110)->maxDistance, 150.0f);
}

// Tapping a house ten times gives 110..118 and starts again at 110
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, KnockingOnARoofWalksTheNineSamples)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	std::vector<int> played;
	for (int i = 0; i < 10; ++i)
	{
		output = FakeOutput {};
		Knock(glm::vec3(0.0f, 0.0f, 10.0f));
		ASSERT_EQ(output.plays, 1) << i;
		for (size_t channel = 0; channel < 16; ++channel)
		{
			if (output.playing[channel])
			{
				played.push_back(static_cast<int>(output.sounds[channel] == SampleId(inGame, 110) ? 110 : 0));
				// the sample the channel started, found by its key
				for (int sample = 110; sample <= 118; ++sample)
				{
					if (output.sounds[channel] == SampleId(inGame, sample))
					{
						played.back() = sample;
					}
				}
				// 3D at the hand's point, with the sample's min / max
				EXPECT_TRUE(output.starts[channel].is3D);
				EXPECT_EQ(output.starts[channel].position, glm::vec3(0.0f, 0.0f, 10.0f));
				EXPECT_FLOAT_EQ(output.starts[channel].maxDistance, 150.0f);
			}
		}
	}
	// the knock counter is process state that nothing resets (other tests may have knocked): from wherever it is,
	// ten taps walk 110..118 in turn and wrap once
	ASSERT_EQ(played.size(), 10u);
	for (size_t i = 1; i < played.size(); ++i)
	{
		EXPECT_EQ(played[i], played[i - 1] == 118 ? 110 : played[i - 1] + 1) << i;
	}
	EXPECT_EQ(played[9], played[0]);
}

TEST_F(UiSfxTest, KnockingOnARoofWalksTheNineSamplesSynthetic)
{
	LoadSyntheticInGame();
	std::vector<int> played;
	for (int i = 0; i < 10; ++i)
	{
		output = FakeOutput {};
		Knock(glm::vec3(0.0f, 0.0f, 10.0f));
		ASSERT_EQ(output.plays, 1) << i;
		for (size_t channel = 0; channel < 16; ++channel)
		{
			if (output.playing[channel])
			{
				played.push_back(0);
				for (int sample = 110; sample <= 118; ++sample)
				{
					if (output.sounds[channel] == SampleId(inGame, sample))
					{
						played.back() = sample;
					}
				}
				// 3D at the hand's point, with the record's max distance
				EXPECT_TRUE(output.starts[channel].is3D);
				EXPECT_EQ(output.starts[channel].position, glm::vec3(0.0f, 0.0f, 10.0f));
				EXPECT_FLOAT_EQ(output.starts[channel].maxDistance, 150.0f);
			}
		}
	}
	// the knock counter is process state other tests may have moved: from wherever it is, ten taps walk 110..118
	// in turn and wrap once
	ASSERT_EQ(played.size(), 10u);
	EXPECT_NE(played[0], 0);
	for (size_t i = 1; i < played.size(); ++i)
	{
		EXPECT_EQ(played[i], played[i - 1] == 118 ? 110 : played[i - 1] + 1) << i;
	}
	EXPECT_EQ(played[9], played[0]);
}

// Inside the citadel only the samples of user parameter 2 play, so neither the menu click (0) nor the knock (1)
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, TheCitadelSilencesTheMenuClickAndTheKnock)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// outside: both play
	EXPECT_NE(PlaySoundEffect(Owner::None(), 159, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 2);
	s_InsideCitadel = true;
	output = FakeOutput {};
	EXPECT_EQ(PlaySoundEffect(Owner::None(), 159, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 0);
	// G_Woosh_01 (user parameter 2) still plays there
	EXPECT_NE(PlaySoundEffect(Owner::None(), 46, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	EXPECT_EQ(output.plays, 1);
}

TEST_F(UiSfxTest, TheCitadelSilencesTheMenuClickAndTheKnockSynthetic)
{
	LoadSyntheticInGame();
	// outside: the user parameter 0 click and the user parameter 1 knock both play
	EXPECT_NE(PlaySoundEffect(Owner::None(), 159, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 2);
	s_InsideCitadel = true;
	output = FakeOutput {};
	EXPECT_EQ(PlaySoundEffect(Owner::None(), 159, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 0);
	// user parameter 2 still plays there
	EXPECT_NE(PlaySoundEffect(Owner::None(), 46, 3, 0, false, false, SfxBank::InGame), k_NoChannel);
	EXPECT_EQ(output.plays, 1);
}

// G_HandThroughInfluence_01: the .sad's mode 1 takes a new channel for every crossing, and the 300 max distance culls
// a hand farther than that from the camera
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, InfluenceCrossingTakesANewChannelAndIsCulledAt300)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto cross = [this](const glm::vec3& handPosition) {
		PlayOptions options;
		options.sample = {inGame, 52};
		options.owner = Owner::None();
		options.is3D = true;
		options.track = false;
		options.position = handPosition;
		return PlaySoundEffect(options);
	};
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 50.0f)), k_NoChannel);
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 60.0f)), k_NoChannel);
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 70.0f)), k_NoChannel);
	EXPECT_EQ(output.Channels(), 3); // mode 1: three channels, none restarted
	EXPECT_EQ(cross(glm::vec3(0.0f, 0.0f, 301.0f)), k_NoChannel);
	EXPECT_EQ(output.Channels(), 3);
}

TEST_F(UiSfxTest, InfluenceCrossingTakesANewChannelAndIsCulledAt300Synthetic)
{
	LoadSyntheticInGame();
	ASSERT_EQ(Record(52)->playMode, 1);
	ASSERT_FLOAT_EQ(Record(52)->maxDistance, 300.0f);
	const auto cross = [this](const glm::vec3& handPosition) {
		PlayOptions options;
		options.sample = {inGame, 52};
		options.owner = Owner::None();
		options.is3D = true;
		options.track = false;
		options.position = handPosition;
		return PlaySoundEffect(options);
	};
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 50.0f)), k_NoChannel);
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 60.0f)), k_NoChannel);
	EXPECT_NE(cross(glm::vec3(0.0f, 0.0f, 70.0f)), k_NoChannel);
	EXPECT_EQ(output.Channels(), 3); // mode 1: three channels, none restarted
	EXPECT_EQ(cross(glm::vec3(0.0f, 0.0f, 301.0f)), k_NoChannel);
	EXPECT_EQ(output.Channels(), 3);
}

// Only an abode with the living-quarters bit knocks; a civic one (the town centre) is tapped in
// silence, and a non-abode is not tappable at all
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, OnlyLivingQuartersKnock)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto centre = MakeAbode(AbodeNumber::TownCentre);
	EXPECT_TRUE(ecs::abodes::InterfaceValidToTap(centre));
	ecs::abodes::InterfaceTap(centre, glm::vec3(0.0f, 0.0f, 10.0f), false, glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 0);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 1);
	const auto town = MakeTown(glm::vec3(0.0f), 50.0f, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(ecs::abodes::InterfaceValidToTap(town));
}

TEST_F(UiSfxTest, OnlyLivingQuartersKnockSynthetic)
{
	LoadSyntheticInGame();
	const auto centre = MakeAbode(AbodeNumber::TownCentre);
	EXPECT_TRUE(ecs::abodes::InterfaceValidToTap(centre));
	ecs::abodes::InterfaceTap(centre, glm::vec3(0.0f, 0.0f, 10.0f), false, glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 0);
	Knock(glm::vec3(0.0f, 0.0f, 10.0f));
	EXPECT_EQ(output.plays, 1);
	const auto town = MakeTown(glm::vec3(0.0f), 50.0f, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(ecs::abodes::InterfaceValidToTap(town));
}

// The 52 sounds when the hand's move crosses the edge of a circle, in or out; a circle that
// grows under a still hand changes the "inside" bit but crosses nothing, so it is silent and the bit is kept
// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST_F(UiSfxTest, InfluenceCrossingNeedsTheHandToCrossAnEdge)
{
	if (!LoadInGame())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto town = MakeTown(glm::vec3(0.0f), 50.0f, PlayerNames::PLAYER_ONE);
	MakeTown(glm::vec3(200.0f, 0.0f, 0.0f), 30.0f, PlayerNames::NEUTRAL); // the neutral player has no circle
	// Update3DInfluence makes the circles (a turn multiple of 10, the dirty byte set) and the player's border is shown:
	// without it a crossing makes no ripple and no sound
	game_clock::SetTurn(0);
	influence::ForceNeedUpdateInfluence();
	influence::Update3DInfluence();
	influence::ShowBoundary(PlayerNames::PLAYER_ONE);
	const glm::vec3 outside(0.0f, 0.0f, 60.0f);
	const glm::vec3 inside(0.0f, 0.0f, 40.0f);
	influence::ProcessHandCrossing(outside); // the first call only remembers
	EXPECT_EQ(output.plays, 0);
	influence::ProcessHandCrossing(inside);
	EXPECT_EQ(output.plays, 1);
	influence::ProcessHandCrossing(inside);
	EXPECT_EQ(output.plays, 1);
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	// the circle grows over the still hand: the bit changes, no edge was crossed
	Locator::entitiesRegistry::value().Get<TownInfluence>(town).radius = 70.0f;
	influence::ForceNeedUpdateInfluence();
	influence::Update3DInfluence();
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	// out again across the new edge
	influence::ProcessHandCrossing(glm::vec3(0.0f, 0.0f, 80.0f));
	EXPECT_EQ(output.plays, 3);
	// the neutral town's edge: nothing
	influence::ProcessHandCrossing(glm::vec3(200.0f, 0.0f, 80.0f));
	influence::ProcessHandCrossing(glm::vec3(200.0f, 0.0f, 0.0f));
	EXPECT_EQ(output.plays, 3);
}

TEST_F(UiSfxTest, InfluenceCrossingNeedsTheHandToCrossAnEdgeSynthetic)
{
	LoadSyntheticInGame();
	const auto town = MakeTown(glm::vec3(0.0f), 50.0f, PlayerNames::PLAYER_ONE);
	MakeTown(glm::vec3(200.0f, 0.0f, 0.0f), 30.0f, PlayerNames::NEUTRAL); // the neutral player has no circle
	// the circles are made on a turn multiple of 10 with the dirty flag set, and the player's border is shown
	game_clock::SetTurn(0);
	influence::ForceNeedUpdateInfluence();
	influence::Update3DInfluence();
	influence::ShowBoundary(PlayerNames::PLAYER_ONE);
	const glm::vec3 outside(0.0f, 0.0f, 60.0f);
	const glm::vec3 inside(0.0f, 0.0f, 40.0f);
	influence::ProcessHandCrossing(outside); // the first call only remembers
	EXPECT_EQ(output.plays, 0);
	influence::ProcessHandCrossing(inside);
	EXPECT_EQ(output.plays, 1);
	influence::ProcessHandCrossing(inside);
	EXPECT_EQ(output.plays, 1);
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	// the circle grows over the still hand: the bit changes, no edge was crossed
	Locator::entitiesRegistry::value().Get<TownInfluence>(town).radius = 70.0f;
	influence::ForceNeedUpdateInfluence();
	influence::Update3DInfluence();
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	influence::ProcessHandCrossing(outside);
	EXPECT_EQ(output.plays, 2);
	// out again across the new edge
	influence::ProcessHandCrossing(glm::vec3(0.0f, 0.0f, 80.0f));
	EXPECT_EQ(output.plays, 3);
	// the neutral town's edge: nothing
	influence::ProcessHandCrossing(glm::vec3(200.0f, 0.0f, 80.0f));
	influence::ProcessHandCrossing(glm::vec3(200.0f, 0.0f, 0.0f));
	EXPECT_EQ(output.plays, 3);
}
