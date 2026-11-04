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
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/CreatureFromFile.h"
#include "ECS/TownDesire.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorPaths.h"
#include "Editor/LandBrush.h"
#include "Editor/LandSave.h"
#include "FileSystem/MemoryStream.h"
#include "LHScriptX/FeatureScriptWriter.h"
#include "LHScriptX/Script.h"
#include "Serializer/FotFile.h"
#include "Serializer/GameThingSerializer.h"

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

TEST(FeatureScriptWriter, ANumberWithAFractionCanBeADoubleForAWholeNumberAboveTheFloats)
{
	const std::array<lhscriptx::writer::Argument, 2> arguments {0.5f, 16777217.0};
	EXPECT_EQ(lhscriptx::writer::CommandLine("X", "FF", arguments), "X(0.500000, 16777217.000000)\n");
}

TEST(EditorLandSave, MobileStaticsAndMobileObjectsAreWrittenAsTheGameWritesThem)
{
	const glm::ivec2 place {65536, 131072};
	EXPECT_EQ(land_save::MobileStaticText(
	              {.place = place, .row = 2, .altitude = 0.5f, .angles = glm::vec3(0.25f, -1.5f, 0.125f), .scale = 0.267f}),
	          "CREATE_MOBILE_STATIC(\"10.00,20.00\", 2, 0.500000, 0.250000, -1.500000, 0.125000, 0.267000)\n");
	// Each product rounds to a float before the cut: 0.9 times 1000 is 900, not 899
	EXPECT_EQ(land_save::MobileObjectText({.place = place, .row = 5, .yAngle = 1.2345f, .scale = 0.9f}),
	          "CREATE_MOBILEOBJECT(\"10.00,20.00\", 5, 1234, 900)\n");
}

TEST(EditorLandSave, AVillagerIsWrittenInOneOfThreeLines)
{
	land_save::VillagerLine villager {.place = {65536, 131072}, .town = 3, .row = 12, .name = "NORSE_FORESTER", .age = 21};
	EXPECT_EQ(land_save::VillagerText(villager), "CREATE_TOWN_VILLAGER(3, \"10.00,20.00\", NORSE_FORESTER, 21)\n");
	villager.door = glm::ivec2(98304, 131072);
	EXPECT_EQ(land_save::VillagerText(villager),
	          "CREATE_VILLAGER_POS(\"15.00,20.00\", \"10.00,20.00\", \"NORSE_FORESTER\", 21)\n");
	villager.special = true;
	EXPECT_EQ(land_save::VillagerText(villager), "CREATE_SPECIAL_TOWN_VILLAGER(3, \"10.00,20.00\", 12, 21)\n");
}

TEST(EditorLandSave, DispensersBubblesAndCreaturesAreWrittenAsTheGameWritesThem)
{
	const glm::ivec2 place {65536, 131072};
	EXPECT_EQ(land_save::DispenserText({.town = 0,
	                                    .place = place,
	                                    .building = "JAPANESE_ABODE_SPELL_DISPENSER",
	                                    .miracle = "FIRE",
	                                    .yAngle = 0.0f,
	                                    .scale = 1.0f,
	                                    .period = 300}),
	          "CREATE_SPELL_DISPENSER(0, \"10.00,20.00\", \"JAPANESE_ABODE_SPELL_DISPENSER\", \"FIRE\", 0.000000, "
	          "1.000000, 300.000000)\n");
	EXPECT_EQ(land_save::OneShotMiracleText(place, "HEAL"), "CREATE_ONE_SHOT_SPELL_PU(\"10.00,20.00\", \"HEAL\")\n");
	EXPECT_EQ(land_save::CreatureText(place, 2, 0), "CREATE_CREATURE(\"10.00,20.00\", 2, 0)\n");
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

TEST(EditorLandSave, BuildingsAndTownCentresAreWrittenAsTheGameWritesThem)
{
	land_save::AbodeLine abode {.town = 4,
	                            .place = {65536, 131072},
	                            .name = "NORSE_ABODE_A",
	                            .yAngle = 1.2345f,
	                            .scale = 0.9f,
	                            .food = 100,
	                            .wood = 25};
	EXPECT_EQ(land_save::AbodeText(abode), "CREATE_ABODE(4, \"10.00,20.00\", \"NORSE_ABODE_A\", 1234, 900, 100, 25)\n");
	abode.planned = true;
	EXPECT_EQ(land_save::AbodeText(abode), "CREATE_PLANNED_ABODE(4, \"10.00,20.00\", \"NORSE_ABODE_A\", 1234, 900, 100, 25)\n");
	EXPECT_EQ(land_save::TownCentreText({.town = 4,
	                                     .place = {98304, 131072},
	                                     .name = "NORSE_ABODE_TOWN_CENTRE",
	                                     .yAngle = 0.5f,
	                                     .scale = 1.0f,
	                                     .worshipShare = 0.3f}),
	          "CREATE_TOWN_CENTRE(4, \"15.00,20.00\", \"NORSE_ABODE_TOWN_CENTRE\", 500, 1000, 300)\n");
}

TEST(EditorLandSave, ATownWritesOnlyTheLinesOfWhatIsNotItsDefault)
{
	land_save::TownLine town {.id = 4,
	                          .place = {65536, 131072},
	                          .player = "PLAYER_ONE",
	                          .tribe = 7,
	                          .tribeName = "NORSE",
	                          .beliefs = {{.player = "PLAYER_ONE", .belief = 0.0f, .cap = 10.0f},
	                                      {.player = "NEUTRAL", .belief = 0.0f, .cap = 10.0f}},
	                          .desires = {{.desire = "Food", .boost = 0.0f}}};
	EXPECT_EQ(land_save::TownText(town), "CREATE_TOWN(4, \"10.00,20.00\", \"PLAYER_ONE\", 7, \"NORSE\")\n");
	town.miracles = {"FOOD", "HEAL"};
	town.beliefs.front().belief = 2.5f;
	town.beliefs.back().cap = 2.0f;
	town.desires.push_back({.desire = "Abodes", .boost = -0.75f});
	town.uninhabitable = true;
	town.balanceBeliefScale = 0.5f;
	town.congregation = glm::ivec2(98304, 131072);
	EXPECT_EQ(land_save::TownText(town), "CREATE_TOWN(4, \"10.00,20.00\", \"PLAYER_ONE\", 7, \"NORSE\")\n"
	                                     "CREATE_NEW_TOWN_SPELL(4, \"FOOD\")\n"
	                                     "CREATE_NEW_TOWN_SPELL(4, \"HEAL\")\n"
	                                     "SET_TOWN_BELIEF(4, \"PLAYER_ONE\", 2.500000)\n"
	                                     "SET_TOWN_BELIEF_CAP(4, \"NEUTRAL\", 2.000000)\n"
	                                     "TOWN_DESIRE_BOOST(4, \"Abodes\", -0.750000)\n"
	                                     "SET_TOWN_UNINHABITABLE(4)\n"
	                                     "SET_TOWN_BALANCE_BELIEF_SCALE(4, 0.500000)\n"
	                                     "SET_TOWN_CONGREGATION_POS(4, \"15.00,20.00\")\n");
	// A gathering place at 0, 0 is no place
	town.congregation = glm::ivec2(0, 0);
	EXPECT_EQ(land_save::TownText(town).find("SET_TOWN_CONGREGATION_POS"), std::string::npos);
	EXPECT_EQ(land_save::PlayerHeading("PLAYER_ONE"), "\nrem *********************** PLAYER_ONE ***********************\n");
	EXPECT_EQ(land_save::TownHeading(4), "rem ---------------- Town: 4 ----------------\n");
}

TEST(EditorLandSave, ADesireNameReadsBackAsTheSameDesire)
{
	for (size_t desire = 0; desire < ecs::town_desire::k_Count; ++desire)
	{
		EXPECT_EQ(ecs::town_desire::DesireByName(ecs::town_desire::DesireName(desire)), desire);
	}
	EXPECT_TRUE(ecs::town_desire::DesireName(ecs::town_desire::k_Count).empty());
}

TEST(EditorLandSave, AWrittenLandScriptReadsBackAsTheSameWorld)
{
	// Write a town with a building and a centre, a creature, a stream and a footpath, then read the lines back
	const std::array places {glm::ivec2(65536, 131072), glm::ivec2(98304, 131072)};
	land_save::TownLine town {.id = 3, .place = places.front(), .player = "PLAYER_TWO", .tribe = 3, .tribeName = "JAPANESE"};
	town.beliefs = {{.player = "PLAYER_TWO", .belief = 1.5f, .cap = 10.0f}};
	std::string script = lhscriptx::writer::VersionLine(lhscriptx::writer::k_Version);
	script += land_save::PlayerHeading("PLAYER_TWO");
	script += land_save::TownHeading(3);
	script += land_save::TownText(town);
	script += land_save::AbodeText(
	    {.town = 3, .place = places.back(), .name = "JAPANESE_ABODE_A", .yAngle = 0.25f, .scale = 1.0f, .food = 7, .wood = 9});
	script += land_save::TownCentreText(
	    {.town = 3, .place = places.front(), .name = "JAPANESE_ABODE_TOWN_CENTRE", .scale = 1.0f, .worshipShare = 0.5f});
	script += land_save::CreatureText(places.back(), 2, 1);
	script += land_save::StreamText(0, places);
	script += land_save::FootpathText(places);

	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	std::vector<std::string> names;
	for (const auto& command : read.commands)
	{
		names.push_back(command.name);
	}
	const std::vector<std::string> expected {"VERSION",         "CREATE_TOWN",          "SET_TOWN_BELIEF",
	                                         "CREATE_ABODE",    "CREATE_TOWN_CENTRE",   "CREATE_CREATURE",
	                                         "CREATE_STREAM",   "CREATE_STREAM_POINT",  "CREATE_STREAM_POINT",
	                                         "CREATE_FOOTPATH", "CREATE_FOOTPATH_NODE", "CREATE_FOOTPATH_NODE"};
	EXPECT_EQ(names, expected);

	const auto& townLine = read.commands.at(1).arguments;
	ASSERT_EQ(townLine.size(), 5u);
	EXPECT_EQ(*townLine.at(0).IntegerValue(), 3);
	EXPECT_EQ(townLine.at(1).StringValue(), "10.00,20.00");
	EXPECT_EQ(townLine.at(2).StringValue(), "PLAYER_TWO");
	EXPECT_EQ(*townLine.at(3).IntegerValue(), 3);
	EXPECT_EQ(townLine.at(4).StringValue(), "JAPANESE");
	const auto& belief = read.commands.at(2).arguments;
	ASSERT_EQ(belief.size(), 3u);
	EXPECT_EQ(*belief.at(2).FloatValue(), 1.5f);
	const auto& abode = read.commands.at(3).arguments;
	ASSERT_EQ(abode.size(), 7u);
	EXPECT_EQ(abode.at(2).StringValue(), "JAPANESE_ABODE_A");
	EXPECT_EQ(*abode.at(3).IntegerValue(), 250);
	EXPECT_EQ(*abode.at(4).IntegerValue(), 1000);
	EXPECT_EQ(*abode.at(5).IntegerValue(), 7);
	EXPECT_EQ(*abode.at(6).IntegerValue(), 9);
	const auto& centre = read.commands.at(4).arguments;
	ASSERT_EQ(centre.size(), 6u);
	EXPECT_EQ(*centre.at(5).IntegerValue(), 500);
	const auto& creature = read.commands.at(5).arguments;
	ASSERT_EQ(creature.size(), 3u);
	EXPECT_EQ(creature.at(0).StringValue(), "15.00,20.00");
	EXPECT_EQ(*creature.at(1).IntegerValue(), 2);
	EXPECT_EQ(*creature.at(2).IntegerValue(), 1);

	// The paths read back with the texts of their places in order
	const auto paths = land_save::ReadSavedPaths(script);
	const std::vector<std::string> texts {"10.00,20.00", "15.00,20.00"};
	ASSERT_EQ(paths.streams.size(), 1u);
	EXPECT_EQ(paths.streams.front(), texts);
	ASSERT_EQ(paths.footpaths.size(), 1u);
	EXPECT_EQ(paths.footpaths.front(), texts);
}

TEST(EditorLandSave, APlainCreatureComesOnlyForAPlayerWithNoCreature)
{
	using ecs::creature_from_file::ScriptMakesPlainCreature;
	EXPECT_TRUE(ScriptMakesPlainCreature(true, true, true, false));
	EXPECT_FALSE(ScriptMakesPlainCreature(true, true, true, true));
	EXPECT_FALSE(ScriptMakesPlainCreature(false, true, true, false));
	EXPECT_FALSE(ScriptMakesPlainCreature(true, false, true, false));
	EXPECT_FALSE(ScriptMakesPlainCreature(true, true, false, false));
}

TEST(EditorLandSave, AWrittenFootpathFileReadsBackWithItsLinksAndPaths)
{
	// The reader of the file logs to the game logger
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_st>("game");
	}
	const std::vector<fot_file::Path> paths {
	    {.nodes = {{.place = {65536, 131072}, .altitude = 0.0f, .flags = 1},
	               {.place = {98304, 131072}, .altitude = 0.25f, .flags = 3}},
	     .walkable = true},
	    {.nodes = {{.place = {1000, 2000}, .altitude = 0.0f, .flags = 1}}, .walkable = false},
	};
	const std::vector<fot_file::LinkSave> links {
	    {.place = {65536, 131072}, .altitude = 0.5f, .paths = std::vector<size_t> {1, 0}},
	    {.place = {7, 8}, .altitude = 0.0f, .paths = std::nullopt},
	    {.place = {98304, 131072}, .altitude = -0.5f, .paths = std::vector<size_t> {0}},
	};
	auto bytes = fot_file::Write(links, paths);
	filesystem::MemoryStream stream(std::move(bytes));
	serializer::GameThingSerializer reader(stream);
	const auto saves = reader.DeserializeList<serializer::GameThingSerializer::FootpathLinkSave>();
	const auto footpaths = reader.DeserializeList<serializer::GameThingSerializer::Footpath>();
	EXPECT_TRUE(stream.IsEndOfFile());

	// The thing with no link gives no link save
	ASSERT_EQ(saves.size(), 2u);
	EXPECT_EQ(saves.at(0).coords.x, 65536u);
	EXPECT_EQ(saves.at(0).coords.altitude, 0.5f);
	ASSERT_EQ(saves.at(0).link.footpaths.size(), 2u);
	EXPECT_EQ(saves.at(0).link.footpaths.at(0).nodes.size(), 1u);
	EXPECT_EQ(saves.at(0).link.footpaths.at(1).nodes.size(), 2u);
	ASSERT_EQ(saves.at(1).link.footpaths.size(), 1u);
	EXPECT_EQ(saves.at(1).link.footpaths.front(), saves.at(0).link.footpaths.at(1));

	ASSERT_EQ(footpaths.size(), 2u);
	EXPECT_EQ(footpaths.at(0), saves.at(0).link.footpaths.at(1));
	EXPECT_EQ(footpaths.at(0).unknown, 1u);
	EXPECT_EQ(footpaths.at(1).unknown, 0u);
	const auto& node = footpaths.at(0).nodes.at(1);
	EXPECT_EQ(node.coords.x, 98304u);
	EXPECT_EQ(node.coords.z, 131072u);
	EXPECT_EQ(node.coords.altitude, 0.25f);
	EXPECT_EQ(node.unknown, 3u);
}

TEST(EditorLandSave, TheWholeLandIsWrittenAfterTheVersion)
{
	const auto text = land_save::GameText({.landNumber = 2,
	                                       .landscape = "C:/Profile/Editor/land2.lnd",
	                                       .townInfluenceMultiplier = 1.5f,
	                                       .playerInfluenceMultiplier = 0.5f});
	EXPECT_EQ(text, "SET_LAND_NUMBER(2)\n"
	                "LOAD_LANDSCAPE(\"C:/Profile/Editor/land2.lnd\")\n"
	                "SET_TOWN_INFLUENCE_MULTIPLIER(1.500000)\n"
	                "SET_PLAYER_INFLUENCE_MULTIPLIER(0.500000)\n");
	// A land with no landscape file gets no landscape line
	EXPECT_EQ(land_save::GameText({}).find("LOAD_LANDSCAPE"), std::string::npos);
}

TEST(EditorLandSave, AScriptWithNoLandscapeLineLoadsNoLandscape)
{
	const auto saved = land_save::GameText({.landNumber = 1, .landscape = "land1.lnd"});
	EXPECT_TRUE(lhscriptx::LoadsLandscape(lhscriptx::Script::Read("VERSION(2.300000)\n" + saved)));
	EXPECT_FALSE(lhscriptx::LoadsLandscape(lhscriptx::Script::Read("VERSION(2.300000)\n" + land_save::GameText({}))));
	// A landscape line after a line that stops the read does not load
	EXPECT_FALSE(lhscriptx::LoadsLandscape(lhscriptx::Script::Read("VERSION 2.3\nLOAD_LANDSCAPE(\"land1.lnd\")\n")));
}

TEST(EditorLandSave, AFieldIsWrittenWithItsTownOrWithNone)
{
	const glm::ivec2 place {65536, 131072};
	EXPECT_EQ(land_save::FieldText({.town = 4, .place = place, .row = 1, .yAngle = 0.5f}),
	          "CREATE_NEW_TOWN_FIELD(4, \"10.00,20.00\", 1, 0.500000)\n");
	EXPECT_EQ(land_save::FieldText({.town = -1, .place = place, .row = 1, .yAngle = 0.5f}),
	          "CREATE_FIELD(\"10.00,20.00\", 1)\n");
}

TEST(EditorLandSave, TheListOfTheThingsMadeReadsBackInItsOrder)
{
	const std::vector made {static_cast<entt::entity>(9), static_cast<entt::entity>(0x100004), static_cast<entt::entity>(2)};
	EXPECT_EQ(land_save::ReadMade(land_save::MadeText(made)), made);
	// Line ends with a carriage return read, and a line that is not a number is passed over
	const std::vector expected {static_cast<entt::entity>(7), static_cast<entt::entity>(8)};
	EXPECT_EQ(land_save::ReadMade("7\r\nseven\n\n8"), expected);
}

TEST(EditorHistory, TheThingsKeptAtTheNextClearBecomeTheThingsMade)
{
	EditorHistory history;
	const auto old = history.Track(static_cast<entt::entity>(4));
	history.Push({.label = "Place", .changes = {MakeChange {.ref = old}}});
	history.KeepMadeAtNextClear({static_cast<entt::entity>(12), static_cast<entt::entity>(5)});
	history.Clear();
	ASSERT_EQ(history.MadeRefs().size(), 2u);
	EXPECT_EQ(history.Resolve(history.MadeRefs()[0]), static_cast<entt::entity>(12));
	EXPECT_EQ(history.Resolve(history.MadeRefs()[1]), static_cast<entt::entity>(5));
	EXPECT_FALSE(history.CanUndo());
	// The list is kept for one clear only
	history.Clear();
	EXPECT_TRUE(history.MadeRefs().empty());
}
