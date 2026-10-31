/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Common/WindowsText.h"
#include "Creature/ClanCreatureOrder.h"

using namespace openblack;

namespace
{

std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}

} // namespace

TEST(ClanCreatureOrder, TheOrderGivesTheRowAndTheName)
{
	using namespace std::string_view_literals;
	const auto order = clan_creature_order::Read(Bytes("CLANCREATECREATURE05Rex\0more"sv));
	ASSERT_TRUE(order.has_value());
	EXPECT_EQ(order->row, 5);
	EXPECT_EQ(order->name, u"Rex");
}

TEST(ClanCreatureOrder, TheTextOfTheOrderHasNoCase)
{
	const auto order = clan_creature_order::Read(Bytes("ClanCreateCreature16Gorilla"));
	ASSERT_TRUE(order.has_value());
	EXPECT_EQ(order->row, 16);
	EXPECT_EQ(order->name, u"Gorilla");
}

TEST(ClanCreatureOrder, DataWithAnotherStartIsNoOrder)
{
	EXPECT_FALSE(clan_creature_order::Read(Bytes("LHCreatureMind")).has_value());
	EXPECT_FALSE(clan_creature_order::Read(Bytes("CLANCREATECREATUR")).has_value());
	EXPECT_FALSE(clan_creature_order::Read({}).has_value());
	EXPECT_FALSE(clan_creature_order::Read(Bytes(" CLANCREATECREATURE05Rex")).has_value());
}

TEST(ClanCreatureOrder, TheDigitsReadAsTheCFunctionAtoiReadsThem)
{
	EXPECT_EQ(clan_creature_order::Read(Bytes("CLANCREATECREATURE 7Ape"))->row, 7);
	EXPECT_EQ(clan_creature_order::Read(Bytes("CLANCREATECREATURE-3Ape"))->row, -3);
	EXPECT_EQ(clan_creature_order::Read(Bytes("CLANCREATECREATURE+4Ape"))->row, 4);
	EXPECT_EQ(clan_creature_order::Read(Bytes("CLANCREATECREATURE9xApe"))->row, 9);
	// No digit gives 0, the row of the Giant Ape
	EXPECT_EQ(clan_creature_order::Read(Bytes("CLANCREATECREATUREabApe"))->row, 0);
	// Only two bytes count: the third digit is the start of the name
	const auto order = clan_creature_order::Read(Bytes("CLANCREATECREATURE123"));
	EXPECT_EQ(order->row, 12);
	EXPECT_EQ(order->name, u"3");
}

TEST(ClanCreatureOrder, AShortOrderGivesRowZeroAndNoName)
{
	using namespace std::string_view_literals;
	const auto bare = clan_creature_order::Read(Bytes("CLANCREATECREATURE"));
	ASSERT_TRUE(bare.has_value());
	EXPECT_EQ(bare->row, 0);
	EXPECT_TRUE(bare->name.empty());
	// A zero byte stops the digits, and the name starts after the two places of the digits
	const auto stopped = clan_creature_order::Read(Bytes("CLANCREATECREATURE\0"
	                                                     "5Rex"sv));
	EXPECT_EQ(stopped->row, 0);
	EXPECT_EQ(stopped->name, u"Rex");
}

TEST(ClanCreatureOrder, TheNameIsInCodePage1252AndKeepsItsFirst63Characters)
{
	const auto accents = clan_creature_order::Read(Bytes("CLANCREATECREATURE03\xE9\x80\x81"));
	EXPECT_EQ(accents->name, u"\u00E9\u20AC\u0081");
	const auto longName = clan_creature_order::Read(Bytes("CLANCREATECREATURE03" + std::string(70, 'a')));
	EXPECT_EQ(longName->name, std::u16string(clan_creature_order::k_MostNameCharacters, u'a'));
}

TEST(WindowsText, TextGoesToTheCodePageAndBack)
{
	EXPECT_EQ(windows_text::ToWindowsText(u"\u00E9\u20AC\u2019\u4E00"), "\xE9\x80\x92?");
	EXPECT_EQ(windows_text::FromWindowsText("\xE9\x80\x92"), u"\u00E9\u20AC\u2019");
	EXPECT_EQ(windows_text::FromWindowsText("Rex"), u"Rex");
	// A byte that the code page does not use gives the character of the same number
	EXPECT_EQ(windows_text::FromWindowsText("\x8D"), u"\u008D");
}
