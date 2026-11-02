/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <set>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/FixedKinds.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace fixed_kinds = openblack::ecs::fixed_kinds;

namespace
{

template <typename Component>
entt::entity Make(Registry& registry)
{
	const auto thing = registry.Create();
	registry.Assign<Component>(thing);
	return thing;
}

/// A fake test of the mark: only the things in the set have it
struct MarkedSet
{
	std::set<entt::entity> marked;
	[[nodiscard]] bool operator()(entt::entity thing) const { return marked.contains(thing); }
};

} // namespace

TEST(FixedKinds, AWorshipSiteAndItsPartsHaveTheMark)
{
	Registry registry;
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<WorshipSite>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<WorshipAltar>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<WorshipIcon>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<Temple>(registry)));
}

TEST(FixedKinds, BuildingsAndThingsOfTheLandHaveTheMark)
{
	Registry registry;
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<Abode>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<Field>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<Feature>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<BigForest>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<FishFarm>(registry)));
	// A rock and a dead tree have the mark, though a scaffold or a creature can throw them
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<MobileStatic>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<DeadTree>(registry)));
	EXPECT_TRUE(fixed_kinds::IsFixedKind(registry, Make<Vortex>(registry)));
}

TEST(FixedKinds, TreesPilesLanternsAndTheLivingHaveNoMark)
{
	Registry registry;
	EXPECT_FALSE(fixed_kinds::IsFixedKind(registry, Make<Tree>(registry)));
	EXPECT_FALSE(fixed_kinds::IsFixedKind(registry, Make<Pot>(registry)));
	EXPECT_FALSE(fixed_kinds::IsFixedKind(registry, Make<StreetLantern>(registry)));
	EXPECT_FALSE(fixed_kinds::IsFixedKind(registry, Make<Villager>(registry)));
	EXPECT_FALSE(fixed_kinds::IsFixedKind(registry, registry.Create()));
}

TEST(FixedKinds, OnlyTheFirstThingOfTheCellCounts)
{
	const auto tree = static_cast<entt::entity>(1);
	const auto building = static_cast<entt::entity>(2);
	const MarkedSet marks {.marked = {building}};
	// A tree in front of a building leaves the cell with no mark
	const std::array treeFirst {tree, building};
	EXPECT_FALSE(fixed_kinds::CellIsFixed(treeFirst, marks));
	// A building in front of a tree gives the cell the mark
	const std::array buildingFirst {building, tree};
	EXPECT_TRUE(fixed_kinds::CellIsFixed(buildingFirst, marks));
}

TEST(FixedKinds, AnEmptyCellHasNoMark)
{
	const MarkedSet marks {.marked = {static_cast<entt::entity>(1)}};
	EXPECT_FALSE(fixed_kinds::CellIsFixed(std::vector<entt::entity> {}, marks));
}
