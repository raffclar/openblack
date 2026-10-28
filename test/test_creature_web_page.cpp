/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <CreatureHtml.h>
#include <gtest/gtest.h>

#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureWebPage.h"
#include "Graphics/ScreenshotCapture.h"

using namespace openblack;
using creature_web_page::Picture;
using creaturehtml::Value;

namespace
{

std::string FillWith(std::string_view page, const std::vector<Value>& values)
{
	return creaturehtml::Fill(page, values);
}

/// A screen of BGRA pixels whose red is the column and whose green is the row, each cut to a byte
struct FakeScreen
{
	uint32_t width;
	uint32_t height;
	std::vector<uint8_t> bgra;

	FakeScreen(uint32_t w, uint32_t h)
	    : width(w)
	    , height(h)
	    , bgra(static_cast<size_t>(w) * h * 4)
	{
		for (uint32_t y = 0; y < h; ++y)
		{
			for (uint32_t x = 0; x < w; ++x)
			{
				const auto at = (static_cast<size_t>(y) * w + x) * 4;
				bgra[at + 0] = 7;
				bgra[at + 1] = static_cast<uint8_t>(y);
				bgra[at + 2] = static_cast<uint8_t>(x);
				bgra[at + 3] = 255;
			}
		}
	}

	[[nodiscard]] graphics::ScreenView View(bool yflip = false) const
	{
		return {.width = width, .height = height, .pitch = width * 4, .yflip = yflip, .bgra = bgra};
	}
};

std::vector<Picture> FivePictures()
{
	return {{.score = 0.5f, .action = 1},
	        {.score = 0.2f, .action = 2},
	        {.score = 0.7f, .action = 3},
	        {.score = 0.2f, .action = 4},
	        {.score = 0.9f, .action = 5}};
}

} // namespace

TEST(CreatureHtml, FillsMarksWithValues)
{
	const std::vector<Value> values {{"name", "Rex"}, {"age", "12"}};
	EXPECT_EQ(FillWith("Name [$name$] is [$age$].\n", values), "Name Rex is 12.\r\n");
}

TEST(CreatureHtml, TheLastValueOfANameCounts)
{
	const std::vector<Value> values {{"name", "first"}, {"name", "last"}};
	EXPECT_EQ(FillWith("[$name$]", values), "last");
}

TEST(CreatureHtml, AMarkWithNoValueStopsTheRestOfItsLine)
{
	const std::vector<Value> values {{"name", "Rex"}};
	EXPECT_EQ(FillWith("a [$name$] b [$none$] c\nnext\n", values), "a Rex b next\r\n");
}

TEST(CreatureHtml, TheCharacterAfterTheDollarGoesWhateverItIs)
{
	const std::vector<Value> values {{"name", "Rex"}};
	EXPECT_EQ(FillWith("[$name$x!", values), "Rex!");
	EXPECT_EQ(FillWith("[$name$", values), "Rex");
	// Without a "$", the rest of the line is the name, its line feed too
	EXPECT_EQ(FillWith("[$name\nnext", values), "next");
	EXPECT_EQ(FillWith("[ $name$] [x", values), "[ $name$] [x");
}

TEST(CreatureHtml, AnEmptyValueLeavesNothing)
{
	const std::vector<Value> values {{"name", ""}};
	EXPECT_EQ(FillWith("<[$name$]>", values), "<>");
}

TEST(CreatureHtml, ReadsAndWritesWindowsText)
{
	const std::vector<Value> values {};
	EXPECT_EQ(FillWith("a\r\nb\rc\n", values), "a\r\nb\rc\r\n");
	EXPECT_EQ(FillWith("a\nb\x1A"
	                   "c\n",
	                   values),
	          "a\r\nb");
	EXPECT_EQ(FillWith(std::string_view("a\0b\nc", 5), values), "ac");
}

TEST(CreatureHtml, FillsALongLineInParts)
{
	const std::vector<Value> values {{"name", "Rex"}};
	// The mark starts at the end of the first part, so the second part starts with the rest of the mark
	const auto page = std::string(creaturehtml::k_LongestPart - 1, '.') + "[$name$]";
	EXPECT_EQ(FillWith(page, values), std::string(creaturehtml::k_LongestPart - 1, '.') + "[$name$]");
	const auto whole = std::string(creaturehtml::k_LongestPart - 8, '.') + "[$name$]";
	EXPECT_EQ(FillWith(whole, values), std::string(creaturehtml::k_LongestPart - 8, '.') + "Rex");
}

TEST(CreatureWebPage, FormatsHundredthsAsTheGame)
{
	EXPECT_EQ(creature_web_page::FormatHundredths(0.5f), "0.50");
	EXPECT_EQ(creature_web_page::FormatHundredths(1.0f / 3.0f), "0.33");
	EXPECT_EQ(creature_web_page::FormatHundredths(0.124f), "0.12");
	// Halfway goes away from zero
	EXPECT_EQ(creature_web_page::FormatHundredths(0.125f), "0.13");
	EXPECT_EQ(creature_web_page::FormatHundredths(0.875f), "0.88");
	EXPECT_EQ(creature_web_page::FormatHundredths(-0.125f), "-0.13");
	EXPECT_EQ(creature_web_page::FormatHundredths(2.675f), "2.67");
	EXPECT_EQ(creature_web_page::FormatHundredths(-0.001f), "-0.00");
}

TEST(CreatureWebPage, WritesTextAsEnglishWindows)
{
	EXPECT_EQ(creature_web_page::ToWindowsText(u"Rex"), "Rex");
	EXPECT_EQ(creature_web_page::ToWindowsText(u"é€’一"), "\xE9\x80\x92?");
	EXPECT_EQ(creature_web_page::PageFile(u"Rex"), "Rex.html");
	EXPECT_EQ(creature_web_page::PictureFile(3), "creatureshot_3.jpg");
}

TEST(CreatureWebPage, CountsWholeSeconds)
{
	EXPECT_EQ(creature_web_page::WholeSecondsBetween(0, 109, 100), 10u);
	EXPECT_EQ(creature_web_page::WholeSecondsBetween(0, 110, 100), 11u);
	EXPECT_EQ(creature_web_page::WholeSecondsBetween(5, 4, 100), 429496729u);
}

TEST(CreatureWebPage, TakesAPictureOnlyWhenAllAgree)
{
	const std::vector<Picture> pictures {{.score = 0.5f, .age = 1, .action = 7}};
	const creature_web_page::Chance chance {.lookedAt = true, .seconds = 11, .actionMoment = true, .action = 8};
	EXPECT_TRUE(creature_web_page::TakesPicture(pictures, chance));
	auto other = chance;
	other.seconds = 10;
	EXPECT_FALSE(creature_web_page::TakesPicture(pictures, other));
	other = chance;
	other.lookedAt = false;
	EXPECT_FALSE(creature_web_page::TakesPicture(pictures, other));
	other = chance;
	other.actionMoment = false;
	EXPECT_FALSE(creature_web_page::TakesPicture(pictures, other));
	other = chance;
	other.action = 7;
	EXPECT_FALSE(creature_web_page::TakesPicture(pictures, other));
}

TEST(CreatureWebPage, ScoresByWorthPriorityAndAge)
{
	EXPECT_FLOAT_EQ(creature_web_page::Score(0.5f, 0.5f, 29), 0.25f);
	EXPECT_FLOAT_EQ(creature_web_page::Score(1.0f, 1.0f, 30), 0.9f);
	EXPECT_FLOAT_EQ(creature_web_page::Score(1.0f, 1.0f, 99), 0.9f);
	EXPECT_FLOAT_EQ(creature_web_page::Score(1.0f, 1.0f, 100), 0.8f);
	EXPECT_FLOAT_EQ(creature_web_page::Score(2.0f, -1.0f, 0), 0.0f);
	EXPECT_FLOAT_EQ(creature_web_page::Score(2.0f, 3.0f, 0), 1.0f);
}

TEST(CreatureWebPage, KeepsTheFirstFivePicturesInTheirSlots)
{
	std::vector<Picture> pictures;
	for (uint32_t i = 0; i < creature_web_page::k_MostPictures; ++i)
	{
		EXPECT_EQ(creature_web_page::Keep(pictures, {.score = 0.1f, .action = i}), i);
	}
	EXPECT_EQ(pictures.size(), creature_web_page::k_MostPictures);
}

TEST(CreatureWebPage, ABetterPictureReplacesTheFirstLowestAndGoesToTheLastSlot)
{
	auto pictures = FivePictures();
	EXPECT_EQ(creature_web_page::Keep(pictures, {.score = 0.3f, .action = 9}), 4u);
	ASSERT_EQ(pictures.size(), 5u);
	EXPECT_EQ(pictures[1].action, 3u);
	EXPECT_EQ(pictures[2].action, 4u);
	EXPECT_EQ(pictures[4].action, 9u);
}

TEST(CreatureWebPage, AWorsePictureIsNotKept)
{
	auto pictures = FivePictures();
	EXPECT_FALSE(creature_web_page::Keep(pictures, {.score = 0.2f, .action = 9}).has_value());
	EXPECT_EQ(pictures[1].action, 2u);
}

TEST(CreatureWebPage, FivePicturesWithFullScoresKeepNoNewOne)
{
	std::vector<Picture> pictures(creature_web_page::k_MostPictures, {.score = 1.0f});
	EXPECT_FALSE(creature_web_page::Keep(pictures, {.score = 1.0f, .action = 9}).has_value());
	EXPECT_EQ(pictures.size(), creature_web_page::k_MostPictures);
}

TEST(CreatureWebPage, CaptionsByTheDesireOfThePlan)
{
	EXPECT_EQ(creature_web_page::CaptionText(0), 624u);
	EXPECT_EQ(creature_web_page::CaptionText(39), 663u);
	EXPECT_EQ(creature_web_page::CaptionText(std::nullopt), 664u);
	EXPECT_EQ(creature_web_page::Caption(u"Wants food."), u" Wants food.");
}

TEST(CreatureWebPage, PicksAndBlendsPixelsAcrossTheScreen)
{
	const FakeScreen screen(800, 600);
	const auto picture = creature_web_page::TakePicture(screen.View());
	ASSERT_EQ(picture.size(), 400u * 300u * 3u);
	// The first pixel of a row is its first pixel with itself
	EXPECT_EQ(picture[0], 0);
	EXPECT_EQ(picture[1], 0);
	EXPECT_EQ(picture[2], 7);
	// Then each pixel is the mean of the pixel picked and the one picked before it: columns 2 apart
	const auto at = [&picture](uint32_t x, uint32_t y) { return (static_cast<size_t>(y) * 400 + x) * 3; };
	EXPECT_EQ(picture[at(10, 0)], (20 + 18) / 2);
	EXPECT_EQ(picture[at(10, 5) + 1], 10);
	const auto flipped = creature_web_page::TakePicture(screen.View(true));
	EXPECT_EQ(flipped[at(0, 0) + 1], static_cast<uint8_t>(599));
}

TEST(CreatureWebPage, AWideScreenGivesThePictureOfItsMiddle)
{
	const FakeScreen screen(160, 60);
	const auto picture = creature_web_page::TakePicture(screen.View());
	ASSERT_FALSE(picture.empty());
	// The 4:3 middle of 160 by 60 is 80 by 60 from column 40
	EXPECT_EQ(picture[0], 40);
}

TEST(CreatureWebPage, MakesAJpegFile)
{
	const std::vector<uint8_t> rgb(400 * 300 * 3, 128);
	const auto jpeg = creature_web_page::EncodeJpeg(rgb, 400, 300);
	ASSERT_GT(jpeg.size(), 4u);
	EXPECT_EQ(jpeg[0], 0xFF);
	EXPECT_EQ(jpeg[1], 0xD8);
	EXPECT_TRUE(creature_web_page::EncodeJpeg({}, 400, 300).empty());
}

TEST(CreatureWebPage, FillsTheShippedMarks)
{
	creature_web_page::PageFacts facts {
	    .name = u"Rex",
	    .age = 12,
	    .health = 0.75f,
	    .alignment = -0.5f,
	    .energy = 1.0f,
	    .strength = 0.25f,
	    .peopleKilled = 3,
	    .battlesWon = 2,
	};
	facts.captions.at(1) = u" Your Creature is angry.";
	facts.desires.emplace_back("impress", 0.5f);
	facts.beliefs.at(2) = 4;
	facts.niceness = 6;
	facts.attention = 4;
	const auto values = creature_web_page::PageValues(facts);
	const auto page = creaturehtml::Fill("[$name$] [$age$] [$alignment$] [$strength$] [$health$] [$energy$] "
	                                     "[$num-people-killed$] [$num-battles-won$]",
	                                     values);
	EXPECT_EQ(page, "Rex 12 -0.50 0.25 0.75 1.00 3 2");
	EXPECT_EQ(creaturehtml::Fill("[$picture_dialog_1$]|[$picture_dialog_4$]|[$desire-impress$]|[$beliefs_2$]", values),
	          " Your Creature is angry.||0.50|4");
	EXPECT_EQ(creaturehtml::Fill("[$how_nice_player_has_been$] [$how_much_attention_player_has_given_me$]", values),
	          "1136 1141");
}

TEST(CreatureWebPage, SaysWhichSpellIsOnTheCreature)
{
	creature_web_page::PageFacts facts;
	const auto none = creature_web_page::PageValues(facts);
	EXPECT_TRUE(std::ranges::none_of(none, [](const Value& value) { return value.name.starts_with("spell_received_"); }));

	facts.spellOn = 4;
	const auto values = creature_web_page::PageValues(facts);
	const auto spell = std::ranges::find(values, std::string("spell_received_1050"), &Value::name);
	ASSERT_NE(spell, values.end());
	EXPECT_EQ(spell->value, "1");
	// It comes after the beliefs and before the two help numbers
	EXPECT_EQ((spell - 1)->name, "beliefs_23");
	EXPECT_EQ((spell + 1)->name, "how_nice_player_has_been");
}

TEST(CreatureWebPage, PacksTheValuesLastFirst)
{
	const std::vector<Value> values {{"name", "Rex"}, {"age", "12"}};
	const auto packed = creature_web_page::PackValues(values);
	ASSERT_EQ(packed.size(), 1000u);
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(packed.data())), "age");
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(packed.data() + 250)), "12");
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(packed.data() + 500)), "name");
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(packed.data() + 750)), "Rex");
	EXPECT_TRUE(std::all_of(packed.begin() + 504, packed.begin() + 750, [](uint8_t byte) { return byte == 0; }));

	const std::vector<Value> longValue {{"key", std::string(300, 'v')}};
	const auto cut = creature_web_page::PackValues(longValue);
	EXPECT_EQ(std::string(reinterpret_cast<const char*>(cut.data() + 250)), std::string(249, 'v'));
}

TEST(CreatureWebPage, PackageHasTheValuesThenThePicturesThatExist)
{
	const std::vector<Value> values {{"name", "Rex"}};
	std::array<std::optional<std::vector<uint8_t>>, creature_web_page::k_MostPictures> pictures;
	pictures.at(1) = std::vector<uint8_t> {1, 2};
	pictures.at(4) = std::vector<uint8_t> {3};
	const auto package = creature_web_page::UploadPackage(values, pictures, 77);
	EXPECT_EQ(package.stamp, 77);
	ASSERT_EQ(package.blocks.size(), 3u);
	EXPECT_EQ(package.blocks[0].category, "CREATURE");
	EXPECT_EQ(package.blocks[0].name, "creature");
	EXPECT_EQ(package.blocks[0].data, creature_web_page::PackValues(values));
	EXPECT_EQ(package.blocks[1].category, "PICTURES");
	EXPECT_EQ(package.blocks[1].name, "_1.jpg");
	EXPECT_EQ(package.blocks[1].data, (std::vector<uint8_t> {1, 2}));
	EXPECT_EQ(package.blocks[2].name, "_4.jpg");
}

TEST(CreatureWebPage, PointsTheDecorationsAtTheGameFolder)
{
	EXPECT_EQ(creature_web_page::FolderUrl("C:/Games/Black & White"), "file:///C:/Games/Black%20%26%20White/");
	EXPECT_EQ(creature_web_page::FolderUrl("/home/me/bw/"), "file:///home/me/bw/");
	const auto page = creature_web_page::PointLinksAtGameFolder(
	    "var ImageRoot\t=\t\"../../../scripts/html/\";\r\nvar CreatureImages\t=\t\"../\";\r\n", "D:/bw");
	EXPECT_EQ(page, "var ImageRoot\t=\t\"file:///D:/bw/scripts/html/\";\r\nvar CreatureImages\t=\t\"../\";\r\n");
	EXPECT_EQ(creature_web_page::PointLinksAtGameFolder("a ../../../x ../../../y", "/g"), "a file:///g/x file:///g/y");
}

TEST(CreatureWebPage, TheBodyGetsToTheMomentOfItsAction)
{
	creature_layers::BodyAction once {.kind = creature_layers::BodyAction::Kind::Once, .animations = {53, 53, 53}};
	once.timeMs = 400.0f;
	// The middle of a 1001 millisecond animation is 500
	EXPECT_FALSE(creature_layers::ReachesActionMoment(once, 100.0f, 1001, 0));
	EXPECT_TRUE(creature_layers::ReachesActionMoment(once, 100.1f, 1001, 0));
	once.timeMs = 500.0f;
	EXPECT_TRUE(creature_layers::ReachesActionMoment(once, 1.0f, 1001, 0));
	once.timeMs = 501.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(once, 100.0f, 1001, 0));
	once.timeMs = 260.0f;
	once.hurried = true;
	EXPECT_TRUE(creature_layers::ReachesActionMoment(once, 121.0f, 1001, 0));
	once.hurried = false;
	once.timedByPlayer = true;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(once, 1000.0f, 1001, 0));

	// The kiss of the air plays the low kiss alone. It gets to its moment at the kiss time of the species, at its start
	// when the species has none.
	creature_layers::BodyAction kiss {
	    .kind = creature_layers::BodyAction::Kind::Once, .animations = {creature_kiss::k_LowKiss, 0, 0}, .kiss = true};
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kiss, 10.0f, 1733, 0));
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kiss, 10.0f, 1733, 800));
	kiss.timeMs = 795.0f;
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kiss, 10.0f, 1733, 800));
	kiss.timeMs = 10.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kiss, 10.0f, 1733, 0));
	// The kiss time counts in whole milliseconds: the moment comes when the time before the step is at most the kiss
	// time and the time after the step is more
	kiss.timeMs = 800.0f;
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kiss, 1.0f, 1733, 800));
	kiss.timeMs = 799.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kiss, 1.0f, 1733, 800));
	// An action that plays a kiss clip by itself is not the kiss: it gets to its moment at the middle of its animation
	creature_layers::BodyAction kissClip {.kind = creature_layers::BodyAction::Kind::Once,
	                                      .animations = {creature_kiss::k_HighKiss, 0, 0}};
	kissClip.timeMs = 795.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kissClip, 10.0f, 1733, 800));
	kissClip.timeMs = 860.0f;
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kissClip, 10.0f, 1733, 800));

	const creature_layers::BodyAction stand {};
	EXPECT_FALSE(creature_layers::ReachesActionMoment(stand, 1000.0f, 1001, 0));
}

TEST(CreatureWebPage, TheKissMovesOnByTheMillisecondsOfTheTurn)
{
	creature_layers::BodyAction kiss {
	    .kind = creature_layers::BodyAction::Kind::Once, .animations = {creature_kiss::k_LowKiss, 0, 0}, .kiss = true};
	// The high kiss times the kiss, also when the body plays the low kiss alone
	EXPECT_EQ(creature_layers::TimingAnimation(kiss), creature_kiss::k_HighKiss);
	const creature_layers::BodyAction kissClip {.kind = creature_layers::BodyAction::Kind::Once,
	                                            .animations = {creature_kiss::k_LowKiss, 0, 0}};
	EXPECT_EQ(creature_layers::TimingAnimation(kissClip), creature_kiss::k_LowKiss);
	const creature_layers::BodyAction stand {};
	EXPECT_EQ(creature_layers::TimingAnimation(stand), creature_layers::animations::k_Stand);

	// The kiss moves on by the milliseconds of the turn, not by the step of the playback at the rate of the size
	kiss.timeMs = 790.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kiss, 5.0f, 1733, 800));
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kiss, 5.0f, 1733, 800, 15.0f));
	EXPECT_FLOAT_EQ(creature_layers::AdvanceBody(kiss, 5.0f, 1733, 15.0f).timeMs, 805.0f);
	EXPECT_FLOAT_EQ(creature_layers::AdvanceBody(kissClip, 5.0f, 1733, 15.0f).timeMs, 5.0f);

	// The step that gets to the length of the kiss ends it, and the kiss gets to no moment in that step
	kiss.timeMs = 795.0f;
	EXPECT_FALSE(creature_layers::ReachesActionMoment(kiss, 10.0f, 805, 800, 10.0f));
	EXPECT_TRUE(creature_layers::ReachesActionMoment(kiss, 10.0f, 806, 800, 10.0f));
	EXPECT_EQ(creature_layers::AdvanceBody(kiss, 10.0f, 805, 10.0f).kind, creature_layers::BodyAction::Kind::Stand);
	EXPECT_EQ(creature_layers::AdvanceBody(kiss, 10.0f, 806, 10.0f).kind, creature_layers::BodyAction::Kind::Once);
}
