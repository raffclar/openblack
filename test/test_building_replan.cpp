/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Common/PortableRotations.h"
#include "ECS/BuildingReplan.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/TownRemoval.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

constexpr uint32_t k_TownId = 7;
constexpr glm::vec3 k_Place {120.0f, 4.0f, 80.0f};
constexpr float k_Scale = 1.5f;

entt::entity MakeTown(Registry& registry, uint32_t id)
{
	const auto entity = registry.Create();
	registry.Assign<Town>(entity).id = id;
	registry.Context().towns.insert({id, entity});
	return entity;
}

void Place(Registry& registry, entt::entity building, float yAngle)
{
	registry.Assign<Transform>(building, k_Place, glm::mat3(portable_math::EulerAngleY(-yAngle)), glm::vec3(k_Scale));
}

entt::entity MakeDispenser(Registry& registry, entt::entity town)
{
	const auto entity = registry.Create();
	Place(registry, entity, 0.0f);
	registry.Assign<SpellDispenser>(entity, SpellDispenser {.building = AbodeInfo::CelticSpellDispenser, .town = town});
	return entity;
}

} // namespace

TEST(BuildingReplan, ADispenserOfATownIsPlannedAgainAsItStood)
{
	Registry registry;
	const auto town = MakeTown(registry, k_TownId);
	const auto dispenser = MakeDispenser(registry, town);
	const auto replan = building_replan::Of(registry, dispenser);
	ASSERT_TRUE(replan.has_value());
	EXPECT_EQ(replan->townId, k_TownId);
	EXPECT_EQ(replan->info, AbodeInfo::CelticSpellDispenser);
	EXPECT_EQ(replan->position, k_Place);
	EXPECT_EQ(replan->scale, k_Scale);
	EXPECT_TRUE(replan->wasBuilt);
}

TEST(BuildingReplan, ThePlanOfATurnedDispenserKeepsItsTurn)
{
	constexpr float k_Turn = 1.25f;
	Registry registry;
	const auto town = MakeTown(registry, k_TownId);
	const auto dispenser = MakeDispenser(registry, town);
	registry.Remove<Transform>(dispenser);
	Place(registry, dispenser, k_Turn);
	const auto replan = building_replan::Of(registry, dispenser);
	ASSERT_TRUE(replan.has_value());
	EXPECT_NEAR(replan->yAngle, k_Turn, 1e-5f);
}

TEST(BuildingReplan, ADispenserNotYetBuiltLeavesAPlanOfABuildingNeverFinished)
{
	Registry registry;
	const auto town = MakeTown(registry, k_TownId);
	const auto dispenser = MakeDispenser(registry, town);
	registry.Assign<BuildProgress>(dispenser, 0.4f);
	const auto replan = building_replan::Of(registry, dispenser);
	ASSERT_TRUE(replan.has_value());
	EXPECT_FALSE(replan->wasBuilt);
}

TEST(BuildingReplan, ADispenserOfNoTownOrThatAScaffoldMadeIsNotPlannedAgain)
{
	Registry registry;
	const auto town = MakeTown(registry, k_TownId);
	EXPECT_FALSE(building_replan::Of(registry, MakeDispenser(registry, entt::null)).has_value());
	const auto scaffolds = MakeDispenser(registry, town);
	registry.Get<SpellDispenser>(scaffolds).notPlannedAgain = true;
	EXPECT_FALSE(building_replan::Of(registry, scaffolds).has_value());
	const auto gone = MakeTown(registry, k_TownId + 1);
	const auto ofGoneTown = MakeDispenser(registry, gone);
	registry.Destroy(gone);
	EXPECT_FALSE(building_replan::Of(registry, ofGoneTown).has_value());
}

TEST(BuildingReplan, AHouseOfATownIsPlannedAgainUnlessAScaffoldMadeIt)
{
	Registry registry;
	MakeTown(registry, k_TownId);
	const auto hut = registry.Create();
	Place(registry, hut, 0.0f);
	registry.Assign<Abode>(hut, AbodeNumber::A, k_TownId, 0u, 0u).info = AbodeInfo::CelticHut;
	const auto replan = building_replan::Of(registry, hut);
	ASSERT_TRUE(replan.has_value());
	EXPECT_EQ(replan->townId, k_TownId);
	EXPECT_EQ(replan->info, AbodeInfo::CelticHut);
	registry.Get<Abode>(hut).notPlannedAgain = true;
	EXPECT_FALSE(building_replan::Of(registry, hut).has_value());
}

TEST(BuildingReplan, AThingThatIsNotABuildingIsNotPlannedAgain)
{
	Registry registry;
	MakeTown(registry, k_TownId);
	const auto thing = registry.Create();
	Place(registry, thing, 0.0f);
	EXPECT_FALSE(building_replan::Of(registry, thing).has_value());
	EXPECT_FALSE(building_replan::Of(registry, entt::null).has_value());
}

TEST(BuildingReplan, OnlyTheDispenserKindGoesUpAsADispenser)
{
	EXPECT_TRUE(building_replan::MadeAsDispenser(AbodeType::SpellDispenser));
	EXPECT_FALSE(building_replan::MadeAsDispenser(AbodeType::Civic));
	EXPECT_FALSE(building_replan::MadeAsDispenser(AbodeType::LivingQuarters));
	EXPECT_FALSE(building_replan::MadeAsDispenser(AbodeType::Field));
	EXPECT_FALSE(building_replan::MadeAsDispenser(AbodeType::Workshop));
}

TEST(BuildingReplan, ATownWhoseLastDispenserIsPlannedAgainStays)
{
	// The plan of the lost dispenser keeps the town; with no plan, the town has nothing left and goes
	EXPECT_FALSE(town_removal::GoesWhenBuildingLeaves({.buildings = 0, .plans = 1, .people = 0}));
	EXPECT_TRUE(town_removal::GoesWhenBuildingLeaves({.buildings = 0, .plans = 0, .people = 0}));
}
