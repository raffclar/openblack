/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Influence.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/InfluenceSystem.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The heart of each citadel reaches this far on a land with no number
constexpr float k_HeartReach = 50.0f;
constexpr std::array k_Players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
} // namespace

class CitadelReach: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The tables are large: made on the heap
		_info = std::make_unique<InfoConstants>();
		_info->citadelHeart.influence = k_HeartReach;
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		for (size_t i = 0; i < k_Players.size(); ++i)
		{
			const auto temple = registry.Create();
			registry.Assign<components::Temple>(temple, k_Players.at(i));
			const glm::vec3 position {1000.0f + (500.0f * static_cast<float>(i)), 0.0f, 1000.0f};
			registry.Assign<components::Transform>(temple, position, glm::mat3(1.0f), glm::vec3(1.0f));
			_temples.at(i) = temple;
		}
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	[[nodiscard]] static bool AnyReachFixed()
	{
		return !Locator::entitiesRegistry::value().Underlying().storage<components::CitadelInfluence>().empty();
	}

	std::unique_ptr<InfoConstants> _info;
	std::array<entt::entity, k_Players.size()> _temples {};
	systems::InfluenceSystem _influence;
};

TEST_F(CitadelReach, AQuestionBeforeTheTurnGivesTheReachAndFixesNothing)
{
	// The interface of the third player's computer asks about its own player before the first turn
	EXPECT_FLOAT_EQ(_influence.CitadelReachOf(PlayerNames::PLAYER_THREE), k_HeartReach);
	EXPECT_GT(_influence.PlayerRawInfluence(PlayerNames::PLAYER_THREE, glm::vec3(2000.0f, 0.0f, 1000.0f)), 0.0f);
	EXPECT_FALSE(AnyReachFixed());
}

TEST_F(CitadelReach, TheTurnFixesTheReachesInTheOrderOfThePlayersWhateverWasAskedBefore)
{
	// One computer asks about the third player first, another computer asks nothing. Both must keep the same world.
	static_cast<void>(_influence.CitadelReachOf(PlayerNames::PLAYER_THREE));
	_influence.FixCitadelReaches();
	const auto& storage = Locator::entitiesRegistry::value().Underlying().storage<components::CitadelInfluence>();
	ASSERT_EQ(storage.size(), _temples.size());
	for (size_t i = 0; i < _temples.size(); ++i)
	{
		EXPECT_EQ(storage.index(_temples.at(i)), i);
		EXPECT_FLOAT_EQ(storage.get(_temples.at(i)).reach, k_HeartReach);
	}
}

TEST_F(CitadelReach, TheAnswerIsTheSameBeforeAndAfterTheTurnFixesIt)
{
	const float before = _influence.CitadelReachOf(PlayerNames::PLAYER_TWO);
	_influence.FixCitadelReaches();
	EXPECT_FLOAT_EQ(_influence.CitadelReachOf(PlayerNames::PLAYER_TWO), before);
}

namespace
{
/// A town reaches this far when it is made, before it has a turn
constexpr float k_TownReach = 20.0f;
} // namespace

class PowerAtLoad: public CitadelReach
{
protected:
	void SetUp() override
	{
		CitadelReach::SetUp();
		// The locator keeps a copy, so the constants are put in it again
		_info->town.influence = k_TownReach;
		Locator::infoConstants::reset();
		Locator::infoConstants::emplace(*_info);
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto name : k_Players)
		{
			const auto player = registry.Create();
			registry.Assign<components::Player>(player, components::Player {.name = name});
			registry.Assign<components::PlayerInfluencePower>(player);
			registry.Assign<components::PlayerStatistics>(player);
			_players.push_back(player);
		}
		// Two towns of the second player and one of the first
		MakeTown(PlayerNames::PLAYER_TWO);
		MakeTown(PlayerNames::PLAYER_TWO);
		MakeTown(PlayerNames::PLAYER_ONE);
	}

	static entt::entity MakeTown(PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<components::Town>(town, components::Town {.owner = owner});
		return town;
	}

	static void MakeSource(PlayerNames player, float radius, bool anti)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto source = registry.Create();
		registry.Assign<components::InfluenceSource>(source, player, radius, anti, entt::null);
		registry.Assign<components::Transform>(source, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	}

	[[nodiscard]] static const components::PlayerInfluencePower& PowerOf(entt::entity player)
	{
		return Locator::entitiesRegistry::value().Get<const components::PlayerInfluencePower>(player);
	}

	std::vector<entt::entity> _players;
};

TEST_F(PowerAtLoad, ACitadelAndItsTownsAtTheirBaseGiveThePlayersPower)
{
	_influence.LandLaidOut();
	EXPECT_FLOAT_EQ(PowerOf(_players.at(0)).power, k_HeartReach + k_TownReach);
	EXPECT_FLOAT_EQ(PowerOf(_players.at(1)).power, k_HeartReach + (2.0f * k_TownReach));
	EXPECT_FLOAT_EQ(PowerOf(_players.at(2)).power, k_HeartReach);
}

TEST_F(PowerAtLoad, TheRunningAverageTakesOneStepFromNothing)
{
	_influence.LandLaidOut();
	EXPECT_FLOAT_EQ(PowerOf(_players.at(1)).average, 0.5f * (k_HeartReach + (2.0f * k_TownReach)));
}

TEST_F(PowerAtLoad, TheInfluenceHistoryTakesOneSample)
{
	_influence.LandLaidOut();
	const auto& registry = Locator::entitiesRegistry::value();
	for (const auto player : _players)
	{
		EXPECT_EQ(registry.Get<const components::PlayerStatistics>(player).influenceHistory.samplesInPoint, 1u);
	}
}

TEST_F(PowerAtLoad, APlayerWithoutACitadelIsLeftAlone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = registry.Create();
	registry.Assign<components::Player>(player, components::Player {.name = PlayerNames::PLAYER_FOUR});
	registry.Assign<components::PlayerInfluencePower>(player);
	registry.Assign<components::PlayerStatistics>(player);
	MakeTown(PlayerNames::PLAYER_FOUR);
	_influence.LandLaidOut();
	EXPECT_FLOAT_EQ(PowerOf(player).power, 0.0f);
	EXPECT_EQ(registry.Get<const components::PlayerStatistics>(player).influenceHistory.samplesInPoint, 0u);
}

TEST_F(PowerAtLoad, AnAntiInfluenceSourceAddsItsReachToThePower)
{
	constexpr float k_AntiReach = 7.0f;
	MakeSource(PlayerNames::PLAYER_THREE, k_AntiReach, true);
	_influence.LandLaidOut();
	EXPECT_FLOAT_EQ(PowerOf(_players.at(2)).power, k_HeartReach + k_AntiReach);
}

namespace
{
/// How far a town reaches after its turns
constexpr float k_GrownTownReach = 35.0f;
} // namespace

class StoredPower: public PowerAtLoad
{
protected:
	void SetUp() override
	{
		PowerAtLoad::SetUp();
		auto& registry = Locator::entitiesRegistry::value();
		_town = MakeTown(PlayerNames::PLAYER_FOUR);
		registry.Assign<components::TownInfluence>(_town, components::TownInfluence {.radius = k_GrownTownReach});
		_player = registry.Create();
		registry.Assign<components::Player>(_player, components::Player {.name = PlayerNames::PLAYER_FOUR});
		registry.Assign<components::PlayerInfluencePower>(_player);
		registry.Assign<components::PlayerStatistics>(_player);
	}

	entt::entity _town {entt::null};
	entt::entity _player {entt::null};
};

TEST_F(StoredPower, AQuestionBeforeTheFirstNoteGetsNoPower)
{
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_FOUR), 0.0f);
}

TEST_F(StoredPower, TheNoteSetsThePowerTheAverageAndOneSampleOfTheHistory)
{
	_influence.NoteInfluencePower(PlayerNames::PLAYER_FOUR);
	EXPECT_FLOAT_EQ(PowerOf(_player).power, k_GrownTownReach);
	EXPECT_FLOAT_EQ(PowerOf(_player).average, 0.5f * k_GrownTownReach);
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_FOUR), k_GrownTownReach);
	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<const components::PlayerStatistics>(_player).influenceHistory.samplesInPoint, 1u);
}

TEST_F(StoredPower, AQuestionGetsTheNotedPowerUntilTheNextNote)
{
	_influence.NoteInfluencePower(PlayerNames::PLAYER_FOUR);
	auto& registry = Locator::entitiesRegistry::value();
	constexpr float k_LaterReach = 60.0f;
	registry.Get<components::TownInfluence>(_town).radius = k_LaterReach;
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_FOUR), k_GrownTownReach);
	_influence.NoteInfluencePower(PlayerNames::PLAYER_FOUR);
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_FOUR), k_LaterReach);
	EXPECT_FLOAT_EQ(PowerOf(_player).average, (0.5f * k_GrownTownReach + k_LaterReach) * 0.5f);
}

TEST_F(StoredPower, TheNoteOfOnePlayerLeavesTheOthersAlone)
{
	_influence.NoteInfluencePower(PlayerNames::PLAYER_FOUR);
	for (const auto player : _players)
	{
		EXPECT_FLOAT_EQ(PowerOf(player).power, 0.0f);
	}
}

TEST_F(StoredPower, TheCitadelTheTownsAndTheSourcesAddUp)
{
	constexpr float k_SourceReach = 5.0f;
	constexpr float k_AntiReach = 3.0f;
	MakeSource(PlayerNames::PLAYER_TWO, k_SourceReach, false);
	MakeSource(PlayerNames::PLAYER_TWO, k_AntiReach, true);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const components::Town>([&registry](entt::entity town, const components::Town& each) {
		if (each.owner == PlayerNames::PLAYER_TWO)
		{
			registry.Assign<components::TownInfluence>(town, components::TownInfluence {.radius = k_TownReach});
		}
	});
	_influence.NoteInfluencePower(PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(PowerOf(_players.at(1)).power, k_HeartReach + (2.0f * k_TownReach) + k_SourceReach + k_AntiReach);
}

TEST_F(StoredPower, TheTownsAddUpInTheOrderOfThePlayersTownList)
{
	// A small reach after a large one is lost, so only the list order gives this sum. The large town is made between
	// the two small ones but comes last in the list.
	constexpr float k_Large = 16777216.0f;
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<components::TownInfluence>(_town).radius = 1.0f;
	const auto large = MakeTown(PlayerNames::PLAYER_FOUR);
	registry.Get<components::Town>(large).gained = 2;
	registry.Assign<components::TownInfluence>(large, components::TownInfluence {.radius = k_Large});
	const auto small = MakeTown(PlayerNames::PLAYER_FOUR);
	registry.Get<components::Town>(small).gained = 1;
	registry.Assign<components::TownInfluence>(small, components::TownInfluence {.radius = 1.0f});
	_influence.NoteInfluencePower(PlayerNames::PLAYER_FOUR);
	EXPECT_EQ(PowerOf(_player).power, k_Large + 2.0f);
}

TEST(InfluencePowerOf, AddsTheCitadelThenEachTownThenEachSourceInTurn)
{
	// Each sum is a float, so a small reach after a large one is lost, as in the game
	constexpr float k_Large = 16777216.0f;
	const std::array towns {1.0f, 1.0f};
	EXPECT_EQ(systems::influence_rules::InfluencePowerOf(k_Large, towns, {}), k_Large);
	EXPECT_EQ(systems::influence_rules::InfluencePowerOf(0.0f, towns, towns), 4.0f);
}
