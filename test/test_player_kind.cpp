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

TEST(PlayerKind, InAGameAloneTheComputerPlaysOnlyThePlayersOfItsMinds)
{
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_ONE}));
	// Nobody plays a player with no mind: its creature is not of a computer player
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_TWO}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_SEVEN}));
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_TWO, .computerMind = true}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::NEUTRAL}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::NEUTRAL, .computerMind = true}));
}

TEST(PlayerKind, InANetworkGameTheComputerPlaysOnlyThePlayersOfItsMinds)
{
	// A person at another computer plays the second player
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_TWO, .networkGame = true}));
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_THREE, .networkGame = true, .computerMind = true}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_ONE, .networkGame = true}));
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::NEUTRAL, .networkGame = true, .computerMind = true}));
}

TEST(PlayerKind, AMindThatTheGameSwitchedOnMakesAComputerPlayerAlsoOfTheFirstPlayerInAGameAlone)
{
	// The AI soak gives the first player a computer mind: the computer then plays that player
	EXPECT_TRUE(player_kind::IsComputer({.player = PlayerNames::PLAYER_ONE, .computerMind = true}));
	EXPECT_FALSE(player_kind::IsHuman({.player = PlayerNames::PLAYER_ONE, .computerMind = true}));
	EXPECT_EQ(player_kind::KindOf({.player = PlayerNames::PLAYER_ONE, .computerMind = true}), player_kind::Kind::Computer);
	EXPECT_EQ(player_kind::KindOf({.player = PlayerNames::PLAYER_TWO, .computerMind = true}), player_kind::Kind::Computer);
}

TEST(PlayerKind, OnlyThePersonsPlayerIsHumanAndAComputerGodWithHandsIsNot)
{
	// In a game alone, the person plays only the first player. Hands do not make a human.
	EXPECT_TRUE(player_kind::IsHuman({.player = PlayerNames::PLAYER_ONE, .hands = true}));
	EXPECT_FALSE(player_kind::IsHuman({.player = PlayerNames::PLAYER_TWO, .hands = true}));
	EXPECT_EQ(player_kind::KindOf({.player = PlayerNames::PLAYER_TWO}), player_kind::Kind::None);
	// In a network game, a person plays each player with hands that no mind plays
	EXPECT_TRUE(player_kind::IsHuman({.player = PlayerNames::PLAYER_TWO, .networkGame = true, .hands = true}));
	EXPECT_FALSE(
	    player_kind::IsHuman({.player = PlayerNames::PLAYER_TWO, .networkGame = true, .computerMind = true, .hands = true}));
	EXPECT_EQ(player_kind::KindOf({.player = PlayerNames::PLAYER_THREE, .networkGame = true}), player_kind::Kind::None);
	EXPECT_FALSE(player_kind::IsComputer({.player = PlayerNames::PLAYER_THREE, .networkGame = true}));
	// Nobody plays the neutral player
	EXPECT_EQ(player_kind::KindOf({.player = PlayerNames::NEUTRAL, .computerMind = true, .hands = true}),
	          player_kind::Kind::None);
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
