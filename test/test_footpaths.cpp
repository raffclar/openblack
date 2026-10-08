/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footpaths' queries: GetNextNode, GetEndNonHiddenNode, GetNearestPos, NextNodeNear / StepNextNode,
// GetNearestPathTo / GetNearestPathToQuick and the loaded link's resolve (AttachLoadedLink). No game data: hand-made
// footpaths, made in list order (node id = its place) unless a test inserts at the head as adding a node does.

#define LOCATOR_IMPLEMENTATIONS

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Town.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace map_coords = openblack::map_coords;

namespace
{
class FootpathsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A footpath along x, one node every 10 m from x0, the flags of each
	static entt::entity Make(int x0, std::initializer_list<uint8_t> flags)
	{
		const auto e = Reg().Create();
		auto& f = Reg().Assign<Footpath>(e);
		int x = x0;
		for (const auto flag : flags)
		{
			const auto coords = map_coords::FromMetres(glm::vec2(static_cast<float>(x), 0.0f));
			f.nodes.push_back({glm::vec3(static_cast<float>(x), 0.0f, 0.0f), coords, flag, f.nextNodeId++});
			x += 10;
		}
		return e;
	}

	static map_coords::MapCoords At(float x) { return map_coords::FromMetres(glm::vec2(x, 0.0f)); }
};

TEST_F(FootpathsTest, NextNodeStepsOverHiddenNodes)
{
	// nodes 0..4; 2 is hidden (bit 2)
	const auto f = Make(0, {1, 1, 4, 1, 1});
	EXPECT_EQ(footpaths::GetNextNode(f, 1, 0), 3);                   // dir 0: the one after, over the hidden one
	EXPECT_EQ(footpaths::GetNextNode(f, 3, 1), 1);                   // dir != 0: the one before
	EXPECT_EQ(footpaths::GetNextNode(f, 0, 1), footpaths::k_NoNode); // nothing before the head
	EXPECT_EQ(footpaths::GetNextNode(f, 4, 0), footpaths::k_NoNode);
}

TEST_F(FootpathsTest, EndNonHiddenNode)
{
	const auto f = Make(0, {8, 1, 1, 8});
	const auto first = footpaths::GetEndNonHiddenNode(f, 1);
	const auto last = footpaths::GetEndNonHiddenNode(f, 0);
	ASSERT_TRUE(first.has_value() && last.has_value());
	EXPECT_EQ(*first, footpaths::NodeCoords(f, 1));
	EXPECT_EQ(*last, footpaths::NodeCoords(f, 2));
	EXPECT_FALSE(footpaths::GetEndNonHiddenNode(Make(0, {4, 8}), 0).has_value());
}

TEST_F(FootpathsTest, NearestPosTakesTheNextWhenThePointIsPastTheNearest)
{
	const auto f = Make(0, {1, 1, 1});
	// 14 m: nearest node 1 (10 m); toward node 2 (dir 0) the point lies between them -> node 2
	EXPECT_EQ(footpaths::GetNearestPos(f, At(14.0f), 0), 2);
	// toward node 0 (dir != 0): node 0 is 10 m from node 1 and 14 m from the point -> the nearest stays
	EXPECT_EQ(footpaths::GetNearestPos(f, At(14.0f), 1), 1);
}

TEST_F(FootpathsTest, LinkPicksTheNearestFootpathAndItsDirection)
{
	const auto near = Make(0, {1, 1, 1});
	const auto far = Make(100, {1, 1, 1});
	const auto link = Reg().Create();
	Reg().Assign<FootpathLink>(link, glm::vec3(0.0f),
	                           std::vector<Footpath::Id> {static_cast<Footpath::Id>(far), static_cast<Footpath::Id>(near)});
	float best = 1000.0f;
	const auto choice = footpaths::GetNearestPathTo(link, At(1.0f), At(50.0f), best);
	ASSERT_TRUE(choice.has_value());
	EXPECT_TRUE(choice->footpath == near);
	EXPECT_EQ(choice->direction, 0); // the tail (20 m) is nearer `to` than the head: toward the tail
	// the quick one enters at the far end
	float quickBest = 1000.0f;
	const auto quick = footpaths::GetNearestPathToQuick(link, At(1.0f), At(50.0f), quickBest);
	ASSERT_TRUE(quick.has_value());
	EXPECT_TRUE(quick->footpath == near);
	EXPECT_EQ(quick->node, 0);
}

// Adding a node inserts it at the head: the ids name the nodes, not their places
TEST_F(FootpathsTest, NodeIdsSurviveHeadInsertion)
{
	const auto e = Reg().Create();
	auto& f = Reg().Assign<Footpath>(e);
	for (int x = 0; x < 30; x += 10)
	{
		const auto coords = At(static_cast<float>(x));
		f.nodes.insert(f.nodes.begin(),
		               Footpath::Node {glm::vec3(static_cast<float>(x), 0.0f, 0.0f), coords, 1, f.nextNodeId++});
	}
	// list: id 2 (20 m, the head), id 1 (10 m), id 0 (0 m)
	EXPECT_EQ(footpaths::HeadNode(e), 2);
	EXPECT_EQ(footpaths::TailNode(e), 0);
	EXPECT_EQ(footpaths::GetNextNode(e, 2, 0), 1);
	EXPECT_EQ(footpaths::GetNextNode(e, 1, 0), 0);
	EXPECT_EQ(footpaths::GetNextNode(e, 1, 1), 2);
	EXPECT_EQ(footpaths::NodeCoords(e, 0), At(0.0f));
}

// NextNodeNear: the nearest node within maxDist, then the next one; 0x2D at the end
TEST_F(FootpathsTest, NextNodeNearAndStepNextNode)
{
	// nodes 0, 1, 2 at 0, 10 and 20 m
	const auto f = Make(0, {1, 1, 1});
	footpaths::NodeId node = footpaths::k_NoNode;
	map_coords::MapCoords out {};
	// the nearest (GetNearestPos, dir 0) 30 m away: over maxDist -> 0
	EXPECT_EQ(footpaths::NextNodeNear(f, At(50.0f), node, out, 0, 5.0f), 0);
	// at 9 m: GetNearestPos gives node 1 (node 2 is not nearer the point than node 1 is), 1 m away; its next is 2
	EXPECT_EQ(footpaths::NextNodeNear(f, At(9.0f), node, out, 0, 5.0f), 1);
	EXPECT_EQ(node, 2);
	EXPECT_EQ(out, At(20.0f));
	// no node after 2: 0x2D, node unchanged
	EXPECT_EQ(footpaths::StepNextNode(f, node, out, 0), footpaths::k_EndOfFootpath);
	EXPECT_EQ(node, 2);
	// toward the head (dir != 0): 2 -> 1
	EXPECT_EQ(footpaths::StepNextNode(f, node, out, 1), 1);
	EXPECT_EQ(node, 1);
	EXPECT_EQ(out, At(10.0f));
	// at 21 m the nearest is the last node, with no next: 0x2D
	EXPECT_EQ(footpaths::NextNodeNear(f, At(21.0f), node, out, 0, 5.0f), footpaths::k_EndOfFootpath);
}

// A node's occupants: the newest at the head
TEST_F(FootpathsTest, OccupantsHeadFirst)
{
	const auto f = Make(0, {1, 1});
	const auto a = Reg().Create();
	const auto b = Reg().Create();
	footpaths::AddOccupant(f, 1, a);
	footpaths::AddOccupant(f, 1, b);
	const auto& occupants = Reg().Get<Footpath>(f).nodes.at(1).occupants;
	ASSERT_EQ(occupants.size(), 2u);
	EXPECT_TRUE(occupants.front() == b);
	footpaths::RemoveOccupant(f, 1, b);
	ASSERT_EQ(occupants.size(), 1u);
	EXPECT_TRUE(occupants.front() == a);
}

// AttachLoadedLink: no fixed object at the point -> a planned abode within 0.05 m; none -> the link deleted
TEST_F(FootpathsTest, LoadedLinkGoesToAPlanOrIsDeleted)
{
	const auto town = Reg().Create();
	auto& t = Reg().Assign<Town>(town);
	t.id = 1;
	Reg().Context().towns[1] = town;
	PlannedAbode plan {};
	plan.position = glm::vec3(100.0f, 0.0f, 50.0f);
	t.plannedAbodes.push_back(plan);
	const auto link = Reg().Create();
	Reg().Assign<FootpathLink>(link, glm::vec3(0.0f), std::vector<Footpath::Id> {});
	footpaths::AttachLoadedLink(link, map_coords::FromWorld(glm::vec3(100.0f, 0.0f, 50.0f)));
	EXPECT_TRUE(Reg().Get<Town>(town).plannedAbodes.at(0).footpathLink == link);
	const auto stray = Reg().Create();
	Reg().Assign<FootpathLink>(stray, glm::vec3(0.0f), std::vector<Footpath::Id> {});
	footpaths::AttachLoadedLink(stray, map_coords::FromWorld(glm::vec3(300.0f, 0.0f, 50.0f)));
	EXPECT_FALSE(Reg().Valid(stray));
	EXPECT_TRUE(footpaths::GetFootpathLink(town) == entt::null);
}
// Create pushes at the head; PositionFromHead counts from it: position 0 is the newest
TEST_F(FootpathsTest, PositionsCountFromTheNewest)
{
	const auto a = footpaths::Create();
	const auto b = footpaths::Create();
	EXPECT_EQ(footpaths::PositionFromHead(b), 0);
	EXPECT_EQ(footpaths::PositionFromHead(a), 1);
	EXPECT_TRUE(footpaths::AtPositionFromHead(0) == b);
	EXPECT_TRUE(footpaths::AtPositionFromHead(1) == a);
	EXPECT_TRUE(footpaths::AtPositionFromHead(2) == entt::null);
	EXPECT_TRUE(footpaths::AtPositionFromHead(-1) == entt::null);
	// a footpath made in between (the planner's, a .fot's) shifts the older ones
	const auto c = footpaths::Create();
	EXPECT_EQ(footpaths::PositionFromHead(a), 2);
	EXPECT_TRUE(footpaths::AtPositionFromHead(0) == c);
}
} // namespace
