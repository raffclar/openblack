/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/ThingMapPlace.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ForestSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// A map position far from the corner of the map, which metres cannot keep: its metres give the next map position
constexpr int32_t k_Far = 0x01000001;
/// A z that metres keep
constexpr int32_t k_Z = 0x00100000;
constexpr uint32_t k_ForestId = 7;

class ForestMapPlaces: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	/// A thing at the metres of a map position, which keeps the map position when `kept` is set
	static entt::entity At(const map_coords::MapCoords& place, bool kept)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		const auto xz = map_coords::ToMetres(place);
		registry.Assign<Transform>(thing, glm::vec3(xz.x, 0.0f, xz.y), glm::mat3(1.0f), glm::vec3(1.0f));
		if (kept)
		{
			ecs::thing_map_place::Keep(registry, thing, place);
		}
		return thing;
	}

	static entt::entity GrownTree(const map_coords::MapCoords& place, bool kept, uint32_t listed)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto tree = At(place, kept);
		registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f});
		registry.Assign<ForestMember>(tree, ForestMember {.forest = k_ForestId, .growing = false, .listed = listed});
		return tree;
	}
};
} // namespace

TEST_F(ForestMapPlaces, AForestListsItsTreesByTheDistanceBetweenTheirMapPositions)
{
	auto& registry = Locator::entitiesRegistry::value();
	const map_coords::MapCoords forestPlace {.x = k_Far + 3, .z = k_Z};
	const auto forest = At(forestPlace, true);
	registry.Assign<LandForest>(forest, LandForest {.id = k_ForestId});

	// The first tree is 3 map units from the forest, but its metres give a place 2 units from it. The second tree is 2
	// units from the forest, and its metres give its place again.
	const map_coords::MapCoords firstPlace {.x = k_Far, .z = k_Z};
	const map_coords::MapCoords secondPlace {.x = k_Far + 5, .z = k_Z};
	ASSERT_EQ(map_coords::FromMetres(map_coords::ToMetres(firstPlace)).x, k_Far + 1);
	ASSERT_EQ(map_coords::FromMetres(map_coords::ToMetres(secondPlace)).x, k_Far + 5);
	ASSERT_EQ(map_coords::FromMetres(map_coords::ToMetres(forestPlace)).x, k_Far + 3);
	const auto first = GrownTree(firstPlace, true, 0);
	const auto second = GrownTree(secondPlace, false, 1);

	const ecs::systems::ForestSystem forests;
	// By the map positions the second tree is the nearer. By the metres the two are as near, and the first, listed
	// first, would come first.
	EXPECT_EQ(forests.TreesOf(forest, false), (std::vector {second, first}));
	EXPECT_EQ(forests.CentreTreeOf(forest), second);
}
