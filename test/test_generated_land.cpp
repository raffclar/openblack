/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/GeneratedLand.h"
#include "Audio/SoundMap.h"
#include "LHScriptX/Script.h"

using namespace openblack;
using landgen::countries::Surface;

namespace
{
/// The parts of the story with a plain material for each surface, whose type is the number of the surface
generated_land::StoryParts FakeParts()
{
	generated_land::StoryParts parts;
	for (size_t surface = 0; surface < landgen::countries::k_SurfaceCount; ++surface)
	{
		auto& material = parts.surfaces.emplace_back();
		material.type = static_cast<uint16_t>(surface);
	}
	parts.noise.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, 3);
	parts.bump.assign(static_cast<size_t>(lnd::LNDBumpMap::k_Width) * lnd::LNDBumpMap::k_Height, 0x80);
	return parts;
}
} // namespace

TEST(GeneratedLand, ThePathKeepsTheSeed)
{
	EXPECT_EQ(generated_land::PathOf(42), "landgen:42");
	EXPECT_EQ(generated_land::SeedOf(generated_land::PathOf(18446744073709551615ull)), 18446744073709551615ull);
	EXPECT_FALSE(generated_land::SeedOf("testbed").has_value());
	EXPECT_FALSE(generated_land::SeedOf("landgen:").has_value());
	EXPECT_FALSE(generated_land::SeedOf("landgen:12x").has_value());
	EXPECT_FALSE(generated_land::SeedOf("Data/Landscape/Land1.lnd").has_value());
}

TEST(GeneratedLand, TheFirstChoicesGiveTheFirstSettingsOfTheGenerator)
{
	const auto settings = generated_land::SettingsOf({.seed = 7});
	const landgen::Settings first {.seed = 7};
	EXPECT_EQ(settings.seed, 7u);
	EXPECT_EQ(settings.base.landAmount, first.base.landAmount);
	EXPECT_EQ(settings.base.ranges, first.base.ranges);
	EXPECT_EQ(settings.base.mountainHeight, first.base.mountainHeight);
	EXPECT_EQ(settings.base.plainHeight, first.base.plainHeight);
	EXPECT_EQ(settings.base.hillHeight, first.base.hillHeight);
	EXPECT_EQ(settings.base.hollows, first.base.hollows);
	EXPECT_EQ(settings.hydraulic.drops, first.hydraulic.drops);
	EXPECT_EQ(settings.rivers.sourceFlow, first.rivers.sourceFlow);
	EXPECT_EQ(settings.climate.northTemperature, first.climate.northTemperature);
	EXPECT_EQ(settings.climate.southTemperature, first.climate.southTemperature);
	EXPECT_FALSE(settings.wind.has_value());
	EXPECT_EQ(settings.massifs.countLow, first.massifs.countLow);
	EXPECT_EQ(settings.massifs.countHigh, first.massifs.countHigh);
	EXPECT_EQ(settings.topHeight, first.topHeight);
	EXPECT_EQ(settings.features.treeDensity, first.features.treeDensity);
	EXPECT_EQ(settings.features.most, first.features.most);
	EXPECT_FALSE(settings.landType.type.has_value());
	EXPECT_FALSE(settings.landType.archipelago);
}

TEST(GeneratedLand, TheLandTypeNeedsTheArchipelagoChoiceForAnArchipelago)
{
	using landgen::landtypes::Type;
	EXPECT_EQ(generated_land::SettingsOf({.seed = 3, .landType = 2}).landType.type, Type::Highlands);
	EXPECT_EQ(generated_land::SettingsOf({.seed = 3, .landType = 0}).landType.type, Type::StoryIsland);
	// Without the archipelago choice, the seed picks a type other than an archipelago
	const auto without = generated_land::SettingsOf({.seed = 3, .landType = 4});
	EXPECT_FALSE(without.landType.type.has_value());
	EXPECT_FALSE(without.landType.archipelago);
	const auto with = generated_land::SettingsOf({.seed = 3, .landType = 4, .archipelago = 1});
	EXPECT_EQ(with.landType.type, Type::Archipelago);
	EXPECT_TRUE(with.landType.archipelago);
	EXPECT_EQ(generated_land::PathOf({.seed = 3, .landType = 1}), "landgen:3;type=1");
	EXPECT_EQ(generated_land::ChoicesOf("landgen:3;type=3;archipelago=1"),
	          (generated_land::Choices {.seed = 3, .landType = 3, .archipelago = 1}));
}

TEST(GeneratedLand, EachChoiceGoesToItsSettingOfTheGenerator)
{
	const auto settings = generated_land::SettingsOf({.seed = 9,
	                                                  .landAmount = -20,
	                                                  .ranges = 5,
	                                                  .mountainHeight = 400,
	                                                  .plainHeight = 40,
	                                                  .hillHeight = 10,
	                                                  .hollows = 2,
	                                                  .rain = 250,
	                                                  .riverFlow = 1500,
	                                                  .northTemperature = 10,
	                                                  .southTemperature = 80,
	                                                  .wind = 2,
	                                                  .massifs = 2,
	                                                  .topHeight = 300,
	                                                  .trees = 400});
	EXPECT_EQ(settings.seed, 9u);
	EXPECT_FLOAT_EQ(settings.base.landAmount, -0.2f);
	EXPECT_EQ(settings.base.ranges, 5);
	EXPECT_EQ(settings.base.mountainHeight, 400.0f);
	EXPECT_EQ(settings.base.plainHeight, 40.0f);
	EXPECT_EQ(settings.base.hillHeight, 10.0f);
	EXPECT_EQ(settings.base.hollows, 2);
	EXPECT_EQ(settings.hydraulic.drops, 250000);
	EXPECT_EQ(settings.rivers.sourceFlow, 1500.0f);
	EXPECT_FLOAT_EQ(settings.climate.northTemperature, 0.1f);
	EXPECT_FLOAT_EQ(settings.climate.southTemperature, 0.8f);
	EXPECT_EQ(settings.wind, landgen::climate::Wind::South);
	EXPECT_EQ(settings.massifs.countLow, 2);
	EXPECT_EQ(settings.massifs.countHigh, 2);
	EXPECT_EQ(settings.topHeight, 300.0f);
	EXPECT_FLOAT_EQ(settings.features.treeDensity, 4.0f);
	EXPECT_EQ(settings.features.most, landgen::features::Settings {}.most * 4);
}

TEST(GeneratedLand, AChoiceOutOfItsRangeMovesToTheNearestEnd)
{
	const auto clamped = generated_land::Clamped({.seed = 3, .ranges = 99, .mountainHeight = 5, .wind = -8});
	EXPECT_EQ(clamped.seed, 3u);
	EXPECT_EQ(clamped.ranges, 6);
	EXPECT_EQ(clamped.mountainHeight, 100);
	EXPECT_EQ(clamped.wind, -1);
	// The settings use the values in their ranges
	EXPECT_EQ(generated_land::SettingsOf({.ranges = 99}).base.ranges, 6);
	// Each range holds the first value of its choice
	const generated_land::Choices first;
	for (const auto& range : generated_land::ChoiceRanges())
	{
		EXPECT_LE(range.lowest, first.*range.member) << range.key;
		EXPECT_GE(range.highest, first.*range.member) << range.key;
	}
}

TEST(GeneratedLand, ThePathKeepsEachChoiceThatIsNotAtItsFirstValue)
{
	EXPECT_EQ(generated_land::PathOf(generated_land::Choices {.seed = 7}), "landgen:7");
	const generated_land::Choices changed {.seed = 7, .landAmount = -12, .ranges = 5, .rain = 600, .wind = 0};
	const auto path = generated_land::PathOf(changed);
	EXPECT_EQ(path, "landgen:7;land=-12;ranges=5;rain=600;wind=0");
	EXPECT_EQ(generated_land::ChoicesOf(path), changed);
	EXPECT_EQ(generated_land::SeedOf(path), 7u);
	// Each choice at each end of its range comes back from the path
	for (const auto& range : generated_land::ChoiceRanges())
	{
		for (const auto value : {range.lowest, range.highest})
		{
			generated_land::Choices choices {.seed = 11};
			choices.*range.member = value;
			EXPECT_EQ(generated_land::ChoicesOf(generated_land::PathOf(choices)), choices) << range.key;
		}
	}
}

TEST(GeneratedLand, APathWithAnUnknownOrBadChoiceIsNotAGeneratedLand)
{
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:7;colour=3").has_value());
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:7;ranges=7").has_value());
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:7;ranges").has_value());
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:7;ranges=").has_value());
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:7;").has_value());
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:;ranges=4").has_value());
}

TEST(GeneratedLand, EachSurfaceTakesAMaterialOfTheStory)
{
	for (const auto& source : generated_land::MaterialSources())
	{
		EXPECT_TRUE(source.landscape.ends_with(".lnd"));
	}
}

TEST(GeneratedLand, OnlyTheBlocksWithLandAreKept)
{
	landgen::GeneratedLand land;
	land.cells = landgen::Grid<lnd::LNDCell>(landgen::k_MapCorners, lnd::LNDCell {});
	// Land in the middle of block (10, 12) only
	land.cells.At((10 * 16) + 8, (12 * 16) + 8).altitude = 40;
	land.countries.resize(landgen::biomes::k_BiomeCount);
	land.materials = {Surface::Grass, Surface::Snow};
	const auto parts = FakeParts();

	const auto data = generated_land::Build(land, parts);
	ASSERT_EQ(data.blocks.size(), 1u);
	const auto& block = data.blocks.front();
	EXPECT_EQ(block.blockX, 10u);
	EXPECT_EQ(block.blockZ, 12u);
	EXPECT_EQ(block.cells.at((8 * 17) + 8).altitude, 40);
	EXPECT_EQ(data.blockIndexLookup.at((10 * 32) + 12), 1);
	EXPECT_EQ(std::ranges::count(data.blockIndexLookup, uint16_t {0}), 1023);
	ASSERT_EQ(data.materials.size(), 2u);
	EXPECT_EQ(data.materials[0].type, static_cast<uint16_t>(Surface::Grass));
	EXPECT_EQ(data.materials[1].type, static_cast<uint16_t>(Surface::Snow));
	EXPECT_EQ(data.countries.size(), landgen::biomes::k_BiomeCount);
	EXPECT_EQ(data.noise, parts.noise);
	EXPECT_EQ(data.bump, parts.bump);
}

TEST(GeneratedLand, TheSoundsHaveTheNumbersOfTheGame)
{
	EXPECT_EQ(static_cast<int>(landgen::Sound::Sea), static_cast<int>(audio::AtmosType::Sea));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Coastal), static_cast<int>(audio::AtmosType::Coastal));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Jungle), static_cast<int>(audio::AtmosType::Jungle));
	EXPECT_EQ(static_cast<int>(landgen::Sound::Countryside), static_cast<int>(audio::AtmosType::Countryside));
	EXPECT_EQ(static_cast<int>(landgen::Sound::RunningWater), static_cast<int>(audio::AtmosType::RunningWater));
}

TEST(GeneratedLand, TheTownIsTheLinesOfALandScript)
{
	using landgen::settlement::Building;
	landgen::settlement::Plan plan;
	plan.town.tribe = landgen::settlement::Tribe::Norse;
	plan.citadelX = 100.0f;
	plan.citadelZ = 200.0f;
	plan.citadelRotation = 1234;
	plan.town.x = 195.0f;
	plan.town.z = 200.0f;
	plan.town.buildings = {{.building = Building::TownCentre, .x = 195.0f, .z = 200.0f, .rotation = 10},
	                       {.building = Building::StoragePit, .x = 195.0f, .z = 222.5f, .rotation = 20},
	                       {.building = Building::AbodeC, .x = 217.0f, .z = 200.0f, .rotation = 30}};
	plan.town.villagers = {{.kind = landgen::settlement::Villager::Forester, .x = 218.0f, .z = 201.0f, .age = 30}};
	plan.town.fields = {{.x = 245.0f, .z = 205.0f}};
	const auto script = generated_land::SettlementScript(plan);
	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	std::vector<std::string> names;
	for (const auto& command : read.commands)
	{
		names.push_back(command.name);
	}
	// The town first, the town centre after the other buildings, then the people, the temple and its site
	const std::vector<std::string> expected {"CREATE_TOWN",          "SET_TOWN_BELIEF", "CREATE_NEW_TOWN_FIELD",
	                                         "CREATE_ABODE",         "CREATE_ABODE",    "CREATE_TOWN_CENTRE",
	                                         "CREATE_TOWN_VILLAGER", "CREATE_CITADEL",  "CREATE_WORSHIP_SITE"};
	EXPECT_EQ(names, expected);
	EXPECT_NE(script.find(R"(CREATE_TOWN(0, "195.00,200.00", "PLAYER_ONE", 7, "NORSE"))"), std::string::npos);
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF(0, "PLAYER_ONE", 1.000000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(0, "195.00,222.50", "NORSE_ABODE_STORAGE_PIT", 20, 1000, 20000, 30000))"),
	          std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(0, "217.00,200.00", "NORSE_ABODE_C", 30, 1000, 0, 0))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN_CENTRE(0, "195.00,200.00", "NORSE_ABODE_TOWN_CENTRE", 10, 1000, 0))"),
	          std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN_VILLAGER(0, "218.00,201.00", "NORSE_FORESTER", 30))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_CITADEL("100.00,200.00", 0, "PLAYER_ONE", 1234, 1000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_WORSHIP_SITE("100.00,200.00", 0, "PLAYER_ONE", "NORSE", 0, 1000))"), std::string::npos);
}

TEST(GeneratedLand, EachTribeHasItsNameOfTheGame)
{
	EXPECT_EQ(generated_land::TribeName(landgen::settlement::Tribe::Celtic), "CELTIC");
	EXPECT_EQ(generated_land::TribeName(landgen::settlement::Tribe::Aztec), "AZTEC");
	EXPECT_EQ(generated_land::TribeName(landgen::settlement::Tribe::Egyptian), "EGYPTIAN");
	EXPECT_EQ(generated_land::TribeName(landgen::settlement::Tribe::Tibetan), "TIBETAN");
}

TEST(GeneratedLand, TheNeutralTownsComeAfterTheTemple)
{
	using landgen::settlement::Building;
	landgen::settlement::Plan plan;
	plan.town.tribe = landgen::settlement::Tribe::Norse;
	plan.town.x = 195.0f;
	plan.town.z = 200.0f;
	plan.town.buildings = {{.building = Building::TownCentre, .x = 195.0f, .z = 200.0f, .rotation = 10}};
	landgen::settlement::Town neutral;
	neutral.tribe = landgen::settlement::Tribe::Japanese;
	neutral.x = 600.0f;
	neutral.z = 400.0f;
	neutral.buildings = {{.building = Building::TownCentre, .x = 600.0f, .z = 400.0f, .rotation = 5},
	                     {.building = Building::StoragePit, .x = 620.0f, .z = 400.0f, .rotation = 6},
	                     {.building = Building::Workshop, .x = 600.0f, .z = 422.0f, .rotation = 7},
	                     {.building = Building::Graveyard, .x = 578.0f, .z = 400.0f, .rotation = 8}};
	plan.neutral.push_back(neutral);
	const auto script = generated_land::SettlementScript(plan);
	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	const auto worship = script.find("CREATE_WORSHIP_SITE");
	const auto second = script.find(R"(CREATE_TOWN(1, "600.00,400.00", "NEUTRAL", 3, "JAPANESE"))");
	ASSERT_NE(second, std::string::npos);
	EXPECT_GT(second, worship);
	// A neutral town has the start values of the neutral towns on the skirmish lands
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF(1, "NEUTRAL", 0.500000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(1, "620.00,400.00", "JAPANESE_ABODE_STORAGE_PIT", 6, 1000, 500, 500))"),
	          std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(1, "600.00,422.00", "JAPANESE_ABODE_WORKSHOP", 7, 1000, 0, 0))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(1, "578.00,400.00", "JAPANESE_ABODE_GRAVEYARD", 8, 1000, 0, 0))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN_CENTRE(1, "600.00,400.00", "JAPANESE_ABODE_TOWN_CENTRE", 5, 1000, 0))"),
	          std::string::npos);
	// Only the player's town has a worship site
	EXPECT_EQ(script.find("CREATE_WORSHIP_SITE", worship + 1), std::string::npos);
}

TEST(GeneratedLand, ANeutralTownHasItsPlannedBuildingsAndFishFarms)
{
	using landgen::settlement::Building;
	landgen::settlement::Plan plan;
	plan.town.tribe = landgen::settlement::Tribe::Norse;
	plan.town.buildings = {{.building = Building::TownCentre, .x = 195.0f, .z = 200.0f, .rotation = 10}};
	landgen::settlement::Town neutral;
	neutral.tribe = landgen::settlement::Tribe::Celtic;
	neutral.x = 600.0f;
	neutral.z = 400.0f;
	neutral.buildings = {{.building = Building::TownCentre, .x = 600.0f, .z = 400.0f, .rotation = 5},
	                     {.building = Building::AbodeB, .x = 620.0f, .z = 400.0f, .rotation = 6},
	                     {.building = Building::Wonder, .x = 578.0f, .z = 334.0f, .rotation = 7, .planned = true},
	                     {.building = Building::StoragePit, .x = 600.0f, .z = 422.0f, .rotation = 8, .planned = true}};
	neutral.fishFarms = {{.x = 505.0f, .z = 395.0f}, {.x = 505.0f, .z = 405.0f}};
	plan.neutral.push_back(neutral);
	const auto script = generated_land::SettlementScript(plan);
	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	EXPECT_NE(script.find(R"(CREATE_ABODE(1, "620.00,400.00", "CELTIC_ABODE_B", 6, 1000, 0, 0))"), std::string::npos);
	// A planned building holds no food or wood, and it comes before the buildings that stand
	const auto wonder = script.find(R"(CREATE_PLANNED_ABODE(1, "578.00,334.00", "CELTIC_ABODE_WONDER", 7, 1000, 0, 0))");
	ASSERT_NE(wonder, std::string::npos);
	EXPECT_LT(wonder, script.find("CELTIC_ABODE_B"));
	// The centre of a neutral town comes first, so that it has room
	EXPECT_LT(script.find(R"(CREATE_TOWN_CENTRE(1, "600.00,400.00", "CELTIC_ABODE_TOWN_CENTRE", 5, 1000, 0))"), wonder);
	EXPECT_NE(script.find(R"(CREATE_PLANNED_ABODE(1, "600.00,422.00", "CELTIC_ABODE_STORAGE_PIT", 8, 1000, 0, 0))"),
	          std::string::npos);
	EXPECT_EQ(script.find(R"(CREATE_ABODE(1, "600.00,422.00")"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN_FISH_FARM(1, "505.00,395.00", 0))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN_FISH_FARM(1, "505.00,405.00", 0))"), std::string::npos);
}

TEST(GeneratedLand, ThePlayersChoiceGivesTheCountOfPlayers)
{
	EXPECT_EQ(generated_land::SettingsOf({.seed = 4}).players.count, 1u);
	EXPECT_EQ(generated_land::SettingsOf({.seed = 4, .players = 3}).players.count, 3u);
	// A land for one player keeps the path of before
	EXPECT_EQ(generated_land::PathOf(generated_land::Choices {.seed = 4, .players = 1}), "landgen:4");
	EXPECT_EQ(generated_land::PathOf(generated_land::Choices {.seed = 4, .players = 2}), "landgen:4;players=2");
	EXPECT_EQ(generated_land::Clamped({.players = 9}).players, 4);
	EXPECT_FALSE(generated_land::ChoicesOf("landgen:4;players=0").has_value());
}

TEST(GeneratedLand, EachPlayerGetsATownAndATempleBeforeTheNeutralTowns)
{
	using landgen::settlement::Building;
	const auto planAt = [](float x, landgen::settlement::Tribe tribe) {
		landgen::settlement::Plan plan;
		plan.town.tribe = tribe;
		plan.citadelX = x;
		plan.citadelZ = 100.0f;
		plan.citadelRotation = 50;
		plan.town.x = x + 95.0f;
		plan.town.z = 100.0f;
		plan.town.buildings = {{.building = Building::TownCentre, .x = x + 95.0f, .z = 100.0f, .rotation = 1},
		                       {.building = Building::StoragePit, .x = x + 95.0f, .z = 122.0f, .rotation = 2}};
		return plan;
	};
	auto plan = planAt(100.0f, landgen::settlement::Tribe::Norse);
	landgen::settlement::Town neutral;
	neutral.tribe = landgen::settlement::Tribe::Greek;
	neutral.x = 3000.0f;
	neutral.z = 3000.0f;
	neutral.buildings = {{.building = Building::TownCentre, .x = 3000.0f, .z = 3000.0f, .rotation = 3}};
	plan.neutral.push_back(neutral);
	const std::array others {planAt(1000.0f, landgen::settlement::Tribe::Celtic),
	                         planAt(2000.0f, landgen::settlement::Tribe::Japanese)};
	const auto script = generated_land::SettlementScript(plan, others);
	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	// The towns of players two and three have the numbers after the town of player one, with the start values of player
	// one, and each player has a temple and a worship site
	EXPECT_NE(script.find(R"(CREATE_TOWN(1, "1095.00,100.00", "PLAYER_TWO", 0, "CELTIC"))"), std::string::npos);
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF(1, "PLAYER_TWO", 1.000000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_ABODE(1, "1095.00,122.00", "CELTIC_ABODE_STORAGE_PIT", 2, 1000, 20000, 30000))"),
	          std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_CITADEL("1000.00,100.00", 0, "PLAYER_TWO", 50, 1000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_WORSHIP_SITE("1000.00,100.00", 0, "PLAYER_TWO", "CELTIC", 0, 1000))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_TOWN(2, "2095.00,100.00", "PLAYER_THREE", 3, "JAPANESE"))"), std::string::npos);
	EXPECT_NE(script.find(R"(CREATE_CITADEL("2000.00,100.00", 0, "PLAYER_THREE", 50, 1000))"), std::string::npos);
	// The town centre of another player comes before its other buildings, so that they leave room for it
	const auto centreTwo = script.find(R"(CREATE_TOWN_CENTRE(1, "1095.00,100.00", "CELTIC_ABODE_TOWN_CENTRE", 1, 1000, 0))");
	ASSERT_NE(centreTwo, std::string::npos);
	EXPECT_LT(centreTwo, script.find(R"(CREATE_ABODE(1, "1095.00,122.00")"));
	// The town centre of player one stays after its other buildings
	EXPECT_GT(script.find(R"(CREATE_TOWN_CENTRE(0, )"), script.find(R"(CREATE_ABODE(0, )"));
	// The neutral town comes last, with the next number
	const auto third = script.find(R"(CREATE_WORSHIP_SITE("2000.00,100.00", 0, "PLAYER_THREE", "JAPANESE", 0, 1000))");
	const auto neutralTown = script.find(R"(CREATE_TOWN(3, "3000.00,3000.00", "NEUTRAL", 6, "GREEK"))");
	ASSERT_NE(third, std::string::npos);
	ASSERT_NE(neutralTown, std::string::npos);
	EXPECT_GT(neutralTown, third);
}

namespace
{
/// The plan of a start with its temple at x and its town 95 m east of it
landgen::settlement::Plan StartAt(float x)
{
	landgen::settlement::Plan plan;
	plan.town.tribe = landgen::settlement::Tribe::Celtic;
	plan.citadelX = x;
	plan.citadelZ = 100.0f;
	plan.town.x = x + 95.0f;
	plan.town.z = 100.0f;
	return plan;
}

/// The count of the lines of the script that start with the text
size_t CountOf(const std::string& script, std::string_view start)
{
	size_t count = 0;
	for (size_t at = script.find(start); at != std::string::npos; at = script.find(start, at + 1))
	{
		count += at == 0 || script[at - 1] == '\n' ? 1 : 0;
	}
	return count;
}
} // namespace

TEST(GeneratedLand, ALandForOnePlayerHasNoComputerGod)
{
	auto plan = StartAt(100.0f);
	plan.neutral.push_back(StartAt(2000.0f).town);
	const auto script = generated_land::SettlementScript(plan);
	EXPECT_EQ(CountOf(script, "TOGGLE_COMPUTER_PLAYER"), 0u);
	EXPECT_EQ(CountOf(script, "CREATE_CREATURE_FROM_FILE"), 0u);
	EXPECT_EQ(CountOf(script, "SET_COMPUTER_PLAYER_CREATURE_LIKE"), 0u);
	EXPECT_EQ(CountOf(script, "SET_TOWN_BELIEF_CAP"), 0u);
}

TEST(GeneratedLand, EachOtherPlayerIsAComputerGodAsOnTheSkirmishLands)
{
	auto plan = StartAt(100.0f);
	plan.neutral.push_back(StartAt(3000.0f).town);
	const std::array others {StartAt(1000.0f)};
	const auto script = generated_land::SettlementScript(plan, others);
	const auto read = lhscriptx::Script::Read(script);
	ASSERT_FALSE(read.error.has_value()) << *read.error;
	// As on Two Gods: the god switches on, then it gets a leopard from the mind file of the computer creature and its
	// creature grows like the creature of player one. Then its town comes.
	const auto toggle = script.find(R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1))");
	const auto creature =
	    script.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 3, "ComputerControlledCreature", "1000.00,100.00"))");
	const auto like = script.find(R"(SET_COMPUTER_PLAYER_CREATURE_LIKE("PLAYER_TWO", "PLAYER_ONE"))");
	const auto town = script.find(R"(CREATE_TOWN(1, "1095.00,100.00", "PLAYER_TWO", 0, "CELTIC"))");
	ASSERT_NE(toggle, std::string::npos);
	ASSERT_NE(creature, std::string::npos);
	ASSERT_NE(like, std::string::npos);
	ASSERT_NE(town, std::string::npos);
	EXPECT_GT(toggle, script.find(R"(CREATE_WORSHIP_SITE("100.00,100.00", 0, "PLAYER_ONE")"));
	EXPECT_LT(toggle, creature);
	EXPECT_LT(creature, like);
	EXPECT_LT(like, town);
	// Each town limits the belief in player two to 1, but its own town to 2, as on Two Gods. Player one has no cap.
	EXPECT_EQ(CountOf(script, "SET_TOWN_BELIEF_CAP"), 3u);
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF_CAP(0, "PLAYER_TWO", 1.000000))"), std::string::npos);
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF_CAP(1, "PLAYER_TWO", 2.000000))"), std::string::npos);
	EXPECT_NE(script.find(R"(SET_TOWN_BELIEF_CAP(2, "PLAYER_TWO", 1.000000))"), std::string::npos);
	EXPECT_EQ(script.find(R"(SET_TOWN_BELIEF_CAP(0, "PLAYER_ONE")"), std::string::npos);
	// The cap comes just after the town, before its belief
	EXPECT_LT(script.find(R"(SET_TOWN_BELIEF_CAP(1, )"), script.find(R"(SET_TOWN_BELIEF(1, )"));
	EXPECT_GT(script.find(R"(SET_TOWN_BELIEF_CAP(1, )"), town);
}

TEST(GeneratedLand, TheComputerGodsTakeTheCreaturesOfTheSkirmishLandWithTheSameCountOfPlayers)
{
	const auto plan = StartAt(100.0f);
	// Three Gods: a lion and a horse, a cap of 1 in each town
	const std::array three {StartAt(1000.0f), StartAt(2000.0f)};
	const auto threeScript = generated_land::SettlementScript(plan, three);
	EXPECT_NE(threeScript.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 5, "ComputerControlledCreature", "1000.00,100.00"))"),
	          std::string::npos);
	EXPECT_NE(
	    threeScript.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_THREE", 6, "ComputerControlledCreature", "2000.00,100.00"))"),
	    std::string::npos);
	EXPECT_EQ(CountOf(threeScript, "SET_TOWN_BELIEF_CAP"), 6u);
	EXPECT_EQ(threeScript.find("2.000000))"), std::string::npos);
	// Four Gods: a horse, a tortoise and a zebra
	const std::array four {StartAt(1000.0f), StartAt(2000.0f), StartAt(3000.0f)};
	const auto fourScript = generated_land::SettlementScript(plan, four);
	EXPECT_NE(fourScript.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 6, )"), std::string::npos);
	EXPECT_NE(fourScript.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_THREE", 7, )"), std::string::npos);
	EXPECT_NE(fourScript.find(R"(CREATE_CREATURE_FROM_FILE("PLAYER_FOUR", 8, )"), std::string::npos);
	EXPECT_EQ(CountOf(fourScript, "TOGGLE_COMPUTER_PLAYER"), 3u);
	EXPECT_EQ(CountOf(fourScript, "SET_COMPUTER_PLAYER_CREATURE_LIKE"), 3u);
	EXPECT_EQ(CountOf(fourScript, "SET_TOWN_BELIEF_CAP"), 12u);
	EXPECT_NE(fourScript.find(R"(SET_TOWN_BELIEF_CAP(3, "PLAYER_FOUR", 1.000000))"), std::string::npos);
}
