/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The crossing file's line for each kind of thing a vortex takes, read from its components

#include <fmt/format.h>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAge.h"
#include "ECS/VortexCrossingLines.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr float k_Ground = 20.0f;
/// The vortex stands at (1000, 2000) on the land
const map_coords::MapCoords k_Vortex = map_coords::FromMetres({1000.0f, 2000.0f});

vortex_crossing::World FakeWorld()
{
	return {
	    .turn = 30000,
	    .ground = [](glm::vec2) { return k_Ground; },
	    .villagerRow = [](Tribe, VillagerNumber) { return 42; },
	    .potResource = [](PotInfo) { return 1; },
	    .life = [](entt::entity) { return 0.75f; },
	    .player = [](entt::entity) { return std::optional<PlayerNames> {}; },
	};
}

std::string LineText(const Registry& registry, entt::entity thing)
{
	const auto line = vortex_crossing::LineFor(registry, thing, k_Vortex, FakeWorld());
	return line.has_value() ? vortex::crossing::Text(*line) : std::string {};
}

entt::entity Thing(Registry& registry, glm::vec3 position, const glm::mat3& rotation, float scale)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	return entity;
}
} // namespace

TEST(VortexCrossingLines, AVillagerIsWrittenByItsTribeAndJob)
{
	Registry registry;
	const auto town = registry.Create();
	registry.Assign<Town>(town, Town {.id = 3});
	const auto villager = Thing(registry, {1010.0f, k_Ground, 2020.0f}, glm::mat3(1.0f), 1.0f);
	auto& data = registry.Assign<Villager>(villager);
	data.tribe = Tribe::CELTIC;
	data.number = VillagerNumber::Farmer;
	data.town = town;
	data.birthTurn = 30000 - (25 * villager_age::k_TurnsPerYear);
	EXPECT_EQ(LineText(registry, villager), "CREATE_TOWN_VILLAGER(3, \"10.00,20.00\", CELTIC_FARMER, 25)\n");
	// One of no tribe by its row of the villagers' table, here without a town
	data.tribe = Tribe::NONE;
	data.town = entt::null;
	EXPECT_EQ(LineText(registry, villager), "CREATE_SPECIAL_TOWN_VILLAGER(-1, \"10.00,20.00\", 42, 25)\n");
}

TEST(VortexCrossingLines, ATreeKeepsItsTurnAndSizes)
{
	Registry registry;
	const auto tree = Thing(registry, {990.0f, k_Ground, 2000.0f}, glm::eulerAngleY(-0.5f), 0.8f);
	registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Oak, .maxSize = 1.0f, .nonScenic = true});
	EXPECT_EQ(LineText(registry, tree),
	          fmt::format("CREATE_NEW_TREE(-1, \"-10.00,0.00\", {}, 1, 0.500000, 0.800000, 1.000000)\n",
	                      static_cast<int>(TreeInfo::Oak)));
}

TEST(VortexCrossingLines, ARockKeepsItsHeightAboveTheLandAndItsTurns)
{
	Registry registry;
	const auto rock = Thing(registry, {1000.0f, k_Ground + 2.0f, 2000.0f}, glm::eulerAngleXYZ(-0.1f, -0.2f, -0.3f), 1.5f);
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	EXPECT_EQ(LineText(registry, rock), fmt::format("CREATE_MOBILE_STATIC(\"0.00,0.00\", {}, 2.000000, 0.100000, 0.200000, "
	                                                "0.300000, 1.500000)\n",
	                                                static_cast<int>(MobileStaticInfo::Rock)));
}

TEST(VortexCrossingLines, APotIsWrittenUnlessItIsAStoragePitsPile)
{
	Registry registry;
	const auto pot = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<Pot>(pot, Pot {.amount = 1000, .maxAmount = 1000, .type = PotInfo::HandFood});
	EXPECT_EQ(LineText(registry, pot),
	          fmt::format("CREATE_POT(\"0.00,0.00\", {}, 1, 1000)\n", static_cast<int>(PotInfo::HandFood)));
	const auto pit = registry.Create();
	registry.Assign<StoragePit>(
	    pit, StoragePit {.woodPiles = {entt::null, entt::null, entt::null, entt::null, entt::null}, .foodPile = pot});
	EXPECT_EQ(LineText(registry, pot), "");
}

TEST(VortexCrossingLines, ADeadTreeOfNobodyIsTheNeutralPlayers)
{
	Registry registry;
	const auto dead = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<DeadTree>(dead, DeadTree {.type = TreeInfo::Oak});
	EXPECT_EQ(LineText(registry, dead),
	          fmt::format("CREATE_DEAD_TREE(\"0.00,0.00\", \"NEUTRAL\", {}, 0.750000, 0.000000, 0.000000, 0.000000)\n",
	                      static_cast<int>(TreeInfo::Oak)));
}

TEST(VortexCrossingLines, ABuildingHasNoLine)
{
	Registry registry;
	const auto thing = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	EXPECT_EQ(LineText(registry, thing), "");
}
