/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footpaths round a new obstacle:
// RerouteFootpathsAroundObstacle re-plans an original stretch through the planner and leaves a hidden node at the
// obstacle (bits 8 and 2), or bends a stretch of visible nodes round it in 18-degree steps;
// StopReroutingAroundObstacle takes the hidden obstacle node away again. No world: the planner's holder is empty.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace map_coords = openblack::map_coords;

namespace
{
class FootpathObstaclesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A footpath along x at z = 100, its nodes at the given x, each with flags `flags` (1: an original AddPos node)
	static entt::entity Make(std::initializer_list<float> xs, uint8_t flags)
	{
		const auto e = footpaths::Create();
		auto& f = Reg().Get<Footpath>(e);
		for (const auto x : xs)
		{
			const auto coords = map_coords::FromMetres(glm::vec2(x, 100.0f));
			f.nodes.push_back({glm::vec3(x, 0.0f, 100.0f), coords, flags, f.nextNodeId++});
		}
		return e;
	}
	static map_coords::MapCoords At(float x, float z) { return map_coords::FromMetres(glm::vec2(x, z)); }
	static float Range(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
	{
		const float dx = map_coords::ToMetres(a.x) - map_coords::ToMetres(b.x);
		const float dz = map_coords::ToMetres(a.z) - map_coords::ToMetres(b.z);
		return std::sqrt(dx * dx + dz * dz);
	}
};
} // namespace

TEST_F(FootpathObstaclesTest, AnOriginalStretchIsReplannedWithAHiddenNodeAtTheObstacle)
{
	const auto fp = Make({100.0f, 200.0f}, 1);
	const auto pos = At(150.0f, 102.0f);
	// the pair (100, 200) passes 2 m from the obstacle (r 10): pending (the head has no bit 1) -> AttemptRerender(head,
	// tail, &pos), found on an empty map -> the hidden node c at pos (bits 8 and 2)
	footpaths::RerouteFootpathsAroundObstacle(10.0f, pos);
	const auto& f = Reg().Get<Footpath>(fp);
	int obstacleNodes = 0;
	for (const auto& n : f.nodes)
	{
		if ((n.flags & 8u) != 0)
		{
			++obstacleNodes;
			EXPECT_NE(n.flags & 2u, 0u);
			EXPECT_LT(Range(n.coords, pos), 0.01f);
		}
	}
	EXPECT_EQ(obstacleNodes, 1);
	EXPECT_EQ(f.nodes.front().coords, At(100.0f, 100.0f));
	EXPECT_EQ(f.nodes.back().coords, At(200.0f, 100.0f));
}

TEST_F(FootpathObstaclesTest, TheHiddenObstacleNodeGoesWithTheObstacle)
{
	const auto fp = Make({100.0f, 200.0f}, 1);
	const auto pos = At(150.0f, 102.0f);
	footpaths::RerouteFootpathsAroundObstacle(10.0f, pos);
	// the second pass: the node with bits 8 and 2 near pos deleted, the stretch re-planned without it
	footpaths::StopReroutingAroundObstacle(10.0f, pos);
	for (const auto& n : Reg().Get<Footpath>(fp).nodes)
	{
		EXPECT_EQ(n.flags & 8u, 0u);
	}
}

TEST_F(FootpathObstaclesTest, AStretchOfVisibleNodesIsBentRoundTheObstacle)
{
	// nodes with bit 1 (made by the planner): no pending rerender; both ends outside the circle -> the arc
	const auto fp = Make({100.0f, 200.0f}, 3);
	const auto pos = At(150.0f, 102.0f);
	footpaths::RerouteFootpathsAroundObstacle(10.0f, pos);
	const auto& f = Reg().Get<Footpath>(fp);
	ASSERT_GT(f.nodes.size(), 3u);
	EXPECT_EQ(f.nodes.front().coords, At(100.0f, 100.0f));
	EXPECT_EQ(f.nodes.back().coords, At(200.0f, 100.0f));
	// every new node runs round the obstacle at about r (the entry, the 18-degree steps, the exit)
	for (size_t i = 1; i + 1 < f.nodes.size(); ++i)
	{
		EXPECT_NEAR(Range(f.nodes.at(i).coords, pos), 10.0f, 0.05f);
		EXPECT_EQ(f.nodes.at(i).flags, 3u);
	}
}

TEST_F(FootpathObstaclesTest, AStretchAwayFromTheObstacleIsLeftAlone)
{
	const auto fp = Make({100.0f, 200.0f}, 1);
	footpaths::RerouteFootpathsAroundObstacle(10.0f, At(150.0f, 300.0f));
	EXPECT_EQ(Reg().Get<Footpath>(fp).nodes.size(), 2u);
}
