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
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/FixedKinds.h"
#include "ECS/Registry.h"
#include "ECS/TownDesire.h"

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

TEST(FixedKinds, RocksDeadTreesAndForestsAreAlwaysWhole)
{
	Registry registry;
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<MobileStatic>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<DeadTree>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<BuildingPiece>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<BigForest>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<FishFarm>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<Vortex>(registry)));
	EXPECT_TRUE(fixed_kinds::IsAlwaysWhole(registry, Make<TeleportStone>(registry)));
}

TEST(FixedKinds, BuildingsFeaturesAndWorshipSitesAreNotAlwaysWhole)
{
	Registry registry;
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Abode>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Field>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Feature>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<AnimatedStatic>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Flowers>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<WorshipSite>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<WorshipAltar>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<WorshipIcon>(registry)));
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Temple>(registry)));
	// A thing with no mark is not one of them
	EXPECT_FALSE(fixed_kinds::IsAlwaysWhole(registry, Make<Tree>(registry)));
}

TEST(FixedKinds, AFixedThingWantsRepairByItsLostLife)
{
	// Full life: no wish
	EXPECT_EQ(town_desire::FixedDesireToBeRepaired(1.0f, 0.8f), 0.0f);
	// Half life: (0.5 x 0.5 + 0.5) x 0.8
	EXPECT_EQ(town_desire::FixedDesireToBeRepaired(0.5f, 0.8f), 0.6f);
	// No life: (1 x 0.5 + 0.5) x 0.8
	EXPECT_EQ(town_desire::FixedDesireToBeRepaired(0.0f, 0.8f), 0.8f);
	// At most 1
	EXPECT_EQ(town_desire::FixedDesireToBeRepaired(0.0f, 3.0f), 1.0f);
	// A kind that wants no repair
	EXPECT_EQ(town_desire::FixedDesireToBeRepaired(0.2f, 0.0f), 0.0f);
}
