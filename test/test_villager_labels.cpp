/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>

#include <gtest/gtest.h>

#include "Gui/VillagerLabels.h"

using namespace openblack::gui::villager_labels;

namespace
{
/// Every character ten pixels wide at full size, whatever the line height
float TenPixelsEach(std::u16string_view text, float /*lineHeight*/)
{
	return 10.0f * static_cast<float>(text.size());
}

const DetailWords k_Words {.age = u" A:", .life = u" L:", .food = u" F:", .pregnant = u" P:", .poisoned = u" S:"};

Request At(int x, int y, std::u16string text, float distance = 10.0f)
{
	return {.pixel = {x, y}, .distance = distance, .size = 1.0f, .text = std::move(text)};
}
} // namespace

TEST(VillagerLabels, NameDisplayGrowsAndShrinksByTheGameTime)
{
	EXPECT_FLOAT_EQ(StepNameFade(0.0f, true, 100.0f), 0.3f);
	EXPECT_FLOAT_EQ(StepNameFade(0.9f, true, 100.0f), 1.0f);
	EXPECT_FLOAT_EQ(StepNameFade(1.0f, false, 100.0f), 0.7f);
	EXPECT_FLOAT_EQ(StepNameFade(0.1f, false, 100.0f), 0.0f);
}

TEST(VillagerLabels, DetailsComeFirstAndCarryTheNameWhenAllowed)
{
	auto choice = Choose({.named = true, .detailsForThis = true, .nameFade = 0.5f});
	EXPECT_EQ(choice.kind, Kind::Details);
	EXPECT_FLOAT_EQ(choice.size, 1.0f);
	EXPECT_TRUE(choice.withName);

	choice = Choose({.named = true, .nameAllowed = false, .detailsForAll = true});
	EXPECT_EQ(choice.kind, Kind::Details);
	EXPECT_FALSE(choice.withName);

	choice = Choose({.named = false, .detailsForAll = true});
	EXPECT_EQ(choice.kind, Kind::Details);
	EXPECT_FALSE(choice.withName);
}

TEST(VillagerLabels, OnlyNamedVillagersShowANameAtTheDisplaysSize)
{
	auto choice = Choose({.named = true, .nameFade = 0.4f});
	EXPECT_EQ(choice.kind, Kind::Name);
	EXPECT_FLOAT_EQ(choice.size, 0.4f);
	EXPECT_EQ(Choose({.named = false, .nameFade = 1.0f}).kind, Kind::None);
	EXPECT_EQ(Choose({.named = true, .nameFade = 0.01f}).kind, Kind::None);
	EXPECT_EQ(Choose({.named = true, .nameAllowed = false, .nameFade = 1.0f}).kind, Kind::None);
	// A speech bubble takes the place of every label
	EXPECT_EQ(Choose({.named = true, .speaking = true, .detailsForAll = true, .nameFade = 1.0f}).kind, Kind::None);
}

TEST(VillagerLabels, DetailsTextIsTheGamesFormat)
{
	const Details details {.name = u"Peter Molyneux", .doing = u"Going Somewhere", .age = 23, .life = 1.0f, .food = 0.545f};
	EXPECT_EQ(DetailsText(details, k_Words), u"Peter Molyneux\nGoing Somewhere\n A:23  L:100%  F:55%");

	const Details sick {.doing = u"Dying", .age = 70, .life = 0.125f, .food = 0.0f, .pregnant = true, .poisoned = true};
	// 12.5 rounds away from zero, as the game's runtime does
	EXPECT_EQ(DetailsText(sick, k_Words), u"Dying\n A:70  L:13%  F:0% P: S:");
}

TEST(VillagerLabels, DebugDetailsForAStateWithoutText)
{
	const Details details {.doing = u"MoveToPos", .age = 5, .life = 0.5f, .food = 0.125f, .poisoned = true};
	EXPECT_EQ(DebugDetailsText(details, u"A:%d L:%.02f F:%.02f"), u"MoveToPos\nA:5 L:0.50 F:0.13 S");
}

TEST(VillagerLabels, FixedRoundsHalfAwayFromZero)
{
	EXPECT_EQ(FormatFixed(0.5, 0), u"1");
	EXPECT_EQ(FormatFixed(2.5, 0), u"3");
	EXPECT_EQ(FormatFixed(99.5, 0), u"100");
	EXPECT_EQ(FormatFixed(-1.5, 0), u"-2");
	EXPECT_EQ(FormatFixed(0.125, 2), u"0.13");
	EXPECT_EQ(FormatFixed(0.0, 0), u"0");
}

TEST(VillagerLabels, StoryLandsSwapTheComputerPlayersColours)
{
	EXPECT_EQ(PlayerColourIndex(0, 1), 0);
	EXPECT_EQ(PlayerColourIndex(1, 1), 6);
	EXPECT_EQ(PlayerColourIndex(1, 2), 4);
	EXPECT_EQ(PlayerColourIndex(2, 2), 5);
	EXPECT_EQ(PlayerColourIndex(1, 3), 5);
	EXPECT_EQ(PlayerColourIndex(1, 4), 6);
	EXPECT_EQ(PlayerColourIndex(1, 5), 6);
	EXPECT_EQ(PlayerColourIndex(1, 0), 1);
	EXPECT_EQ(PlayerColourIndex(2, 1), 2);
}

TEST(VillagerLabels, ColourGoesHalfWayToWhite)
{
	// No player: white stays white
	EXPECT_EQ(LabelColour(std::nullopt, false), glm::u8vec3(255));
	// Red 255,70,70 goes half way to white
	EXPECT_EQ(LabelColour(glm::u8vec3(255, 70, 70), false), glm::u8vec3(255, 162, 162));
	// A colour all but black is white
	EXPECT_EQ(LabelColour(glm::u8vec3(9, 9, 9), false), glm::u8vec3(255));
	// A woman's colour is first lightened by 145 of 256 of the way to white
	EXPECT_EQ(LabelColour(glm::u8vec3(0, 0, 0), true), glm::u8vec3(199, 199, 199));
}

TEST(VillagerLabels, KeysTakenOnlyWithTheInterfaceOutsideTheTemple)
{
	EXPECT_TRUE(KeysAct(false, false));
	EXPECT_FALSE(KeysAct(true, false));
	EXPECT_FALSE(KeysAct(false, true));
	EXPECT_EQ(DetailsKeyToggles(true), DetailsToggle::Pointed);
	EXPECT_EQ(DetailsKeyToggles(false), DetailsToggle::Everyone);
}

TEST(VillagerLabels, LabelStandsOverItsPoint)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 300, u"abc\nde"), 800.0f, TenPixelsEach, 1.0f));
	const auto& label = board.GetLabels().front();
	// 30 for the widest line and 6 more, centred on the point
	EXPECT_FLOAT_EQ(label.left, 382.0f);
	EXPECT_FLOAT_EQ(label.right, 418.0f);
	EXPECT_FLOAT_EQ(label.bottom, 250.0f);
	// Two lines of 16 and 4 more
	EXPECT_FLOAT_EQ(label.top, 214.0f);
	EXPECT_FLOAT_EQ(label.inset, 2.0f);
	ASSERT_EQ(label.lines.size(), 2u);
	EXPECT_EQ(label.lines[1], u"de");
	EXPECT_EQ(label.boxAlpha, 120);
	EXPECT_EQ(label.wordsAlpha, 255);
}

TEST(VillagerLabels, LabelsKeepOnTheScreenAcross)
{
	Board board;
	ASSERT_TRUE(board.Add(At(5, 300, u"abcd"), 800.0f, TenPixelsEach, 1.0f));
	EXPECT_FLOAT_EQ(board.GetLabels().front().left, 0.0f);
	ASSERT_TRUE(board.Add(At(799, 100, u"abcd"), 800.0f, TenPixelsEach, 1.0f));
	EXPECT_FLOAT_EQ(board.GetLabels().back().right, 800.0f);
	EXPECT_FLOAT_EQ(board.GetLabels().back().left, 754.0f);
	// Wider than the screen it is cut to half the screen
	Board narrow;
	ASSERT_TRUE(narrow.Add(At(50, 300, u"abcdefghijklmnop"), 100.0f, TenPixelsEach, 1.0f));
	EXPECT_FLOAT_EQ(narrow.GetLabels().front().right - narrow.GetLabels().front().left, 50.0f);
}

TEST(VillagerLabels, LaterLabelsStackOverEarlierOnes)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 300, u"abc"), 800.0f, TenPixelsEach, 1.0f));
	ASSERT_TRUE(board.Add(At(405, 305, u"abc"), 800.0f, TenPixelsEach, 1.0f));
	const auto& labels = board.GetLabels();
	ASSERT_EQ(labels.size(), 2u);
	// The first placed is the left one; the second sits a pixel above it
	EXPECT_FLOAT_EQ(labels[0].top, 230.0f);
	EXPECT_FLOAT_EQ(labels[1].bottom, 229.0f);
	EXPECT_FLOAT_EQ(labels[1].top, 209.0f);
}

TEST(VillagerLabels, LabelPushedOffTheTopIsDropped)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 90, u"abc"), 800.0f, TenPixelsEach, 1.0f));
	EXPECT_FALSE(board.Add(At(400, 90, u"abc"), 800.0f, TenPixelsEach, 1.0f));
	EXPECT_EQ(board.GetLabels().size(), 1u);
	EXPECT_FALSE(board.Add(At(400, 60, u"abc"), 800.0f, TenPixelsEach, 1.0f));
}

TEST(VillagerLabels, SmallLabelsAreNotShown)
{
	Board board;
	auto request = At(400, 300, u"abc");
	request.size = 0.005f;
	EXPECT_FALSE(board.Add(request, 800.0f, TenPixelsEach, 1.0f));
	request.size = 0.5f;
	ASSERT_TRUE(board.Add(request, 800.0f, TenPixelsEach, 1.0f));
	EXPECT_FLOAT_EQ(board.GetLabels().front().lineHeight, 8.0f);
	EXPECT_EQ(board.GetLabels().front().boxAlpha, 60);
	EXPECT_EQ(board.GetLabels().front().wordsAlpha, 127);
}

TEST(VillagerLabels, AtMostEightLines)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 500, u"1\n2\n3\n4\n5\n6\n7\n8\n9"), 800.0f, TenPixelsEach, 1.0f));
	const auto& lines = board.GetLabels().front().lines;
	ASSERT_EQ(lines.size(), 8u);
	EXPECT_EQ(lines.back(), u"8\n9");
}

TEST(VillagerLabels, PixelSizesScaleWithTheScreen)
{
	EXPECT_FLOAT_EQ(PixelScale({800.0f, 600.0f}), 1.0f);
	EXPECT_FLOAT_EQ(PixelScale({1920.0f, 1080.0f}), 1.8f);
	EXPECT_FLOAT_EQ(PixelScale({3440.0f, 1440.0f}), 2.4f);
	EXPECT_FLOAT_EQ(PixelScale({1280.0f, 1024.0f}), 1.6f);
	Board board;
	ASSERT_TRUE(board.Add(At(1000, 800, u"abc"), 1920.0f, TenPixelsEach, 2.0f));
	const auto& label = board.GetLabels().front();
	EXPECT_FLOAT_EQ(label.lineHeight, 32.0f);
	EXPECT_FLOAT_EQ(label.bottom, 700.0f);
	EXPECT_FLOAT_EQ(label.top, 700.0f - 8.0f - 32.0f);
	EXPECT_FLOAT_EQ(label.right - label.left, 30.0f + 12.0f);
	const auto pointer = PointerShape(label);
	EXPECT_FLOAT_EQ(pointer[1][0].position.y, 768.0f);
	EXPECT_FLOAT_EQ(pointer[1][0].position.x, 988.0f);
}

TEST(VillagerLabels, DrawnFurthestFirst)
{
	Board board;
	ASSERT_TRUE(board.Add(At(100, 300, u"a", 5.0f), 800.0f, TenPixelsEach, 1.0f));
	ASSERT_TRUE(board.Add(At(600, 300, u"b", 50.0f), 800.0f, TenPixelsEach, 1.0f));
	const auto order = board.DrawOrder();
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0]->lines[0], u"b");
}

TEST(VillagerLabels, BoxIsNineRoundedPiecesAndThePointerReachesThePoint)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 300, u"abc\nde"), 800.0f, TenPixelsEach, 1.0f));
	const auto& label = board.GetLabels().front();
	const auto box = BoxShape(label);
	ASSERT_EQ(box.size(), 9u);
	// A quarter of the 36 pixel height rounds the corners
	EXPECT_FLOAT_EQ(box[0][0].position.x, 382.0f);
	EXPECT_FLOAT_EQ(box[0][2].position.x, 391.0f);
	EXPECT_FLOAT_EQ(box[0][2].position.y, 223.0f);
	EXPECT_FLOAT_EQ(box[8][2].uv.x, 0.25f);
	const auto pointer = PointerShape(label);
	EXPECT_FLOAT_EQ(pointer[0][0].position.y, 250.0f);
	EXPECT_FLOAT_EQ(pointer[0][2].position.y, 284.0f);
	EXPECT_FLOAT_EQ(pointer[1][2].position.y, 300.0f);
	EXPECT_FLOAT_EQ(pointer[1][0].position.x, 394.0f);
	EXPECT_FLOAT_EQ(pointer[1][2].position.x, 406.0f);
}
