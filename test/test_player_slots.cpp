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

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <set>
#include <vector>

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

TEST(PlayerColour, ALaterGodTakesTheChatColourOfItsSlot)
{
	// The eighth god (number 8, slot 7) and the last god (number 32, slot 31) have the chat colours of their slots
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(8)), player_colour::k_GodChatColours.at(7));
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(32)), player_colour::k_GodChatColours.at(31));
	EXPECT_EQ(player_colour::OnLand(static_cast<PlayerNames>(14), 0), player_colour::k_GodChatColours.at(13) | 0xFF000000u);
	EXPECT_FALSE(player_colour::LaterGodColour(PlayerNames::PLAYER_SEVEN).has_value());
	EXPECT_FALSE(player_colour::LaterGodColour(PlayerNames::NEUTRAL).has_value());
	// A number past the gods takes the colour of its low bits
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(33)), ecs::components::Player::k_Colours.at(1));
	constexpr std::array<uint32_t, k_LandSlots> k_Table {1, 2, 3, 4, 5, 6, 7, 8};
	EXPECT_EQ(player_colour::FromTable(k_Table, PlayerNames::PLAYER_TWO), 2u);
	EXPECT_EQ(player_colour::FromTable(k_Table, static_cast<PlayerNames>(9)),
	          player_colour::k_GodChatColours.at(8) & 0xFFFFFFu);
}

TEST(PlayerSlots, TheWalksOfAGameTakeItsPlayersInOrder)
{
	const auto land = Players(k_LandSlots);
	EXPECT_EQ(std::ranges::distance(land), 8);
	EXPECT_EQ(*std::ranges::prev(land.end()), PlayerNames::NEUTRAL);
	std::vector<PlayerNames> gods;
	std::ranges::copy(Gods(10), std::back_inserter(gods));
	const std::vector<PlayerNames> expected {
	    PlayerNames::PLAYER_ONE,   PlayerNames::PLAYER_TWO,     PlayerNames::PLAYER_THREE,
	    PlayerNames::PLAYER_FOUR,  PlayerNames::PLAYER_FIVE,    PlayerNames::PLAYER_SIX,
	    PlayerNames::PLAYER_SEVEN, static_cast<PlayerNames>(8), static_cast<PlayerNames>(9)};
	EXPECT_EQ(gods, expected);
	// A count is never less than a land's or more than the numbers of the largest game
	EXPECT_EQ(PlayerNumbersOf(0), k_LandSlots);
	EXPECT_EQ(PlayerNumbersOf(100), k_PlayerNumbers);
}

TEST(PlayerSlots, TheScriptsNameTheLaterGodsByTheirNumbers)
{
	EXPECT_EQ(ScriptNameOf(PlayerNames::PLAYER_ONE), "PLAYER_ONE");
	EXPECT_EQ(ScriptNameOf(PlayerNames::NEUTRAL), "NEUTRAL");
	EXPECT_EQ(ScriptNameOf(static_cast<PlayerNames>(8)), "PLAYER_8");
	EXPECT_EQ(ScriptNameOf(static_cast<PlayerNames>(32)), "PLAYER_32");
	for (size_t number = 0; number < k_PlayerNumbers; ++number)
	{
		const auto player = static_cast<PlayerNames>(number);
		EXPECT_EQ(PlayerOfScriptName(ScriptNameOf(player)), player) << number;
	}
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_7").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_33").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_08").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_9X").has_value());
	EXPECT_FALSE(PlayerOfScriptName("GOD_9").has_value());
}
