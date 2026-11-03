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
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Editor/EditorHistory.h"
#include "Editor/EditorPaths.h"
#include "Editor/LandBrush.h"
#include "Editor/LandSave.h"
#include "LHScriptX/FeatureScriptWriter.h"

using namespace openblack;
using namespace openblack::editor;

namespace
{

/// A land of corners with altitudes and countries, and no land where none is given
struct FakeLand
{
	std::map<std::pair<int, int>, uint8_t> altitudes;
	std::map<std::pair<int, int>, uint8_t> countries;

	[[nodiscard]] land_brush::AltitudeAt Altitudes() const
	{
		return [this](glm::u16vec2 corner) -> std::optional<uint8_t> {
			const auto found = altitudes.find({corner.x, corner.y});
			return found != altitudes.end() ? std::optional(found->second) : std::nullopt;
		};
	}
	[[nodiscard]] land_brush::CountryAt Countries() const
	{
		return [this](glm::u16vec2 corner) -> std::optional<uint8_t> {
			const auto found = countries.find({corner.x, corner.y});
			return found != countries.end() ? std::optional(found->second) : std::nullopt;
		};
	}
};

/// A square of land round corner (10, 10), all of one altitude and country
FakeLand SquareOfLand(uint8_t altitude, uint8_t country)
{
	FakeLand land;
	for (int x = 5; x <= 15; ++x)
	{
		for (int z = 5; z <= 15; ++z)
		{
			land.altitudes[{x, z}] = altitude;
			land.countries[{x, z}] = country;
		}
	}
	return land;
}

lnd::LNDBlock BlockAt(uint32_t x, uint32_t z, uint8_t altitude)
{
	lnd::LNDBlock block {};
	block.blockX = x;
	block.blockZ = z;
	for (auto& cell : block.cells)
	{
		cell.altitude = altitude;
	}
	return block;
}

} // namespace

TEST(EditorLandBrush, TheInnerCircleTakesTheWholeChangeAndTheOuterCircleNone)
{
	EXPECT_FLOAT_EQ(land_brush::Weight(0.0f, 10.0f, 30.0f), 1.0f);
	EXPECT_FLOAT_EQ(land_brush::Weight(10.0f, 10.0f, 30.0f), 1.0f);
	EXPECT_FLOAT_EQ(land_brush::Weight(20.0f, 10.0f, 30.0f), 0.5f);
	EXPECT_FLOAT_EQ(land_brush::Weight(30.0f, 10.0f, 30.0f), 0.0f);
	EXPECT_FLOAT_EQ(land_brush::Weight(45.0f, 10.0f, 30.0f), 0.0f);
	// Circles of one size: the whole change in the circle and none out of it
	EXPECT_FLOAT_EQ(land_brush::Weight(20.0f, 20.0f, 20.0f), 1.0f);
	EXPECT_FLOAT_EQ(land_brush::Weight(20.5f, 20.0f, 20.0f), 0.0f);
}

TEST(EditorLandBrush, AnAltitudeChangesToTheNearestStepAndStaysInItsByte)
{
	EXPECT_EQ(land_brush::Brushed(100, 4, 1.0f), 104);
	EXPECT_EQ(land_brush::Brushed(100, 4, 0.5f), 102);
	EXPECT_EQ(land_brush::Brushed(100, 4, 0.1f), 100);
	EXPECT_EQ(land_brush::Brushed(100, -4, 1.0f), 96);
	EXPECT_EQ(land_brush::Brushed(253, 8, 1.0f), 255);
	EXPECT_EQ(land_brush::Brushed(3, -8, 1.0f), 0);
}

TEST(EditorLandBrush, TheCornersInReachStopAtTheEdgeOfTheMap)
{
	const auto corners = land_brush::CornersInReach({0.0f, 0.0f}, 10.0f);
	const std::vector<glm::u16vec2> expected {{0, 0}, {0, 1}, {1, 0}};
	EXPECT_EQ(corners, expected);
	const auto far = land_brush::CornersInReach({5110.0f, 5110.0f}, 5.0f);
	ASSERT_EQ(far.size(), 1u);
	EXPECT_EQ(far.front(), glm::u16vec2(511, 511));
	// A circle of 10 metres round a corner reaches it and its four neighbours
	EXPECT_EQ(land_brush::CornersInReach({100.0f, 100.0f}, 10.0f).size(), 5u);
}

TEST(EditorLandBrush, AStrokeRaisesTheCornersInReachAndSkipsCornersWithNoLand)
{
	auto land = SquareOfLand(100, 0);
	land.altitudes.erase({10, 11});
	const land_brush::Brush brush {.mode = land_brush::Mode::Raise, .innerRadius = 0.0f, .outerRadius = 10.0f, .step = 4};
	const auto changes = land_brush::Stroke(brush, {100.0f, 100.0f}, land.Altitudes());
	// The middle takes the whole step; its neighbours at the outer circle take none, so do not change
	const std::vector<land_brush::AltitudeChange> expected {{.corner = {10, 10}, .altitude = 104}};
	EXPECT_EQ(changes, expected);

	const land_brush::Brush wide {.mode = land_brush::Mode::Lower, .innerRadius = 10.0f, .outerRadius = 10.0f, .step = 4};
	const auto lowered = land_brush::Stroke(wide, {100.0f, 100.0f}, land.Altitudes());
	EXPECT_EQ(lowered.size(), 4u);
	for (const auto& change : lowered)
	{
		EXPECT_EQ(change.altitude, 96);
		EXPECT_NE(change.corner, glm::u16vec2(10, 11));
	}
}

TEST(EditorLandBrush, APaintChangesNoAltitudeAndAStrokePaintsNoCountry)
{
	const auto land = SquareOfLand(100, 2);
	const land_brush::Brush paint {.mode = land_brush::Mode::Paint, .innerRadius = 10.0f, .outerRadius = 10.0f};
	EXPECT_TRUE(land_brush::Stroke(paint, {100.0f, 100.0f}, land.Altitudes()).empty());
	const land_brush::Brush raise {.mode = land_brush::Mode::Raise, .innerRadius = 10.0f, .outerRadius = 10.0f};
	EXPECT_TRUE(land_brush::Paint(raise, {100.0f, 100.0f}, 4, land.Countries()).empty());
}

TEST(EditorLandBrush, APaintGivesTheCountryToTheCornersThatHaveAnother)
{
	auto land = SquareOfLand(100, 2);
	land.countries[{10, 10}] = 3;
	const land_brush::Brush brush {.mode = land_brush::Mode::Paint, .innerRadius = 0.0f, .outerRadius = 10.0f, .country = 3};
	const auto changes = land_brush::Paint(brush, {100.0f, 100.0f}, 4, land.Countries());
	EXPECT_EQ(changes.size(), 4u);
	for (const auto& change : changes)
	{
		EXPECT_EQ(change.country, 3);
		EXPECT_NE(change.corner, glm::u16vec2(10, 10));
	}
	// A country that the land does not have paints nothing
	EXPECT_TRUE(land_brush::Paint(brush, {100.0f, 100.0f}, 3, land.Countries()).empty());
}

TEST(EditorLandBrush, ADragStrokesAgainAfterOneCornerSpacing)
{
	EXPECT_FALSE(land_brush::MovedFarEnough({0.0f, 0.0f}, {9.9f, 0.0f}));
	EXPECT_TRUE(land_brush::MovedFarEnough({0.0f, 0.0f}, {6.0f, 8.0f}));
}

TEST(EditorLandBrush, AHeightChangeCommandGivesTheDirectionAndTheStep)
{
	land_brush::Brush brush {.mode = land_brush::Mode::Paint, .step = 4};
	land_brush::TakeHeightChange(brush, 0);
	EXPECT_EQ(brush.mode, land_brush::Mode::Paint);
	EXPECT_EQ(brush.step, 4);
	land_brush::TakeHeightChange(brush, 7);
	EXPECT_EQ(brush.mode, land_brush::Mode::Raise);
	EXPECT_EQ(brush.step, 7);
	land_brush::TakeHeightChange(brush, -3);
	EXPECT_EQ(brush.mode, land_brush::Mode::Lower);
	EXPECT_EQ(brush.step, 3);
	land_brush::TakeHeightChange(brush, INT32_MIN);
	EXPECT_EQ(brush.mode, land_brush::Mode::Lower);
	EXPECT_EQ(brush.step, land_brush::k_MostStep);
}

TEST(EditorLandBrush, ACountryChangeCommandGivesACountryThatACellCanKeep)
{
	land_brush::Brush brush {.mode = land_brush::Mode::Raise, .country = 2};
	land_brush::TakeCountryChange(brush, 15);
	EXPECT_EQ(brush.country, 15);
	EXPECT_EQ(brush.mode, land_brush::Mode::Raise);
	land_brush::TakeCountryChange(brush, 16);
	EXPECT_EQ(brush.country, 15);
	land_brush::TakeCountryChange(brush, -1);
	EXPECT_EQ(brush.country, 15);
}

TEST(EditorLandBrush, ABrushSizeCommandGivesTheInnerAndTheOuterRadius)
{
	land_brush::Brush brush;
	land_brush::TakeBrushSize(brush, 5.0f, 25.0f);
	EXPECT_FLOAT_EQ(brush.innerRadius, 5.0f);
	EXPECT_FLOAT_EQ(brush.outerRadius, 25.0f);
	land_brush::TakeBrushSize(brush, 30.0f, 10.0f);
	EXPECT_FLOAT_EQ(brush.innerRadius, 30.0f);
	EXPECT_FLOAT_EQ(brush.outerRadius, 30.0f);
	land_brush::TakeBrushSize(brush, -2.0f, 1000.0f);
	EXPECT_FLOAT_EQ(brush.innerRadius, 0.0f);
	EXPECT_FLOAT_EQ(brush.outerRadius, land_brush::k_MostRadius);
}

TEST(EditorPaths, ADraftSkipsAPointTooNearTheLast)
{
	paths::Draft draft;
	EXPECT_FALSE(draft.CanFinish());
	EXPECT_TRUE(draft.Add({0.0f, 5.0f, 0.0f}));
	EXPECT_FALSE(draft.Add({1.0f, 50.0f, 1.0f}));
	EXPECT_TRUE(draft.Add({3.0f, 5.0f, 0.0f}));
	EXPECT_TRUE(draft.CanFinish());
	EXPECT_FLOAT_EQ(paths::Length(draft.Points()), 3.0f);
	draft.RemoveLast();
	EXPECT_FALSE(draft.CanFinish());
	draft.RemoveLast();
	draft.RemoveLast();
	EXPECT_TRUE(draft.Points().empty());
}

TEST(EditorPaths, ANewStreamTakesTheNextNumber)
{
	EXPECT_EQ(paths::NextStreamId({}), 0);
	const std::array ids {0, 4, 2};
	EXPECT_EQ(paths::NextStreamId(ids), 5);
	EXPECT_EQ(paths::PlaceOf({10.0f, 3.0f, 20.0f}), glm::ivec2(65536, 131072));
}

TEST(FeatureScriptWriter, APlaceIsWrittenInMetresWithTwoDecimals)
{
	EXPECT_EQ(lhscriptx::writer::PlaceText({65536 * 100, 0}), "\"1000.00,0.00\"");
	EXPECT_EQ(lhscriptx::writer::PlaceText({32768, 98304}), "\"5.00,15.00\"");
}

TEST(FeatureScriptWriter, ALineHasItsArgumentsInTheFormsOfTheirKinds)
{
	namespace writer = lhscriptx::writer;
	const std::array<writer::Argument, 7> tree {
	    int32_t {-1}, writer::Place {"\"1335.32,2406.63\""}, int32_t {3}, int32_t {1}, 1.076392f, 0.678f, 0.678f};
	EXPECT_EQ(writer::CommandLine("CREATE_NEW_TREE", "NANNFFF", tree),
	          "CREATE_NEW_TREE(-1, \"1335.32,2406.63\", 3, 1, 1.076392, 0.678000, 0.678000)\n");
	const std::array<writer::Argument, 2> feature {writer::Place {"\"1.00,2.00\""}, writer::Name {"Pilar2 Lime"}};
	EXPECT_EQ(writer::CommandLine("F", "AL", feature), "F(\"1.00,2.00\", \"Pilar2 Lime\")\n");
	EXPECT_EQ(writer::CommandLine("NONE", "", {}), "NONE()\n");
	EXPECT_EQ(writer::VersionLine(writer::k_Version), "VERSION(2.300000)\n");
}

TEST(FeatureScriptWriter, AnArgumentOfTheWrongKindWritesNoLine)
{
	namespace writer = lhscriptx::writer;
	const std::array<writer::Argument, 1> number {int32_t {1}};
	EXPECT_EQ(writer::CommandLine("CREATE_STREAM", "F", number), "");
	EXPECT_EQ(writer::CommandLine("CREATE_STREAM", "NN", number), "");
}

TEST(EditorLandSave, TheCellsGoToTheBlocksAtTheSamePlaces)
{
	LandData land;
	land.blocks = {BlockAt(3, 4, 10), BlockAt(5, 6, 10)};
	land.blockIndexLookup.at(3 * 32 + 4) = 1;
	land.blockIndexLookup.at(5 * 32 + 6) = 2;
	const std::array live {BlockAt(5, 6, 77), BlockAt(9, 9, 99)};
	EXPECT_EQ(land_save::CopyCells(land, live), 1u);
	EXPECT_EQ(land.blocks.at(0).cells.at(0).altitude, 10);
	EXPECT_EQ(land.blocks.at(1).cells.at(100).altitude, 77);
}

TEST(EditorLandSave, StreamsFootpathsAndTreesAreWrittenAsTheLandScriptMakesThem)
{
	const std::array places {glm::ivec2(65536, 131072), glm::ivec2(98304, 131072)};
	EXPECT_EQ(land_save::StreamText(2, places),
	          "CREATE_STREAM(2)\nCREATE_STREAM_POINT(2, \"10.00,20.00\")\nCREATE_STREAM_POINT(2, \"15.00,20.00\")\n");
	EXPECT_EQ(land_save::FootpathText(places),
	          "CREATE_FOOTPATH(0)\nCREATE_FOOTPATH_NODE(0, \"10.00,20.00\")\nCREATE_FOOTPATH_NODE(0, \"15.00,20.00\")\n");
	EXPECT_EQ(land_save::TreeText({.forest = -1,
	                               .place = places.front(),
	                               .type = 7,
	                               .nonScenic = true,
	                               .yAngle = 0.5f,
	                               .scale = 1.0f,
	                               .maxScale = 1.25f}),
	          "CREATE_NEW_TREE(-1, \"10.00,20.00\", 7, 1, 0.500000, 1.000000, 1.250000)\n");
	EXPECT_EQ(land_save::FeatureText({.place = places.back(), .name = "Pilar2 Lime", .yAngle = 1.5f, .scale = 0.8f}),
	          "CREATE_NEW_FEATURE(\"15.00,20.00\", \"Pilar2 Lime\", 1500, 800, 0)\n");
	EXPECT_EQ(land_save::FileName("Land1", "lnd"), "Land1.lnd");
	EXPECT_EQ(land_save::FileName("", "txt"), "land.txt");
}

TEST(EditorHistory, TheThingsMadeAreKeptOnceInTheOrderMade)
{
	EditorHistory history;
	const auto first = history.Track(static_cast<entt::entity>(4));
	const auto second = history.Track(static_cast<entt::entity>(9));
	history.Push({.label = "Place", .changes = {MakeChange {.ref = first}}});
	history.Push({.label = "Place", .changes = {MakeChange {.ref = second}, MakeChange {.ref = first}}});
	history.Push({.label = "Move", .changes = {PoseChange {.ref = second}}});
	const std::vector<HistoryRef> expected {first, second};
	EXPECT_EQ(history.MadeRefs(), expected);
	history.Clear();
	EXPECT_TRUE(history.MadeRefs().empty());
}
