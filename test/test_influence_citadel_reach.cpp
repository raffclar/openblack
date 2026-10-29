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
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_THREE), k_HeartReach);
	EXPECT_GT(_influence.PlayerRawInfluence(PlayerNames::PLAYER_THREE, glm::vec3(2000.0f, 0.0f, 1000.0f)), 0.0f);
	EXPECT_FALSE(AnyReachFixed());
}

TEST_F(CitadelReach, TheTurnFixesTheReachesInTheOrderOfThePlayersWhateverWasAskedBefore)
{
	// One computer asks about the third player first, another computer asks nothing. Both must keep the same world.
	static_cast<void>(_influence.InfluencePower(PlayerNames::PLAYER_THREE));
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
	const float before = _influence.InfluencePower(PlayerNames::PLAYER_TWO);
	_influence.FixCitadelReaches();
	EXPECT_FLOAT_EQ(_influence.InfluencePower(PlayerNames::PLAYER_TWO), before);
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

	static void MakeTown(PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto town = registry.Create();
		registry.Assign<components::Town>(town, components::Town {.owner = owner});
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
