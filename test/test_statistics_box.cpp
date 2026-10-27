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
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Registry.h"
#include "ECS/StatisticsRules.h"
#include "Gui/StatisticsBoxRules.h"

using namespace openblack;
using namespace openblack::statistics;
using namespace openblack::gui::statistics_box;

namespace
{
/// Two players with a few known figures
BoxFigures TwoPlayers()
{
	BoxFigures figures;
	figures.playersInGame = 2;
	figures.landNumber = 1;
	PlayerFigures one {.player = PlayerNames::PLAYER_ONE};
	one.population = 40;
	one.men = 25;
	one.women = 15;
	one.births = 3;
	one.satisfaction = 0.5f;
	one.creatureDamage = 12.5f;
	one.castsOfType.at(static_cast<size_t>(MagicType::Fireball)) = 2;
	one.castsOfType.at(static_cast<size_t>(MagicType::Heal)) = 1;
	PlayerFigures two {.player = PlayerNames::PLAYER_TWO};
	two.population = 10;
	two.men = 4;
	two.women = 6;
	two.births = 1;
	two.castsOfType.at(static_cast<size_t>(MagicType::Fireball)) = 1;
	figures.players = {one, two};
	return figures;
}

const Row& RowNamed(const Page& page, const std::string& label)
{
	for (const auto& row : page.rows)
	{
		if (row.label == label)
		{
			return row;
		}
	}
	ADD_FAILURE() << "no row " << label;
	return page.rows.front();
}
/// A row of a page that is about to end would dangle, so the page must be kept in a variable
const Row& RowNamed(Page&&, const std::string&) = delete;
} // namespace

TEST(StatisticsBoxPages, EachTabListsItsRowsInOrder)
{
	const auto figures = TwoPlayers();
	EXPECT_EQ(MakePage(Tab::Player, figures, PlayerNames::PLAYER_ONE).rows.size(), 11u);
	EXPECT_EQ(MakePage(Tab::Creature, figures, PlayerNames::PLAYER_ONE).rows.size(), 10u);
	EXPECT_EQ(MakePage(Tab::Town, figures, PlayerNames::PLAYER_ONE).rows.size(), 33u);
	EXPECT_EQ(MakePage(Tab::Miracle, figures, PlayerNames::PLAYER_ONE).rows.size(), 41u);
	EXPECT_EQ(MakePage(Tab::Tech, figures, PlayerNames::PLAYER_ONE).rows.size(), 20u);
	const auto miracles = MakePage(Tab::Miracle, figures, PlayerNames::PLAYER_ONE);
	// The fireball's row comes after the three totals; the itchy creature miracle's after the angry one's
	EXPECT_EQ(miracles.rows.at(3).label, "HELP_TEXT_PAUSE_STATS_66");
	EXPECT_EQ(miracles.rows.at(3).values, (std::vector<float> {2.0f, 1.0f}));
	EXPECT_EQ(miracles.rows.at(37).label, "HELP_TEXT_PAUSE_STATS_100");
	EXPECT_EQ(miracles.rows.at(38).label, "HELP_TEXT_PAUSE_STATS_106");
}

TEST(StatisticsBoxPages, TheGraphBoxesAndThePickedRowFollowTheRowsCopied)
{
	const auto figures = TwoPlayers();
	const auto town = MakePage(Tab::Town, figures, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(town.graphs.at(static_cast<size_t>(Graph::Right)), 2u);
	EXPECT_EQ(town.graphs.at(static_cast<size_t>(Graph::Left)), 13u);
	EXPECT_EQ(town.graphs.at(static_cast<size_t>(Graph::Middle)), 19u);
	EXPECT_EQ(town.selected, 19u);
	// The creature tab fills its hidden boxes with the battles, keeps the right one empty and picks the battles won
	const auto creature = MakePage(Tab::Creature, figures, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(creature.graphs.at(static_cast<size_t>(Graph::Right)).has_value());
	EXPECT_EQ(creature.selected, 7u);
	EXPECT_TRUE(creature.creaturesShown);
	const auto tech = MakePage(Tab::Tech, figures, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(tech.selected, 2u);
	EXPECT_TRUE(MakePage(Tab::Player, figures, PlayerNames::PLAYER_ONE).lineGraphsShown);
}

TEST(StatisticsBoxRows, MenAndWomenAreOutOfTheFirstPlayersPeople)
{
	const auto page = MakePage(Tab::Town, TwoPlayers(), PlayerNames::PLAYER_ONE);
	const auto& men = RowNamed(page, "HELP_TEXT_PAUSE_STATS_33");
	EXPECT_EQ(men.most, 40.0f);
	EXPECT_EQ(Format(men, men.values.at(1)), "4/40");
	EXPECT_EQ(Format(men, *Total(men)), "29/40");
}

TEST(StatisticsBoxRows, ARowOutOfItsHighestTakesTheHighestValue)
{
	const auto page = MakePage(Tab::Town, TwoPlayers(), PlayerNames::PLAYER_ONE);
	const auto& births = RowNamed(page, "HELP_TEXT_PAUSE_STATS_30");
	EXPECT_EQ(births.most, 3.0f);
	EXPECT_EQ(Format(births, 3.0f), "3");
	EXPECT_EQ(Total(births), 4.0f);
}

TEST(StatisticsBoxRows, PercentRowsHaveNoTotal)
{
	const auto page = MakePage(Tab::Creature, TwoPlayers(), PlayerNames::PLAYER_ONE);
	const auto& damage = RowNamed(page, "HELP_TEXT_DIALOG_ADDITION_48");
	EXPECT_EQ(Format(damage, damage.values.at(0)), "12.50%");
	EXPECT_FALSE(Total(damage).has_value());
}

TEST(StatisticsBoxRows, DecimalsAndHeldValues)
{
	const auto page = MakePage(Tab::Town, TwoPlayers(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(Format(RowNamed(page, "HELP_TEXT_PAUSE_STATS_43"), 0.5f), "0.50");
	EXPECT_EQ(Held(3e25f), 1e20f);
	EXPECT_EQ(Held(-3e25f), -1e20f);
	// The fewest people starts at the largest number until a turn sets it
	EXPECT_EQ(RowNamed(page, "HELP_TEXT_PAUSE_STATS_39").values.at(0), 4294967296.0f);
}

TEST(StatisticsBoxRows, SingleFiguresAreAlwaysNumbersInTheLocalColour)
{
	auto figures = TwoPlayers();
	figures.tech.statisticsCount = k_StatisticsCount;
	const auto page = MakePage(Tab::Tech, figures, PlayerNames::PLAYER_TWO);
	const auto& count = RowNamed(page, "HELP_TEXT_PAUSE_STATS_16");
	EXPECT_EQ(count.values, (std::vector<float> {113.0f}));
	EXPECT_EQ(count.players, (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO}));
	EXPECT_FALSE(DrawnAsBar(count, true));
	const auto playerPage = MakePage(Tab::Player, figures, PlayerNames::PLAYER_ONE);
	const auto& alignment = RowNamed(playerPage, "HELP_TEXT_PAUSE_STATS_112");
	EXPECT_FALSE(DrawnAsBar(alignment, true));
}

TEST(StatisticsBoxLayout, ColumnsShareTheRightHand300)
{
	EXPECT_EQ(ColumnWidth(1), 150);
	EXPECT_EQ(ColumnWidth(3), 75);
	// One value: the total alone, centred 75 in from the right
	EXPECT_EQ(ColumnCentre(760, 1, 1), 685);
	EXPECT_EQ(ColumnCentre(760, 2, 0), 510);
}

TEST(StatisticsBoxLayout, ARowsBarIsSplitBetweenThePlayers)
{
	Row row {.values = {3.0f, 1.0f}, .most = 3.0f};
	EXPECT_EQ(BarParts(row), (std::vector<float> {0.75f, 1.0f}));
	// One value is out of the row's most
	Row one {.values = {2.0f}, .most = 8.0f};
	EXPECT_EQ(BarParts(one), (std::vector<float> {0.25f}));
}

TEST(StatisticsBoxLayout, GraphBoxesScaleToTheirMostOrTheTallest)
{
	Row one {.values = {2.0f}, .most = 8.0f};
	EXPECT_EQ(VerticalScale(one).highest, 8.0f);
	Row two {.values = {2.0f, -1.0f}, .most = 8.0f};
	EXPECT_EQ(VerticalScale(two).highest, 2.0f);
	EXPECT_EQ(VerticalScale(two).lowest, -1.0f);
	Row none {.values = {0.0f, 0.0f}};
	EXPECT_EQ(VerticalScale(none).highest, 1.0f);
}

TEST(StatisticsBoxLayout, GuideLinesDoubleUntilSpreadOut)
{
	EXPECT_EQ(GuideStep(1.0f, 230.0f), 0.25f);
	EXPECT_EQ(GuideStep(100.0f, 230.0f), 2.0f);
	EXPECT_EQ(GuideColour(0, true), glm::vec4(1.0f));
	EXPECT_FLOAT_EQ(GuideColour(4, false).a, 80.0f / 255.0f);
	EXPECT_FLOAT_EQ(GuideColour(5, false).a, 16.0f / 255.0f);
}

TEST(StatisticsBoxLayout, LineGraphsRunFromTheLowestPointToTheHighest)
{
	const auto scale = LinesScale({{4.0f, 9.0f}, {6.0f}});
	EXPECT_EQ(scale.lowest, 4.0f);
	EXPECT_EQ(scale.highest, 9.0f);
	// All the same: from nothing
	const auto flat = LinesScale({{5.0f, 5.0f}});
	EXPECT_EQ(flat.lowest, 0.0f);
	EXPECT_EQ(flat.highest, 5.0f);
	const auto empty = LinesScale({});
	EXPECT_EQ(empty.lowest, 0.0f);
	EXPECT_EQ(empty.highest, 1.0f);
}

TEST(StatisticsBoxLayout, TheLandsGiveSomePlayersOtherColours)
{
	EXPECT_EQ(PlayerColour(PlayerNames::PLAYER_ONE, 1), glm::vec4(1.0f, 0x46 / 255.0f, 0x46 / 255.0f, 1.0f));
	// Land 1's second player is orange, land 2's yellow and its third blue
	EXPECT_EQ(PlayerColour(PlayerNames::PLAYER_TWO, 1), PlayerColour(PlayerNames::PLAYER_SEVEN, 0));
	EXPECT_EQ(PlayerColour(PlayerNames::PLAYER_TWO, 2), PlayerColour(PlayerNames::PLAYER_FIVE, 0));
	EXPECT_EQ(PlayerColour(PlayerNames::PLAYER_THREE, 2), PlayerColour(PlayerNames::PLAYER_SIX, 0));
	EXPECT_EQ(PlayerColour(PlayerNames::PLAYER_TWO, 6), glm::vec4(0x47 / 255.0f, 1.0f, 0x54 / 255.0f, 1.0f));
}

TEST(StatisticsBoxLayout, CreaturesFillTheLeftOfTheGraphs)
{
	const auto one = CreatureBoxes(1);
	ASSERT_EQ(one.size(), 1u);
	EXPECT_EQ(one.at(0).first, glm::ivec2(40, 100));
	EXPECT_EQ(one.at(0).second, glm::ivec2(530, 330));
	const auto three = CreatureBoxes(3);
	ASSERT_EQ(three.size(), 3u);
	EXPECT_EQ(three.at(1).first, glm::ivec2(290, 100));
	EXPECT_EQ(three.at(1).second, glm::ivec2(530, 210));
	EXPECT_EQ(three.at(2).first, glm::ivec2(160, 220));
	EXPECT_EQ(three.at(2).second, glm::ivec2(400, 330));
	// Once round every 8,192 milliseconds
	EXPECT_EQ(CreatureTurn(0), CreatureTurn(8192));
	EXPECT_NEAR(CreatureTurn(4096), 3.14159f, 1e-3f);
}

TEST(StatisticsGlobal, LinesOfCodeAreTheTownsTimesTenThousandAndMore)
{
	int32_t asked = -1;
	const auto lines = LinesOfCodeStep(3, [&asked](int32_t n) {
		asked = n;
		return 7u;
	});
	EXPECT_EQ(asked, 6078);
	EXPECT_EQ(lines, 30397u);
	EXPECT_EQ(LinesOfCodeStep(0, [](int32_t) { return 0u; }), 0u);
}

TEST(StatisticsGlobal, LinesOfCodeRoundToAFloat)
{
	// 2000 towns give 20260000 lines. A float above 2^24 holds only even numbers, so one more line rounds away
	EXPECT_EQ(LinesOfCodeStep(2000, [](int32_t) { return 1u; }), 20260000u);
	EXPECT_EQ(LinesOfCodeStep(2000, [](int32_t) { return 3u; }), 20260004u);
}

TEST(StatisticsGlobal, TheFewestFramesASecondStaysAtNothing)
{
	GlobalFigures figures {.townsAtStart = 1};
	const auto none = [](int32_t) { return 0u; };
	StepGlobalFigures(figures, 30, none);
	StepGlobalFigures(figures, 20, none);
	StepGlobalFigures(figures, 40, none);
	EXPECT_EQ(figures.maxFrameRate, 40.0f);
	EXPECT_EQ(figures.minFrameRate, 0.0f);
	EXPECT_EQ(figures.linesOfCode, 3u * 10130u);
}

TEST(StatisticsGlobal, TheFrameRateIsMeasuredOverThreeSeconds)
{
	FrameRateMeter meter;
	EXPECT_EQ(meter.Rate(), 5);
	meter.Frame(1000, 1);
	// The frames are first noted once three seconds have gone
	meter.Frame(4000, 100);
	EXPECT_EQ(meter.Rate(), 5);
	meter.Frame(4500, 130);
	// 30 frames over the 3,500 milliseconds since the start
	EXPECT_EQ(meter.Rate(), 8);
	// Too few frames: the time continues
	meter.Frame(8000, 132);
	EXPECT_EQ(meter.Rate(), 8);
	meter.Frame(8500, 190);
	EXPECT_EQ(meter.Rate(), 15);
}

TEST(StatisticsGlobal, TheHelpRecordGivesTheTechFigures)
{
	const auto help = HelpFiguresFrom([](HelpEvent event) { return static_cast<uint32_t>(event); });
	EXPECT_EQ(help.distanceDragged, 33u * 3u);
	EXPECT_EQ(help.zooms, 29u * 9u);
	EXPECT_EQ(help.timeLookingAtSky, 46u);
	EXPECT_EQ(help.pickedUp, 2u);
	EXPECT_EQ(TimePlayed(1234), 123u);
}

TEST(StatisticsGlobal, EveryEntityMadeOnTheLandCounts)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	registry.Create();
	std::array<entt::entity, 3> more {};
	registry.Create(more.begin(), more.end());
	// A destroyed entity stays counted
	registry.Destroy(first);
	EXPECT_EQ(registry.GetEntitiesMade(), 5u);
	// A new land starts the count again
	registry.Reset();
	EXPECT_EQ(registry.GetEntitiesMade(), 0u);
	registry.Create();
	EXPECT_EQ(registry.GetEntitiesMade(), 1u);
}

TEST(StatisticsGlobal, EveryEntityThatGoesCounts)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	const auto second = registry.Create();
	EXPECT_EQ(registry.GetEntitiesGone(), 0u);
	registry.Destroy(first);
	EXPECT_EQ(registry.GetEntitiesGone(), 1u);
	std::array<entt::entity, 1> more {second};
	registry.Destroy(more.begin(), more.end());
	EXPECT_EQ(registry.GetEntitiesGone(), 2u);
	// Emptying the registry counts too, and the count does not start again
	registry.Create();
	registry.Reset();
	EXPECT_GT(registry.GetEntitiesGone(), 2u);
}
