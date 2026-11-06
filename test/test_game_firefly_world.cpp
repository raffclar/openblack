/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/MapPlace.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ThingMapPlace.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/GameFireflyWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// A map position far from the corner of the map, which metres cannot keep
constexpr int32_t k_Far = 0x01000001;

class GameFireflySpot: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }
};
} // namespace

TEST_F(GameFireflySpot, AFireflyHidesAtTheMapPositionOfATreeNotWhereItIsDrawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto tree = registry.Create();
	const map_coords::MapCoords place {.x = k_Far, .z = k_Far};
	const auto xz = map_coords::ToMetres(place);
	// The tree stands above the ground. Its metres do not give its map position again
	registry.Assign<Transform>(tree, glm::vec3(xz.x, 30.0f, xz.y), glm::mat3(1.0f), glm::vec3(1.0f));
	ecs::thing_map_place::Keep(registry, tree, place);
	ASSERT_NE(map_coords::ToFixed(xz.x), k_Far);

	const ecs::systems::GameFireflyWorld world;
	const auto spot = world.SpotOf(tree);
	ASSERT_TRUE(spot.has_value());
	EXPECT_EQ(*spot, place);
}

TEST_F(GameFireflySpot, AThingThatIsGoneOrHasNoPlaceGivesNoSpot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto bare = registry.Create();
	const auto gone = registry.Create();
	registry.Destroy(gone);
	const ecs::systems::GameFireflyWorld world;
	EXPECT_FALSE(world.SpotOf(bare).has_value());
	EXPECT_FALSE(world.SpotOf(gone).has_value());
}
