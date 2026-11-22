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
#include <optional>
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

#define LOCATOR_IMPLEMENTATIONS 1
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

TEST_F(ForestMapPlaces, TheListsOfTheTreesGiveTheTreesOfAWalkOfAllTheTrees)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto forest = At({.x = k_Far + 3, .z = k_Z}, true);
	registry.Assign<LandForest>(forest, LandForest {.id = k_ForestId});
	const auto other = At({.x = k_Far + 40, .z = k_Z}, true);
	registry.Assign<LandForest>(other, LandForest {.id = k_ForestId + 1});
	GrownTree({.x = k_Far, .z = k_Z}, true, 0);
	GrownTree({.x = k_Far + 5, .z = k_Z}, false, 1);
	GrownTree({.x = k_Far + 6, .z = k_Z}, true, 2);
	const auto lone = GrownTree({.x = k_Far + 41, .z = k_Z}, true, 0);
	registry.Get<ForestMember>(lone).forest = k_ForestId + 1;

	const ecs::systems::ForestSystem forests;
	// The lists hold only the first forest. The other forest gets its trees from a walk of all the trees.
	const std::array listed {forest};
	const auto lists = forests.ListTreesOfForests(listed);
	EXPECT_EQ(lists.size(), 1u);
	for (const auto thing : {forest, other})
	{
		for (const bool growing : {false, true})
		{
			EXPECT_EQ(forests.ListedTreesOf(thing, growing, lists), forests.TreesOf(thing, growing));
		}
		EXPECT_EQ(forests.ListedCentreTreeOf(thing, lists), forests.CentreTreeOf(thing));
	}
	EXPECT_EQ(forests.ListedTreesOf(other, false, lists), (std::vector {lone}));
}

TEST_F(ForestMapPlaces, TheCountOfTheTreesOfAForestIsTheSizeOfBothListsOfItsTrees)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto forest = At({.x = k_Far + 3, .z = k_Z}, true);
	registry.Assign<LandForest>(forest, LandForest {.id = k_ForestId});
	const auto other = At({.x = k_Far + 40, .z = k_Z}, true);
	registry.Assign<LandForest>(other, LandForest {.id = k_ForestId + 1});
	GrownTree({.x = k_Far, .z = k_Z}, true, 0);
	const auto young = GrownTree({.x = k_Far + 5, .z = k_Z}, false, 1);
	registry.Get<ForestMember>(young).growing = true;
	GrownTree({.x = k_Far + 6, .z = k_Z}, true, 2);
	const auto lone = GrownTree({.x = k_Far + 41, .z = k_Z}, true, 0);
	registry.Get<ForestMember>(lone).forest = k_ForestId + 1;

	const ecs::systems::ForestSystem forests;
	for (const auto thing : {forest, other, lone})
	{
		EXPECT_EQ(forests.TreeCountOf(thing), forests.TreesOf(thing, false).size() + forests.TreesOf(thing, true).size());
	}
	EXPECT_EQ(forests.TreeCountOf(forest), 3u);
	EXPECT_EQ(forests.TreeCountOf(other), 1u);
	EXPECT_EQ(forests.TreeCountOf(lone), 0u);
}

TEST_F(ForestMapPlaces, TheForestOfANumberIsTheFirstOfTheListOfForestsWithThatNumber)
{
	// Two forests can share a number. The answer is the one that the list of the forests (the newest first) meets first.
	auto& registry = Locator::entitiesRegistry::value();
	const auto older = At({.x = k_Far, .z = k_Z}, true);
	registry.Assign<LandForest>(older, LandForest {.id = k_ForestId, .made = 1});
	const auto newer = At({.x = k_Far + 9, .z = k_Z}, true);
	registry.Assign<LandForest>(newer, LandForest {.id = k_ForestId, .made = 4});
	const auto another = At({.x = k_Far + 20, .z = k_Z}, true);
	registry.Assign<LandForest>(another, LandForest {.id = k_ForestId + 1, .made = 2});

	const ecs::systems::ForestSystem forests;
	for (const uint32_t id : {k_ForestId, k_ForestId + 1, k_ForestId + 2})
	{
		std::optional<entt::entity> first;
		for (const auto forest : forests.LandForests())
		{
			if (registry.Get<const LandForest>(forest).id == id)
			{
				first = forest;
				break;
			}
		}
		EXPECT_EQ(forests.LandForestOf(id), first);
	}
	EXPECT_EQ(forests.LandForestOf(k_ForestId), newer);
}
