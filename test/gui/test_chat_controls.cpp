/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"
#include "Gui/ChatControls.h"
#include "Gui/ChatControlsRules.h"

using namespace openblack;
using namespace openblack::gui;
namespace rules = openblack::gui::chat_controls;

TEST(ChatControlsRules, ThePlayerListIsAsHighAsItsItemsUpToHalfOfTheScreen)
{
	EXPECT_EQ(rules::PlayerListRect(800, 600, 0).min, glm::ivec2(670, 20));
	EXPECT_EQ(rules::PlayerListRect(800, 600, 0).Height(), 10);
	EXPECT_EQ(rules::PlayerListRect(800, 600, 40).Height(), 48);
	EXPECT_EQ(rules::PlayerListRect(800, 600, 1000).Height(), 300);
}

TEST(ChatControlsRules, TheQuickListIsAtMost520Wide)
{
	EXPECT_EQ(rules::QuickListRect(800, 600).min, glm::ivec2(280, 20));
	EXPECT_EQ(rules::QuickListRect(800, 600).Height(), 300);
	// A narrow screen keeps 50 units at the left
	EXPECT_EQ(rules::QuickListRect(500, 400).min, glm::ivec2(50, 20));
	EXPECT_EQ(rules::QuickListRect(570, 400).min, glm::ivec2(50, 20));
	EXPECT_EQ(rules::QuickListRect(571, 400).min, glm::ivec2(51, 20));
}

TEST(ChatControlsRules, TheLineIsAtTheLeftOfTheSendButton)
{
	const auto edit = rules::EditRect(800, 90);
	EXPECT_EQ(edit.min, glm::ivec2(690, 0));
	EXPECT_EQ(edit.max, glm::ivec2(780, 20));
	EXPECT_EQ(rules::QuickButtonRect(edit).min, glm::ivec2(670, 0));
	EXPECT_EQ(rules::CornerButtonRect(800).min, glm::ivec2(780, 0));
}

TEST(ChatControlsRules, ThePicturesComeFromTheCellsOfTheButtons)
{
	const auto speech = rules::PictureOf(rules::Look::Speech, false);
	EXPECT_FLOAT_EQ(speech.uvMin.x, 0.125f + (1.0f / 512.0f));
	EXPECT_FLOAT_EQ(speech.uvMin.y, 0.25f + (1.0f / 512.0f));
	EXPECT_FLOAT_EQ(speech.uvMax.x - speech.uvMin.x, 0.1875f - (1.0f / 256.0f));
	EXPECT_FLOAT_EQ(rules::PictureOf(rules::Look::Speech, true).uvMin.x, 0.5f + (1.0f / 512.0f));
	EXPECT_FLOAT_EQ(rules::PictureOf(rules::Look::NoSpeech, false).uvMin.x, 0.3125f + (1.0f / 512.0f));
	EXPECT_FLOAT_EQ(rules::PictureOf(rules::Look::ExclaimArrow, true).uvMin.y, 0.4375f + (1.0f / 512.0f));
	EXPECT_EQ(rules::SendLook(true), rules::Look::NoSpeech);
	EXPECT_EQ(rules::SendLook(false), rules::Look::RightArrow);
	EXPECT_EQ(rules::OpenLook(true), rules::Look::ExclaimArrow);
	EXPECT_EQ(rules::OpenLook(false), rules::Look::Speech);
}

TEST(ChatControlsRules, TheControlsShowInANetworkGameUntilTheCreatureIsInTheHand)
{
	rules::ShowFacts facts {.connected = true, .singlePlayer = false, .inGame = true};
	EXPECT_TRUE(rules::ShowsNext(true, facts));
	EXPECT_FALSE(rules::ShowsNext(false, facts));
	facts.creatureInteracting = true;
	EXPECT_FALSE(rules::ShowsNext(true, facts));
	facts.creatureInteracting = false;
	// The button stays away after the creature leaves the hand, until a message comes in
	EXPECT_FALSE(rules::ShowsNext(false, facts));
	facts.incoming = true;
	EXPECT_TRUE(rules::ShowsNext(false, facts));
}

TEST(ChatControlsRules, AGameOfOnePlayerShowsTheControlsOnlyWithMusicOrAnOpenChat)
{
	rules::ShowFacts facts {.connected = true, .singlePlayer = true, .inGame = true};
	EXPECT_FALSE(rules::ShowsNext(true, facts));
	facts.music = true;
	EXPECT_TRUE(rules::ShowsNext(false, facts));
	facts.music = false;
	facts.open = true;
	EXPECT_TRUE(rules::ShowsNext(false, facts));
	// Out of the world, nothing changes after the first rule
	facts = {.connected = false, .singlePlayer = false, .inGame = false, .open = true};
	EXPECT_FALSE(rules::ShowsNext(true, facts));
	facts.connected = true;
	EXPECT_TRUE(rules::ShowsNext(true, facts));
	EXPECT_FALSE(rules::ShowsNext(false, facts));
}

TEST(ChatControlsRules, APlayerGoesHalfOfTheWayToWhite)
{
	EXPECT_EQ(rules::PlayerColour(0xFFFF0000u), 0x00FF7F7Fu);
	EXPECT_EQ(rules::PlayerColour(0xFF000000u), 0x007F7F7Fu);
	EXPECT_EQ(rules::PlayerColour(0x00204060u), 0x008F9FAFu);
}

namespace
{
struct ChatFixture: public ::testing::Test
{
	GameFont font = test::LoadFont();
	ChatControls chat {font};
	rules::ShowFacts network {.connected = true, .singlePlayer = false, .inGame = true};
	static constexpr glm::ivec2 k_OpenButton {790, 10};
	/// The first row of the quick chat list
	static constexpr glm::ivec2 k_FirstTaunt {500, 25};

	ChatFixture()
	{
		chat.SetTexts({u"AB", u"BA", u"W"}, u"A");
		chat.SetPlayers({{.slot = PlayerNames::PLAYER_THREE, .name = u"B", .selected = false},
		                 {.slot = PlayerNames::PLAYER_TWO, .name = u"A", .selected = false}});
	}

	bool Update(bool onlyShift = false)
	{
		network.open = chat.IsOpen();
		return chat.Update({.show = network, .otherBox = false, .onlyShift = onlyShift, .layoutSize = {800, 600}});
	}

	void Click(glm::ivec2 point)
	{
		chat.MouseMove(point);
		chat.MouseDown(point);
		chat.MouseUp(point, false, false, false);
	}
};
} // namespace

TEST_F(ChatFixture, TheSpeechButtonComesOnInANetworkGame)
{
	EXPECT_TRUE(Update());
	EXPECT_TRUE(chat.IsActive());
	EXPECT_FALSE(chat.IsOpen());
	EXPECT_FALSE(Update());
	// Another box over the world hides the controls
	EXPECT_FALSE(chat.Update({.show = network, .otherBox = true, .onlyShift = false, .layoutSize = {800, 600}}));
	EXPECT_FALSE(chat.IsActive());
}

TEST_F(ChatFixture, AClickOnTheSpeechButtonOpensTheLine)
{
	Update();
	Click(k_OpenButton);
	EXPECT_TRUE(chat.IsOpen());
	Update();
	EXPECT_TRUE(chat.WantsKeys(false));
	EXPECT_FALSE(chat.WantsKeys(true));
}

TEST_F(ChatFixture, ReturnSendsTheLineAndTheChatStaysOpen)
{
	Update();
	Click(k_OpenButton);
	Update();
	chat.TextInput(u"AB");
	EXPECT_EQ(chat.GetLine(), u"AB");
	EXPECT_TRUE(chat.KeyDown(SDLK_RETURN, false));
	const auto sent = chat.TakeSent();
	ASSERT_EQ(sent.size(), 1);
	EXPECT_EQ(sent[0].line, u"AB");
	EXPECT_FALSE(sent[0].taunt.has_value());
	EXPECT_TRUE(chat.IsOpen());
	EXPECT_TRUE(chat.GetLine().empty());
	// The line loses the focus
	Update();
	EXPECT_FALSE(chat.WantsKeys(false));
	EXPECT_TRUE(chat.TakeSent().empty());
}

TEST_F(ChatFixture, ReturnOnAnEmptyLineClosesTheChat)
{
	Update();
	Click(k_OpenButton);
	Update();
	chat.KeyDown(SDLK_RETURN, false);
	EXPECT_FALSE(chat.IsOpen());
	EXPECT_TRUE(chat.TakeSent().empty());
}

TEST_F(ChatFixture, EscapeClosesTheChat)
{
	Update();
	Click(k_OpenButton);
	Update();
	chat.TextInput(u"A");
	EXPECT_TRUE(chat.KeyDown(SDLK_ESCAPE, false));
	EXPECT_FALSE(chat.IsOpen());
	EXPECT_TRUE(chat.TakeSent().empty());
}

TEST_F(ChatFixture, WithShiftThePressOnTheSpeechButtonSendsATauntAndCloses)
{
	Update(true);
	chat.MouseMove(k_OpenButton);
	chat.MouseDown(k_OpenButton);
	EXPECT_TRUE(chat.IsOpen());
	EXPECT_TRUE(chat.IsQuickChatOpen());
	chat.MouseMove(k_FirstTaunt);
	Update(true);
	// The line shows the taunt under the pointer
	EXPECT_EQ(chat.GetLine(), u"AB");
	chat.MouseUp(k_FirstTaunt, false, false, false);
	const auto sent = chat.TakeSent();
	ASSERT_EQ(sent.size(), 1);
	EXPECT_EQ(sent[0].taunt, 0u);
	EXPECT_FALSE(chat.IsOpen());
	EXPECT_FALSE(chat.IsQuickChatOpen());
}

TEST_F(ChatFixture, AtFirstALineGoesToAllOfThePlayers)
{
	const auto recipients = chat.Recipients();
	EXPECT_EQ(recipients, (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE}));
	// The list is in the order of the god slots
	ASSERT_EQ(chat.GetPeople().players.size(), 2);
	EXPECT_EQ(chat.GetPeople().players[0].slot, PlayerNames::PLAYER_TWO);
}
