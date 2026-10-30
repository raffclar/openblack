/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <set>

#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/PlayerColour.h"
#include "PlayerSlots.h"

using namespace openblack;
using namespace openblack::player_slots;

TEST(PlayerSlots, TheNeutralPlayerKeepsItsNumberOnEveryLand)
{
	EXPECT_EQ(static_cast<size_t>(PlayerNames::NEUTRAL), 7u);
	EXPECT_EQ(k_LandSlots, 8u);
	EXPECT_EQ(k_LandGods, 7u);
	EXPECT_EQ(k_PlayerNumbers, 33u);
	EXPECT_FALSE(SlotOfPlayer(PlayerNames::NEUTRAL).has_value());
}

TEST(PlayerSlots, TheGodsOfALandHaveTheSlotsOfTheirNumbers)
{
	for (size_t slot = 0; slot < k_LandGods; ++slot)
	{
		ASSERT_TRUE(PlayerOfSlot(slot).has_value());
		EXPECT_EQ(static_cast<size_t>(*PlayerOfSlot(slot)), slot);
		EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(slot)), slot);
	}
}

TEST(PlayerSlots, TheLaterGodsTakeTheNumbersAfterTheNeutralPlayer)
{
	EXPECT_EQ(PlayerOfSlot(7), static_cast<PlayerNames>(8));
	EXPECT_EQ(PlayerOfSlot(31), static_cast<PlayerNames>(32));
	EXPECT_FALSE(PlayerOfSlot(32).has_value());
	EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(8)), 7u);
	EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(32)), 31u);
	EXPECT_FALSE(SlotOfPlayer(static_cast<PlayerNames>(33)).has_value());
}

TEST(PlayerSlots, EachSlotHasItsOwnPlayerAndComesBack)
{
	std::set<PlayerNames> players;
	for (size_t slot = 0; slot < k_MaxPlayers; ++slot)
	{
		const auto player = PlayerOfSlot(slot);
		ASSERT_TRUE(player.has_value());
		EXPECT_NE(*player, PlayerNames::NEUTRAL);
		EXPECT_TRUE(IsPlayerNumber(*player));
		EXPECT_EQ(SlotOfPlayer(*player), slot);
		players.insert(*player);
	}
	EXPECT_EQ(players.size(), k_MaxPlayers);
}

TEST(PlayerSlots, OnlyTheNumbersOfALandAreLandPlayers)
{
	EXPECT_TRUE(IsLandPlayer(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(IsLandPlayer(PlayerNames::NEUTRAL));
	EXPECT_FALSE(IsLandPlayer(static_cast<PlayerNames>(8)));
	EXPECT_TRUE(IsPlayerNumber(static_cast<PlayerNames>(32)));
	EXPECT_FALSE(IsPlayerNumber(static_cast<PlayerNames>(33)));
}

TEST(PlayerColour, ThePlayersOfALandKeepTheirColours)
{
	const auto& colours = ecs::components::Player::k_Colours;
	for (size_t number = 0; number < k_LandSlots; ++number)
	{
		EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(number)), colours.at(number));
	}
}

TEST(PlayerColour, ALaterGodTakesTheColourOfItsLowBits)
{
	const auto& colours = ecs::components::Player::k_Colours;
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(8)), colours.at(0));
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(32)), colours.at(0));
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(14)), colours.at(6));
}
