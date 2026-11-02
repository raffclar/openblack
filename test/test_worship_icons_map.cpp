/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureObstacles.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/TownPlanning.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/MapProduction.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace mind_detail = openblack::ecs::systems::mind_detail;
namespace kinds = openblack::creature_tree::belief_types;

namespace
{

constexpr glm::vec3 k_IconPlace {505.0f, 3.0f, 505.0f};

/// An icon of a worship site of a player, at a place on the land
entt::entity MakeSiteIcon(Registry& registry, PlayerNames player)
{
	const auto site = registry.Create();
	registry.Assign<WorshipSite>(site, WorshipSite {.player = player});
	const auto icon = registry.Create();
	registry.Assign<Transform>(icon, k_IconPlace, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipIcon>(icon, WorshipIcon {.site = site, .seed = SpellSeedType::Heal, .place = 0});
	return icon;
}

class WorshipIconsInTheMap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		// The icon's model covers a circle of 4.88 units in the middle of its cell
		Locator::entitiesMap::emplace<MapProduction>([](const Registry& /*registry*/, entt::entity /*entity*/) {
			return std::optional(
			    map_cells::OutlineOfBox(k_IconPlace, glm::mat3(1.0f), 1.0f, glm::vec3(0.0f), {4.88f, 1.0f, 4.88f}));
		});
	}
	void TearDown() override
	{
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}
};

} // namespace

// A worship site's icon stands in the land as a building does: it goes into the front of the fixed things of each cell
// that its model's outline covers, and it leaves them when it goes
TEST_F(WorshipIconsInTheMap, AnIconIsInEachCellItsOutlineCoversAndLeavesThemWhenGone)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_ONE);
	map.Sync();
	const auto* resident = registry.TryGet<const MapCellResident>(icon);
	ASSERT_NE(resident, nullptr);
	EXPECT_TRUE(resident->coversOutline);
	EXPECT_FALSE(resident->moves);
	const auto middle = map_coords::CellOf(k_IconPlace);
	for (const auto offset : {glm::ivec2 {0, 0}, glm::ivec2 {-1, 0}, glm::ivec2 {1, 0}, glm::ivec2 {0, -1}, glm::ivec2 {0, 1}})
	{
		const auto fixed = map.GetFixedInGridCell(MapInterface::CellId(middle + offset));
		ASSERT_FALSE(fixed.empty());
		EXPECT_EQ(fixed.front(), icon);
	}
	registry.Destroy(icon);
	EXPECT_TRUE(map.GetAllInCell(middle).empty());
}

// The game asks each fixed thing in the outline of a new building whether it is solid to the building. An icon of a
// worship site is, as every building is.
TEST_F(WorshipIconsInTheMap, AnIconStopsANewBuilding)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_ONE);
	const auto kind = MapProduction::KindOf(registry, icon);
	ASSERT_TRUE(kind.has_value());
	EXPECT_TRUE(kind->coversOutline);
	EXPECT_TRUE(town_planning::SolidToNewBuilding(registry, icon));
}

// A creature walks round an icon as round a building, and the footpaths go round it
TEST(WorshipIconsAsObstacles, ACreatureAndTheFootpathsGoRoundAnIcon)
{
	Registry registry;
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_ONE);
	auto thing = creature_obstacle_things::ThingOf(registry, icon);
	ASSERT_TRUE(thing.has_value());
	EXPECT_EQ(thing->avoidance, creature_obstacles::Avoidance::Building);
	EXPECT_EQ(thing->shape, creature_obstacles::Shape::ModelBox);
	EXPECT_FALSE(thing->justBegun);
	EXPECT_TRUE(creature_obstacles::MustAvoidWithoutWalker(*thing));
	// The walkers go round the outline of its model's box
	EXPECT_EQ(thing_circles::ShapeOf(registry, icon), wall_hug::ThingShape::ModelBox);
}

// An icon casts its shape on the land from when the game makes it
TEST(WorshipIconsAsObstacles, AnIconCastsItsShapeOnTheLand)
{
	Registry registry;
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(LandShadowOf(registry, icon), LandShadow::Shape);
}

// A creature that finds an icon believes in it as the icon of a miracle, of the player of its site
TEST(WorshipIconsAsObstacles, ACreatureBelievesInAnIconOfThePlayerOfItsSite)
{
	Registry registry;
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, icon), kinds::k_SpellIcon);
	const auto belief = mind_detail::BeliefNowOf(registry, icon, entt::null);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->Value(creature_tree::Attribute::PlayerNumber), static_cast<uint32_t>(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(belief->Value(creature_tree::Attribute::Origin), creature_tree::origin::k_Natural);
}

// An icon takes no effect of a miracle, so no miracle may destroy it
TEST_F(WorshipIconsInTheMap, NoMiracleMayDestroyAnIcon)
{
	const bool madeLogger = spdlog::get("game") == nullptr;
	if (madeLogger)
	{
		spdlog::register_logger(std::make_shared<spdlog::logger>("game", std::make_shared<spdlog::sinks::null_sink_mt>()));
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto icon = MakeSiteIcon(registry, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(world_objects::CanBeDestroyedBySpell(icon));
	if (madeLogger)
	{
		spdlog::drop("game");
	}
}
