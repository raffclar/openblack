/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <stdexcept>

#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/PlayerSystem.h"

using namespace openblack;
using namespace openblack::ecs;

class PlayerSystemLands: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<components::Player>(entity, name);
		return entity;
	}

	/// The land goes, its players with it, as a new land is loaded
	static void ClearLand() { Locator::entitiesRegistry::value().Reset(); }

	systems::PlayerSystem _players;
};

TEST_F(PlayerSystemLands, KnowsTheLandsPlayers)
{
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
}

TEST_F(PlayerSystemLands, TheFirstPlayerMadeKeepsItsName)
{
	// The land is set up with its player added, then every player on it registered
	const auto added = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(added);
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), added);
}

TEST_F(PlayerSystemLands, ANewLandsPlayersTakeTheLastLandsPlaces)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	ClearLand();
	const auto one = MakePlayer(PlayerNames::PLAYER_ONE);
	_players.AddPlayer(one);
	const auto two = MakePlayer(PlayerNames::PLAYER_TWO);
	_players.RegisterPlayers();

	const auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_ONE), one);
	EXPECT_EQ(_players.GetPlayer(PlayerNames::PLAYER_TWO), two);
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_ONE)));
	EXPECT_TRUE(registry.Valid(_players.GetPlayer(PlayerNames::PLAYER_TWO)));
}

TEST_F(PlayerSystemLands, APlayerNotOnTheNewLandIsForgotten)
{
	MakePlayer(PlayerNames::PLAYER_THREE);
	_players.RegisterPlayers();

	ClearLand();
	MakePlayer(PlayerNames::PLAYER_ONE);
	_players.RegisterPlayers();

	EXPECT_THROW(static_cast<void>(_players.GetPlayer(PlayerNames::PLAYER_THREE)), std::out_of_range);
}
