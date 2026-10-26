/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>

#include <Gui/MultiplayerFrontEndRules.h>
#include <Gui/PingDisplay.h>
#include <gtest/gtest.h>

using namespace openblack::gui;

namespace
{

constexpr multiplayer::RosterTexts k_Texts {.god = u"God %d", .observers = u"Observers", .current = u" (CURRENT)"};

multiplayer::Seat God(int32_t god, int64_t id, const char* name)
{
	return {.account = {.id = id, .name = name}, .god = god, .clan = std::nullopt};
}

} // namespace

TEST(PingDisplay, TheTextIsTheGamesOwnWordsWithWholeMilliseconds)
{
	EXPECT_EQ(ping_display::k_Format, u"ping %d");
	EXPECT_EQ(ping_display::Text(48'000), u"ping 48");
	EXPECT_EQ(ping_display::Text(48'999), u"ping 48");
	EXPECT_EQ(ping_display::Text(999), u"ping 0");
	EXPECT_EQ(ping_display::Text(0), u"ping 0");
	EXPECT_EQ(ping_display::Text(1'234'567), u"ping 1234");
}

TEST(PingDisplay, NoAnswerYetGivesNoText)
{
	EXPECT_FALSE(ping_display::Milliseconds(std::nullopt).has_value());
	EXPECT_FALSE(ping_display::Text(std::nullopt).has_value());
}

TEST(PingDisplay, TheNumberStopsAtTheLargestValueThatTheTextCanShow)
{
	EXPECT_EQ(ping_display::Milliseconds(UINT64_MAX), INT32_MAX);
	EXPECT_EQ(ping_display::Milliseconds(uint64_t {INT32_MAX} * 1000), INT32_MAX);
	EXPECT_EQ(ping_display::Text(UINT64_MAX), u"ping 2147483647");
}

TEST(PingDisplay, ThePingComesAfterTheOwnNameWithOneSpace)
{
	EXPECT_EQ(ping_display::AfterOwnName(u"God 2: Alice (CURRENT)", u"ping 48"), u"God 2: Alice (CURRENT) ping 48");
	EXPECT_EQ(ping_display::AfterOwnName(u"God 2: Alice (CURRENT)", std::nullopt), u"God 2: Alice (CURRENT)");
}

TEST(PingDisplay, OnlyTheOwnLineOfTheRoomShowsThePing)
{
	const std::optional<std::u16string> ping = u"ping 48";
	EXPECT_EQ(multiplayer::RosterLine(God(1, 3, "Bob"), u"Bob", false, k_Texts, ping), u"God 1: Bob");
	EXPECT_EQ(multiplayer::RosterLine(God(2, 7, "Alice"), u"Alice", true, k_Texts, ping), u"God 2: Alice (CURRENT) ping 48");
	EXPECT_EQ(multiplayer::RosterLine(God(0, 9, "Eve"), u"Eve", false, k_Texts, ping), u"Observers: Eve");
	EXPECT_EQ(multiplayer::RosterLine(God(0, 9, "Eve"), u"Eve", true, k_Texts, ping), u"Observers: Eve (CURRENT) ping 48");
}

TEST(PingDisplay, WithTheDisplayOffTheRoomLinesStayAsTheGameShowsThem)
{
	EXPECT_EQ(multiplayer::RosterLine(God(2, 7, "Alice"), u"Alice", true, k_Texts, std::nullopt), u"God 2: Alice (CURRENT)");
	EXPECT_EQ(multiplayer::RosterLine(God(1, 3, "Bob"), u"Bob", false, k_Texts, std::nullopt), u"God 1: Bob");
}
