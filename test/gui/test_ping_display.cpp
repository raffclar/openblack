/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <string>

#include <Creature/CreatureFightHud.h>
#include <Creature/CreatureStatusPanel.h>
#include <Gui/MultiplayerFrontEndRules.h>
#include <Gui/PingDisplay.h>
#include <Gui/ScreenFit.h>
#include <glm/vec2.hpp>
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

TEST(PingDisplay, TheLineIsInTheTopLeftCornerOfTheDefaultScreen)
{
	EXPECT_EQ(ping_display::Origin({800, 600}), glm::vec2(0.0f));
	EXPECT_EQ(ping_display::TextCorner(), glm::ivec2(5, 1));
}

TEST(PingDisplay, TheLineStaysInTheTopLeftCornerOfEveryScreen)
{
	constexpr std::array<glm::u16vec2, 7> k_Screens {{
	    {640, 480},
	    {800, 600},
	    {1280, 1024},
	    {1920, 1080},
	    {2560, 1440},
	    {3440, 1440},
	    {5120, 1440},
	}};
	for (const auto screen : k_Screens)
	{
		const auto space = FrontEndSpace(screen.x, screen.y);
		const auto origin = ping_display::Origin(screen);
		// The corner of the layout is on the corner of the screen
		EXPECT_EQ(space.ToScreen(glm::vec2(0.0f)) + origin, glm::vec2(0.0f)) << screen.x << "x" << screen.y;
		// The text starts 5 units from the left edge, at the scale of the front end
		const auto text = space.ToScreen(glm::vec2(ping_display::TextCorner())) + origin;
		EXPECT_FLOAT_EQ(text.x, 5.0f * space.scale) << screen.x << "x" << screen.y;
		// The bottom of the line is above the creature's status panel with its reward row, a quarter down the screen.
		// The panel without the reward row starts above the bottom of the line, so the line does not show with it.
		const auto bottom = (space.ToScreen({0.0f, static_cast<float>(ping_display::k_LineHeight)}) + origin).y;
		const auto withReward = openblack::creature_panel::Compute(screen, true, 0.0f);
		EXPECT_LE(bottom, withReward.box.min.y) << screen.x << "x" << screen.y;
		const auto nearTop = openblack::creature_panel::Compute(screen, false, 0.0f);
		EXPECT_GT(bottom, nearTop.box.min.y) << screen.x << "x" << screen.y;
	}
}

TEST(PingDisplay, TheFightPanelCoversTheCornerOfTheLine)
{
	// The fight panel's shade starts in the top left corner of the screen, where the text of the line is
	const auto fight = openblack::creature_fight_hud::Compute({800, 600});
	const auto text = glm::vec2(ping_display::TextCorner());
	EXPECT_LE(fight.box.min.x, text.x);
	EXPECT_LE(fight.box.min.y, text.y);
	EXPECT_GT(fight.box.max.x, text.x);
	EXPECT_GT(fight.box.max.y, text.y);
}

TEST(PingDisplay, TheLineShowsOnlyOnTheGameScreen)
{
	constexpr ping_display::LineFacts k_Shown {.hasText = true};
	EXPECT_TRUE(ping_display::LineShows(k_Shown));
	EXPECT_FALSE(ping_display::LineShows({}));
	auto facts = k_Shown;
	facts.menuOpen = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
	facts = k_Shown;
	facts.inTemple = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
	facts = k_Shown;
	facts.videoPlays = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
	facts = k_Shown;
	facts.cinemaBars = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
	facts = k_Shown;
	facts.fightPanel = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
	facts = k_Shown;
	facts.creaturePanelNearTop = true;
	EXPECT_FALSE(ping_display::LineShows(facts));
}
