/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <3D/LandIslandInterface.h>
#include <ECS/Components/Abode.h>
#include <ECS/Components/Construction.h>
#include <ECS/Components/Footpath.h>
#include <ECS/Components/Mesh.h>
#include <ECS/Components/MiracleImpression.h>
#include <ECS/Components/Mobile.h>
#include <ECS/Components/StreetLantern.h>
#include <ECS/Components/StreetLight.h>
#include <ECS/Components/Temple.h>
#include <ECS/Components/Town.h>
#include <ECS/Components/Transform.h>
#include <ECS/Registry.h>
#include <ECS/TempleHeartOf.h>
#include <ECS/ThingMapPlace.h>
#include <Game.h>
#include <LHScriptX/Script.h>
#include <Locator.h>
#include <gtest/gtest.h>

#include "TestTempFolder.h"

class LoadScene: public ::testing::Test
{
public:
	void LoadTestScene(const char* sceneScript)
	{
		openblack::lhscriptx::Script script;
		script.Load(sceneScript);
	}

protected:
	void SetUp() override
	{
		static const auto mock_game_path = std::filesystem::path(TEST_BINARY_DIR) / "mock";
		auto args = openblack::Arguments {
		    .graphicsBackend = openblack::GraphicsBackend::Noop,
		    .gamePath = mock_game_path.string(),
		    .numFramesToSimulate = 0,
		    .logFile = "stdout",
		    .background = true,
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::debug);
		args.profileFolder = profileFolder_.Path();
		game_ = std::make_unique<openblack::Game>(std::move(args));
		ASSERT_TRUE(game_->Initialize());
	}
	void TearDown() override { game_.reset(); }
	const openblack::test::TestTempFolder profileFolder_;
	std::unique_ptr<openblack::Game> game_;
};

TEST_F(LoadScene, minimal)
{
	LoadTestScene(R""""(
VERSION(2.300000)
)"""");
}

TEST_F(LoadScene, landscape_only)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
)"""");
}

TEST_F(LoadScene, load_abode_no_landscape)
{
	// A place on a line is read with no land under it, as the game reads it. The abode has no town to join.
	const char* sceneScript = R""""(
VERSION(2.300000)
CREATE_ABODE(0, "2224.63,2372.52", "CELTIC_ABODE_F", 11100, 1095, 0, 0)
)"""";
	ASSERT_NO_THROW(LoadTestScene(sceneScript));
}

TEST_F(LoadScene, a_camera_line_before_the_landscape_line_loads_the_land)
{
	// The camera line only keeps its place, so the land loads as the game loads it
	LoadTestScene(R""""(
VERSION(2.200000)
SET_LAND_NUMBER(3)
START_CAMERA_POS("3237.21,3157.85")
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "2829.13,2822.80", "PLAYER_ONE", 3, "CELTIC")
)"""");
	EXPECT_TRUE(openblack::Locator::terrainSystem::value().IsLoaded());
}

TEST_F(LoadScene, load_celtic_abode_f)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_ABODE(0, "2224.63,2372.52", "CELTIC_ABODE_F", 11100, 1095, 0, 0)
)"""");
}

TEST_F(LoadScene, a_thing_made_at_a_place_keeps_the_map_position_that_the_game_reads)
{
	// Far from the corner of the map, the game reads 1280.04 one unit less than the metres give
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "1300.00,1300.00", "PLAYER_ONE", 0, "CELTIC")
CREATE_ABODE(0, "1280.04,1280.04", "CELTIC_ABODE_F", 11100, 1095, 0, 0)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t abodes = 0;
	registry.Each<const openblack::ecs::components::Abode>([&](entt::entity abode, const auto&) {
		EXPECT_EQ(openblack::ecs::thing_map_place::Of(registry, abode),
		          (openblack::map_coords::MapCoords {.x = 8388870, .z = 8388870}));
		++abodes;
	});
	EXPECT_EQ(abodes, 1u);
}

TEST_F(LoadScene, a_town_centre_goes_up_only_where_it_fits_in_its_town)
{
	// The first centre stands on the house of its town, so the script makes nothing for it. The second has free land.
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "1300.00,1300.00", "PLAYER_ONE", 0, "CELTIC")
CREATE_ABODE(0, "1280.04,1280.04", "CELTIC_ABODE_F", 0, 1000, 0, 0)
CREATE_TOWN_CENTRE(0, "1280.04,1280.04", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 300)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t abodes = 0;
	registry.Each<const openblack::ecs::components::Abode>([&abodes](entt::entity, const auto&) { ++abodes; });
	EXPECT_EQ(abodes, 1u);

	LoadTestScene(R""""(
VERSION(2.300000)
CREATE_TOWN_CENTRE(0, "1320.00,1320.00", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 300)
)"""");
	abodes = 0;
	registry.Each<const openblack::ecs::components::Abode>([&abodes](entt::entity, const auto&) { ++abodes; });
	EXPECT_EQ(abodes, 2u);
}

TEST_F(LoadScene, a_temple_goes_up_only_where_no_building_stands_in_its_way)
{
	// The first temple stands on a house, so the script makes nothing for it. The second has free land.
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "1300.00,1300.00", "PLAYER_ONE", 0, "CELTIC")
CREATE_ABODE(0, "1280.04,1280.04", "CELTIC_ABODE_F", 0, 1000, 0, 0)
CREATE_CITADEL("1280.04,1280.04", 0, "PLAYER_ONE", 0, 1000)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t temples = 0;
	registry.Each<const openblack::ecs::components::Temple>([&temples](entt::entity, const auto&) { ++temples; });
	EXPECT_EQ(temples, 0u);

	LoadTestScene(R""""(
VERSION(2.300000)
CREATE_CITADEL("1360.00,1360.00", 0, "PLAYER_ONE", 0, 500)
)"""");
	registry.Each<const openblack::ecs::components::Temple>([&temples, &registry](entt::entity temple, const auto&) {
		// The temple has its full size, whatever size the line gives
		EXPECT_EQ(registry.Get<const openblack::ecs::components::Transform>(temple).scale, glm::vec3(1.0f));
		++temples;
	});
	EXPECT_EQ(temples, 1u);
}

TEST_F(LoadScene, a_temple_is_planned_only_for_a_town_that_is_there)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "1300.00,1300.00", "PLAYER_ONE", 0, "CELTIC")
CREATE_PLANNED_CITADEL(5, "1360.00,1360.00", 0, "PLAYER_ONE", 0, 1000)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t plans = 0;
	registry.Each<const openblack::ecs::components::PlannedTemple>([&plans](entt::entity, const auto&) { ++plans; });
	EXPECT_EQ(plans, 0u);

	LoadTestScene(R""""(
VERSION(2.300000)
CREATE_PLANNED_CITADEL(0, "1360.00,1360.00", 0, "PLAYER_ONE", 0, 1000)
)"""");
	registry.Each<const openblack::ecs::components::PlannedTemple>([&plans](entt::entity, const auto&) { ++plans; });
	EXPECT_EQ(plans, 1u);
}

TEST_F(LoadScene, a_temple_and_its_heart_keep_the_map_position_that_the_game_reads)
{
	// The game makes the temple and its heart at the same map position, the script's
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_CITADEL("1280.04,1280.04", 0, "PLAYER_ONE", 0, 1000)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	const openblack::map_coords::MapCoords place {.x = 8388870, .z = 8388870};
	size_t hearts = 0;
	registry.Each<const openblack::ecs::components::TempleHeart>([&](entt::entity heart, const auto& part) {
		EXPECT_EQ(openblack::ecs::thing_map_place::Of(registry, heart), place);
		EXPECT_EQ(openblack::ecs::thing_map_place::Of(registry, part.temple), place);
		EXPECT_EQ(openblack::ecs::temple_heart::Of(registry, part.temple), heart);
		++hearts;
	});
	EXPECT_EQ(hearts, 1u);
}

TEST_F(LoadScene, unknown_command_is_skipped)
{
	// A land's script may misspell a command, as one multiplayer land does; the game passes over that line and carries
	// out the commands after it
	LoadTestScene(R""""(
VERSION(2.300000)
SET_LAND_NUMBER(3)
CREATE_SPECIAL_TOWNVILLAGER(1,"1885.00,3055.00",82,32)
SET_LAND_NUMBER(7)
)"""");
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 7);
}

TEST_F(LoadScene, unknown_command_keeps_later_commands_on_its_line)
{
	// Everything after an unknown word is passed over up to the next command the game knows, even on the same line
	LoadTestScene(R""""(
SET_LAND_NUMBER(3)
NOT_A_COMMAND(1, "2,2", 4) SET_LAND_NUMBER(9)
)"""");
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 9);
}

TEST_F(LoadScene, unknown_player_name_is_neutral)
{
	// A name that is no player's, such as a tribe's or an eighth player's, is the neutral player; names match in any case
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_TOWN(0, "2224.63,2372.52", "PLAYER_EIGHT", 0, "GREEK")
CREATE_TOWN(1, "2324.63,2372.52", "player_two", 0, "GREEK")
SET_TOWN_BELIEF(1, "GREEK", 0.500000)
)"""");
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	EXPECT_EQ(registry.Get<openblack::ecs::components::Town>(towns.at(0)).owner, openblack::PlayerNames::NEUTRAL);
	EXPECT_EQ(registry.Get<openblack::ecs::components::Town>(towns.at(1)).owner, openblack::PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(registry.Get<openblack::ecs::components::TownImpression>(towns.at(1)).belief.neutral, 0.5f);
}

TEST_F(LoadScene, town_command_for_missing_town_is_skipped)
{
	// A command for a town the land never made does nothing, and the commands after it are carried out
	LoadTestScene(R""""(
VERSION(2.300000)
SET_TOWN_BELIEF(1, "PLAYER_ONE", 1.000000)
SET_TOWN_BELIEF_CAP(1, "PLAYER_TWO", 1.000000)
SET_LAND_NUMBER(4)
)"""");
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 4);
}

TEST_F(LoadScene, command_missing_its_parenthesis_ends_at_its_line)
{
	// The game reads a land's script a line at a time, so a command whose ')' was left out still runs, and so do the
	// commands after it
	LoadTestScene(R""""(
SET_LAND_NUMBER(3
SET_LAND_NUMBER(6)
)"""");
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 6);
	LoadTestScene("SET_LAND_NUMBER(8");
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 8);
}

TEST_F(LoadScene, command_with_too_few_values_stops_the_load_and_keeps_the_commands_before_it)
{
	// The game stops the land's script at a command that has too few values. It keeps what the commands before it did
	// and does not do the commands after it.
	ASSERT_THROW(LoadTestScene(R""""(
SET_LAND_NUMBER(3)
SET_LAND_NUMBER()
SET_LAND_NUMBER(9)
)""""),
	             std::runtime_error);
	EXPECT_EQ(openblack::Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 3);
}

TEST_F(LoadScene, a_bonfire_takes_its_turn_from_the_third_number_and_not_the_second)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_BONFIRE("1425.03,2331.89", 24.700001, 0.000000, 1.000000)
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t bonfires = 0;
	registry.Each<const openblack::ecs::components::MobileStatic, const openblack::ecs::components::Transform>(
	    [&bonfires](const auto& mobileStatic, const auto& transform) {
		    EXPECT_EQ(mobileStatic.type, openblack::MobileStaticInfo::Bonfire);
		    // Land 1's bonfires stand with no turn: the second number is not used
		    EXPECT_EQ(transform.rotation, glm::mat3(1.0f));
		    ++bonfires;
	    });
	EXPECT_EQ(bonfires, 1u);
}

TEST_F(LoadScene, a_street_light_has_no_model_and_is_no_lantern)
{
	// The street light keeps its place only. The lantern line makes a lantern with a model.
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_STREET_LIGHT("2202.51,2412.95")
)"""");
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	size_t lights = 0;
	registry.Each<const openblack::ecs::components::StreetLight>([&](entt::entity light, const auto& component) {
		EXPECT_FALSE(registry.AnyOf<openblack::ecs::components::Mesh>(light));
		EXPECT_FALSE(registry.AnyOf<openblack::ecs::components::Transform>(light));
		EXPECT_NEAR(component.position.x, 2202.51f, 0.01f);
		EXPECT_NEAR(component.position.z, 2412.95f, 0.01f);
		++lights;
	});
	EXPECT_EQ(lights, 1u);
	size_t lanterns = 0;
	registry.Each<const openblack::ecs::components::StreetLantern>([&lanterns](entt::entity, const auto&) { ++lanterns; });
	EXPECT_EQ(lanterns, 0u);
}

TEST_F(LoadScene, a_footpath_node_with_a_third_number_is_above_the_land_by_that_height)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_FOOTPATH(0)
CREATE_FOOTPATH_NODE(0, "1280.04,1280.04,7.5")
CREATE_FOOTPATH_NODE(0, "1290.00,1280.04")
)"""");
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& footpaths = registry.Context().scriptFootpaths;
	ASSERT_EQ(footpaths.size(), 1u);
	const auto& nodes = registry.Get<openblack::ecs::components::Footpath>(footpaths.front()).nodes;
	ASSERT_EQ(nodes.size(), 2u);
	const auto& terrain = openblack::Locator::terrainSystem::value();
	const auto raised = openblack::map_coords::MapCoords {.x = 8388870, .z = 8388870};
	// Each node line puts its node first on the path
	EXPECT_EQ(nodes[1].altitude, 7.5f);
	EXPECT_EQ(nodes[1].position.y, terrain.GetHeightAt(openblack::map_coords::ToMetres(raised)) + 7.5f);
	EXPECT_EQ(nodes[0].altitude, 0.0f);
}

TEST_F(LoadScene, each_footpath_node_line_puts_its_node_first_on_the_path)
{
	LoadTestScene(R""""(
VERSION(2.300000)
LOAD_LANDSCAPE(".\Data\Landscape\Land1.lnd")
CREATE_FOOTPATH(0)
CREATE_FOOTPATH_NODE(0, "1280.00,1280.00")
CREATE_FOOTPATH_NODE(0, "1290.00,1280.00")
CREATE_FOOTPATH_NODE(0, "1300.00,1280.00")
)"""");
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto& footpaths = registry.Context().scriptFootpaths;
	ASSERT_EQ(footpaths.size(), 1u);
	const auto& nodes = registry.Get<openblack::ecs::components::Footpath>(footpaths.front()).nodes;
	ASSERT_EQ(nodes.size(), 3u);
	// The last line's node comes first, the first line's node last
	EXPECT_GT(nodes[0].place.x, nodes[1].place.x);
	EXPECT_GT(nodes[1].place.x, nodes[2].place.x);
	EXPECT_EQ(nodes[0].place.y, nodes[2].place.y);
}
