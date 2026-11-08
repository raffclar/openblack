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
#include <ios>
#include <iterator>
#include <ranges>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "ColourDifference.h"
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
	EXPECT_EQ(k_PlayerNumbers, 34u);
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
	EXPECT_EQ(PlayerOfSlot(32), static_cast<PlayerNames>(33));
	EXPECT_FALSE(PlayerOfSlot(33).has_value());
	EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(8)), 7u);
	EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(32)), 31u);
	EXPECT_EQ(SlotOfPlayer(static_cast<PlayerNames>(33)), 32u);
	EXPECT_FALSE(SlotOfPlayer(static_cast<PlayerNames>(34)).has_value());
}

// The observers are the humans of the god after the last god slot of the land, and at least of player seven
TEST(PlayerSlots, TheGodOfTheObserversComesAfterTheLastGodSlot)
{
	EXPECT_EQ(ObserversGodOf(0), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(ObserversGodOf(2), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(ObserversGodOf(4), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(ObserversGodOf(6), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(ObserversGodOf(7), static_cast<PlayerNames>(8));
	EXPECT_EQ(ObserversGodOf(31), static_cast<PlayerNames>(32));
	EXPECT_EQ(ObserversGodOf(32), static_cast<PlayerNames>(33));
	EXPECT_EQ(ObserversSlotOf(32), k_MaxPlayers);
	EXPECT_TRUE(IsPlayerNumber(ObserversGodOf(32)));
	EXPECT_EQ(SlotOfPlayer(ObserversGodOf(32)), k_MaxPlayers);
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
	EXPECT_TRUE(IsPlayerNumber(static_cast<PlayerNames>(33)));
	EXPECT_FALSE(IsPlayerNumber(static_cast<PlayerNames>(34)));
}

TEST(PlayerColour, ThePlayersOfALandKeepTheirColours)
{
	const auto& colours = ecs::components::Player::k_Colours;
	for (size_t number = 0; number < k_LandSlots; ++number)
	{
		EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(number)), colours.at(number));
	}
}

TEST(PlayerColour, ALaterGodTakesTheLandColourOfItsSlot)
{
	// The eighth god (number 8, slot 7) and the last god (number 32, slot 31) have the land colours of their slots
	const auto& colours = player_colour::k_LaterGodLandColours;
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(8)), colours.front());
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(32)), colours.back());
	EXPECT_EQ(player_colour::OnLand(static_cast<PlayerNames>(14), 0), colours.at(6) | 0xFF000000u);
	EXPECT_FALSE(player_colour::LaterGodColour(PlayerNames::PLAYER_SEVEN).has_value());
	EXPECT_FALSE(player_colour::LaterGodColour(PlayerNames::NEUTRAL).has_value());
	// A number past the gods takes the colour of its low bits
	EXPECT_EQ(player_colour::Of(static_cast<PlayerNames>(33)), ecs::components::Player::k_Colours.at(1));
	constexpr std::array<uint32_t, k_LandSlots> k_Table {1, 2, 3, 4, 5, 6, 7, 8};
	EXPECT_EQ(player_colour::FromTable(k_Table, PlayerNames::PLAYER_TWO), 2u);
	EXPECT_EQ(player_colour::FromTable(k_Table, static_cast<PlayerNames>(9)), colours.at(1) & 0xFFFFFFu);
}

TEST(PlayerColour, TheSecondLandShowsLethysInYellowAndKhazarInBlue)
{
	// The world shows each god of a land of the story in the colour that the land gives it
	const auto& colours = ecs::components::Player::k_Colours;
	constexpr int32_t k_SecondLand = 2;
	EXPECT_EQ(player_colour::ColourPlayerOnLand(PlayerNames::PLAYER_ONE, k_SecondLand), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_TWO, k_SecondLand), 0xFFFFFD47u);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_THREE, k_SecondLand), 0xFF4777FFu);
	constexpr std::array<uint32_t, k_LandSlots> k_Table {1, 2, 3, 4, 5, 6, 7, 8};
	EXPECT_EQ(player_colour::FromTableOnLand(k_Table, PlayerNames::PLAYER_TWO, k_SecondLand), 5u);
	EXPECT_EQ(player_colour::FromTableOnLand(k_Table, PlayerNames::PLAYER_THREE, k_SecondLand), 6u);
	EXPECT_EQ(colours.at(4), 0xFFFFFD47u);
	EXPECT_EQ(colours.at(5), 0xFF4777FFu);
}

TEST(PlayerColour, EachLandOfTheStoryGivesItsComputerGodsTheirColours)
{
	using player_colour::ColourPlayerOnLand;
	// The first, fourth and fifth lands show the second player in orange; the third land shows it in blue
	for (const int32_t land : {1, 4, 5})
	{
		EXPECT_EQ(ColourPlayerOnLand(PlayerNames::PLAYER_TWO, land), PlayerNames::PLAYER_SEVEN) << land;
		EXPECT_EQ(ColourPlayerOnLand(PlayerNames::PLAYER_THREE, land), PlayerNames::PLAYER_THREE) << land;
	}
	EXPECT_EQ(ColourPlayerOnLand(PlayerNames::PLAYER_TWO, 3), PlayerNames::PLAYER_SIX);
	EXPECT_EQ(ColourPlayerOnLand(PlayerNames::PLAYER_THREE, 3), PlayerNames::PLAYER_THREE);
	// A network land and any other land keep the colour of each player number
	for (const int32_t land : {0, 6})
	{
		for (size_t number = 0; number < k_LandSlots; ++number)
		{
			const auto player = static_cast<PlayerNames>(number);
			EXPECT_EQ(ColourPlayerOnLand(player, land), player);
			EXPECT_EQ(player_colour::OnLand(player, land), player_colour::Of(player));
		}
	}
}

TEST(PlayerColour, TheLandColoursOfTheLaterGodsAreFarFromTheOtherColours)
{
	// The rule of deviations 116 and 164: a CIE L*a*b* difference of 25 or more, and a contrast of 4.5 to 1 or more with
	// black
	constexpr double k_LeastDifference = 25.0;
	constexpr double k_LeastContrast = 4.5;
	const auto& later = player_colour::k_LaterGodLandColours;
	// The land colours of the first 7 gods, the black of the neutral player and the white of its influence
	std::vector<uint32_t> colours(ecs::components::Player::k_Colours.begin(), ecs::components::Player::k_Colours.end());
	colours.push_back(0xFFFFFFFFu);
	colours.insert(colours.end(), later.begin(), later.end());
	for (size_t i = 0; i < colours.size(); ++i)
	{
		for (size_t j = i + 1; j < colours.size(); ++j)
		{
			EXPECT_GE(test_colour::Difference(colours[i], colours[j]), k_LeastDifference)
			    << std::hex << colours[i] << " and " << colours[j];
		}
	}
	for (size_t index = 0; index < later.size(); ++index)
	{
		const auto slot = index + k_LandGods;
		EXPECT_EQ(later.at(index) >> 24u, 0xFFu) << "slot " << slot;
		EXPECT_GE(test_colour::ContrastWithBlack(later.at(index)), k_LeastContrast) << "slot " << slot;
		// The land colour of a god differs from its chat colour
		EXPECT_GE(test_colour::Difference(later.at(index), player_colour::k_GodChatColours.at(slot)), k_LeastDifference)
		    << "slot " << slot;
	}
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
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_34").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_08").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_").has_value());
	EXPECT_FALSE(PlayerOfScriptName("PLAYER_9X").has_value());
	EXPECT_FALSE(PlayerOfScriptName("GOD_9").has_value());
}
