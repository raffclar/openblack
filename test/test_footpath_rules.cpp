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

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/FootpathRules.h"

using namespace openblack::ecs;
using footpath_rules::Join;
using footpath_rules::Leg;
using footpath_rules::Path;

namespace
{
/// Ten metres, a map cell, in whole map units
constexpr int32_t k_Cell = 0x10000;

constexpr glm::ivec2 At(int32_t cellsX, int32_t cellsZ)
{
	return {cellsX * k_Cell, cellsZ * k_Cell};
}

/// A straight path along x at z = 0, one node every ten metres from x = 0
std::vector<glm::ivec2> StraightPath(int32_t nodes)
{
	std::vector<glm::ivec2> places;
	for (int32_t i = 0; i < nodes; ++i)
	{
		places.push_back(At(i, 0));
	}
	return places;
}
} // namespace

TEST(FootpathRules, TheNextNodeGoesEitherWayAndStopsAtTheEnds)
{
	const auto path = StraightPath(3);
	EXPECT_EQ(footpath_rules::NextNode(path, 0, false), std::optional<uint32_t>(1));
	EXPECT_EQ(footpath_rules::NextNode(path, 2, false), std::nullopt);
	EXPECT_EQ(footpath_rules::NextNode(path, 2, true), std::optional<uint32_t>(1));
	EXPECT_EQ(footpath_rules::NextNode(path, 0, true), std::nullopt);
}

TEST(FootpathRules, AVillagerJoinsAtTheNearestNode)
{
	const auto path = StraightPath(4);
	// Beside node 1, short of it going forwards: it walks to node 1
	EXPECT_EQ(footpath_rules::NearestNode(path, At(1, 1) - glm::ivec2(k_Cell / 5, 0), false), std::optional<uint32_t>(1));
}

TEST(FootpathRules, AVillagerPastTheNearestNodeGoesOnToTheNextOne)
{
	const auto path = StraightPath(4);
	// Just past node 1 towards node 2, nearer to node 2 than node 1 is: straight on to node 2
	const glm::ivec2 pastNode1 {k_Cell + k_Cell / 5, 0};
	EXPECT_EQ(footpath_rules::NearestNode(path, pastNode1, false), std::optional<uint32_t>(2));
	// Going the other way it is short of node 1, so node 1 it is
	EXPECT_EQ(footpath_rules::NearestNode(path, pastNode1, true), std::optional<uint32_t>(1));
}

TEST(FootpathRules, APathIsWalkedTowardsTheEndNearerTheGoal)
{
	const auto nodes = StraightPath(5);
	const std::array paths {Path {.nodes = nodes}};
	const auto towardsEnd = footpath_rules::NearestPathTo(paths, At(0, 1), At(5, 0), 40.0f);
	ASSERT_TRUE(towardsEnd.has_value());
	EXPECT_FALSE(towardsEnd->towardsStart);
	const auto towardsStart = footpath_rules::NearestPathTo(paths, At(4, 1), At(-1, 0), 40.0f);
	ASSERT_TRUE(towardsStart.has_value());
	EXPECT_TRUE(towardsStart->towardsStart);
	EXPECT_EQ(towardsStart->node, 4u);
}

TEST(FootpathRules, APathIsJoinedOnlyWithinItsReach)
{
	const auto nodes = StraightPath(2);
	const std::array paths {Path {.nodes = nodes}};
	// Its nearest node is 50 m away: out of reach
	EXPECT_FALSE(footpath_rules::NearestPathTo(paths, At(0, -5), At(1, 0), 40.0f).has_value());
	EXPECT_TRUE(footpath_rules::NearestPathTo(paths, At(0, -3), At(1, 0), 40.0f).has_value());
}

TEST(FootpathRules, OfSeveralPathsTheOneNearestTheVillagerIsTakenAndClosedOnesNever)
{
	const auto far = StraightPath(3);
	const std::vector<glm::ivec2> nearer {At(0, 1), At(1, 1), At(2, 1)};
	const std::array paths {Path {.nodes = far}, Path {.nodes = nearer}};
	const auto join = footpath_rules::NearestPathTo(paths, At(0, 2), At(2, 1), 40.0f);
	ASSERT_TRUE(join.has_value());
	EXPECT_EQ(join->path, 1u);
	const std::array closed {Path {.nodes = far}, Path {.nodes = nearer, .walkable = false}};
	const auto other = footpath_rules::NearestPathTo(closed, At(0, 2), At(2, 1), 40.0f);
	ASSERT_TRUE(other.has_value());
	EXPECT_EQ(other->path, 0u);
}

TEST(FootpathRules, TheQuickChoiceStartsFromTheEndFartherFromTheGoal)
{
	const auto nodes = StraightPath(4);
	const std::array paths {Path {.nodes = nodes}};
	const auto join = footpath_rules::NearestPathToQuick(paths, At(0, 1), At(3, 0), 40.0f);
	ASSERT_TRUE(join.has_value());
	EXPECT_EQ(join, (Join {.path = 0, .towardsStart = false, .node = 0}));
	const auto back = footpath_rules::NearestPathToQuick(paths, At(3, 1), At(0, 0), 40.0f);
	EXPECT_EQ(back, (Join {.path = 0, .towardsStart = true, .node = 3}));
	// That end must be within the reach of the villager
	EXPECT_FALSE(footpath_rules::NearestPathToQuick(paths, At(3, 5), At(3, 0), 40.0f).has_value());
}

TEST(FootpathRules, FarFromTheBuildingsPathsTheWalkGoesThroughTheStoragePit)
{
	// The pit is at (0, 0); the farm's path runs from the pit out to the farm at (5, 0); the pit's other path runs from
	// the pit to a house at (0, 6), where the villager stands
	const auto farmPath = StraightPath(6);
	const std::vector<glm::ivec2> housePath {At(0, 0), At(0, 2), At(0, 4), At(0, 6)};
	const std::array farmPaths {Path {.nodes = farmPath}};
	const std::array storePaths {Path {.nodes = farmPath}, Path {.nodes = housePath}};
	const auto villager = At(0, 6) + glm::ivec2(k_Cell / 10, 0);
	const auto farm = At(5, 0);
	const auto pit = At(0, 0);
	EXPECT_FALSE(footpath_rules::NearestPathTo(farmPaths, villager, farm, footpath_rules::k_JoinReach).has_value());
	const auto route = footpath_rules::RouteThroughStore(farmPaths, storePaths, villager, farm, pit);
	ASSERT_TRUE(route.has_value());
	EXPECT_EQ(route->toStore, (Join {.path = 1, .towardsStart = true, .node = 3}));
	EXPECT_EQ(route->onward, 0u);
	// A pit with no paths of its own gives no route
	EXPECT_FALSE(footpath_rules::RouteThroughStore(farmPaths, {}, villager, farm, pit).has_value());
}

TEST(FootpathRules, NodesOnTheWayAreJitteredButTheLastIsNot)
{
	const auto path = StraightPath(3);
	EXPECT_EQ(footpath_rules::NextLeg(path, 0, false), (Leg {.node = 1, .jitter = true}));
	EXPECT_EQ(footpath_rules::NextLeg(path, 1, false), (Leg {.node = 2, .jitter = false}));
	EXPECT_EQ(footpath_rules::NextLeg(path, 2, false), std::nullopt);
}

TEST(FootpathRules, TheJitterIsUpToASixteenthOfACellEitherWay)
{
	const auto place = At(10, 10);
	EXPECT_EQ(footpath_rules::Jittered(place, 0, 0x1FFF), place + glm::ivec2(-0x1000, 0xFFF));
	EXPECT_EQ(footpath_rules::Jittered(place, 0x1000, 0x1000), place);
}

TEST(FootpathRules, AWalkerThatLostItsNodeCarriesOnOnlyFromOnANode)
{
	const auto path = StraightPath(3);
	EXPECT_EQ(footpath_rules::Rejoin(path, At(1, 0), false), std::optional<uint32_t>(2));
	EXPECT_EQ(footpath_rules::Rejoin(path, At(1, 0) + glm::ivec2(0, k_Cell / 100), false), std::nullopt);
}

TEST(FootpathRules, TheOnwardPathIsWalkedFromItsEndNearerTheWalker)
{
	const auto path = StraightPath(3);
	EXPECT_FALSE(footpath_rules::OnwardTowardsStart(path, At(0, 0)));
	EXPECT_EQ(footpath_rules::OnwardFirstNode(path, false), std::optional<uint32_t>(1));
	EXPECT_TRUE(footpath_rules::OnwardTowardsStart(path, At(2, 0)));
	EXPECT_EQ(footpath_rules::OnwardFirstNode(path, true), std::optional<uint32_t>(1));
	const auto single = StraightPath(1);
	EXPECT_EQ(footpath_rules::OnwardFirstNode(single, false), std::optional<uint32_t>(0));
}
