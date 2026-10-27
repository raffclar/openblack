/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/HandPlayer.h>
#include <ECS/PlayerKind.h>
#include <ECS/Registry.h>
#include <gtest/gtest.h>

using namespace openblack;

TEST(PlayerKind, InAGameAloneThePersonPlaysTheFirstPlayerAndTheComputerTheOthers)
{
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_ONE}));
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_TWO}));
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_SEVEN}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::NEUTRAL}));
}

TEST(PlayerKind, InANetworkGameTheComputerPlaysOnlyThePlayersOfItsMinds)
{
	// A person at another computer plays the second player
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_TWO, .networkGame = true}));
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_THREE, .networkGame = true, .computerMind = true}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_ONE, .networkGame = true}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::NEUTRAL, .networkGame = true, .computerMind = true}));
}

TEST(PlayerKind, AHandWithTheMarkOfAGodIsOfThatGodAndAHandWithNoMarkIsOfTheFirstPlayer)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	const auto third = registry.Create();
	registry.Assign<ecs::components::GodHand>(third, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(ecs::HandPlayer(registry, first), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::HandPlayer(registry, third), PlayerNames::PLAYER_THREE);
	// A hand that is gone has no mark
	EXPECT_EQ(ecs::HandPlayer(registry, entt::null), PlayerNames::PLAYER_ONE);
}
