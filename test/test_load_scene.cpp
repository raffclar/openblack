/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/Components/Abode.h>
#include <ECS/Components/MiracleImpression.h>
#include <ECS/Components/Temple.h>
#include <ECS/Components/Town.h>
#include <ECS/Registry.h>
#include <ECS/TempleHeartOf.h>
#include <ECS/ThingMapPlace.h>
#include <Game.h>
#include <LHScriptX/Script.h>
#include <Locator.h>
#include <gtest/gtest.h>

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
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::debug);
		game_ = std::make_unique<openblack::Game>(std::move(args));
		ASSERT_TRUE(game_->Initialize());
	}
	void TearDown() override { game_.reset(); }
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
	const char* sceneScript = R""""(
VERSION(2.300000)
CREATE_ABODE(0, "2224.63,2372.52", "CELTIC_ABODE_F", 11100, 1095, 0, 0)
)"""";
	ASSERT_THROW(LoadTestScene(sceneScript), std::runtime_error);
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
