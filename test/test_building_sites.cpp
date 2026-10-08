/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building side of plans and building sites:
// GetDesireToBeBuilt, BuildBy / Built / Abode's MakeFunctional,
// the repair branch, BuildingSite's GetNearestEdge index, GetPercentForDrawBuilding, the builders'
// count and the pruning. No game data: a hand-made info.dat, abodes without meshes (no ring, no partly built model).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Graveyard = static_cast<AbodeInfo>(2);
constexpr auto k_Football = static_cast<AbodeInfo>(3);
constexpr uint32_t k_TownId = 1;

class BuildingSitesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& house = info->abode.at(static_cast<size_t>(k_House));
		house.abodeType = AbodeType::LivingQuarters;
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::CELTIC;
		house.maxVillagersInAbode = 4;
		house.desireToBeBuilt = 0.5f;
		house.desireToBeRepaired = 0.5f;
		house.scaffoldsRequired = 2;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
		auto& pit = info->abode.at(static_cast<size_t>(k_Pit));
		pit.abodeType = AbodeType::StoragePit;
		pit.abodeNumber = AbodeNumber::StoragePit;
		pit.tribeType = Tribe::CELTIC;
		pit.desireToBeBuilt = 0.7f;
		auto& graveyard = info->abode.at(static_cast<size_t>(k_Graveyard));
		graveyard.abodeType = AbodeType::Graveyard;
		graveyard.abodeNumber = AbodeNumber::Graveyard;
		graveyard.tribeType = Tribe::CELTIC;
		graveyard.desireToBeBuilt = 0.6f;
		auto& football = info->abode.at(static_cast<size_t>(k_Football));
		football.abodeType = AbodeType::FootballPitch;
		football.abodeNumber = AbodeNumber::FootballPitch;
		football.tribeType = Tribe::CELTIC;
		football.desireToBeBuilt = 0.9f;
		info->town.thresholdToStartRepairing = 0.9f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }
	static const GAbodeInfo& Info(AbodeInfo info)
	{
		return Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
	}

	/// An abode of the town without a mesh; under construction as the plans make them (AbodeArchetype::Create(...,
	/// true))
	entt::entity MakeAbode(AbodeInfo info, bool underConstruction)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& abode = registry.Assign<Abode>(e, Info(info).abodeNumber, k_TownId, 0u, 0u);
		abode.info = info;
		if (underConstruction)
		{
			abode.buildFlags = Abode::k_UnderConstruction;
			abode.percentBuilt = 0.0f;
			abode.addedToTownStats = false;
		}
		return e;
	}

	entt::entity _town {entt::null};
};

// ---- GetDesireToBeBuilt ---------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, HouseNotWantedWithEnoughFreePlaces)
{
	// free (freeAdultPlaces - homeless) = 5 > u = (0 + 0) / 10 + 1 = 1 and n == 0 -> b = 0
	TownData().stats.freeAdultPlaces = 5;
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.0f);
}

TEST_F(BuildingSitesTest, HouseKeepsItsDesireWithFewFreePlaces)
{
	// 0 <= free <= u: b unchanged; m = 0 houses -> b - b / max(1, 10) x 0
	TownData().stats.freeAdultPlaces = 0;
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.5f);
}

TEST_F(BuildingSitesTest, HouseShortage)
{
	// free = 0 - 3 homeless; u = 1; v = min(0.5 + 3, 0.8) = 0.8; q = min(3, 10) = 3 <= M = 4 -> r = 3 / 4 x 0.2 + 0.2 =
	// 0.35; b = (0.35 + 0.6) x 0.8 = 0.76; m = 2 houses -> b - b / max(3, 10) x 2 = 0.608
	auto& town = TownData();
	town.stats.freeAdultPlaces = 0;
	town.homelessVillagers = {Reg().Create(), Reg().Create(), Reg().Create()};
	town.stats.abodesByNumber.at(static_cast<size_t>(AbodeNumber::A)) = 2;
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 0), 0.608f, 1e-6f);
}

TEST_F(BuildingSitesTest, ScaffoldCorrection)
{
	TownData().stats.freeAdultPlaces = 0;
	// n = 3 >= ScaffoldsRequired 2: c = 1 x 0.3 x 0.5 = 0.15 -> 0.35
	EXPECT_NEAR(plans::GetDesireToBeBuilt(_town, Info(k_House), 3), 0.35f, 1e-6f);
	// n = 1 < 2: (uint32)(n - 2) wraps, c is huge and r - min(c, r) = 0 (literal)
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_House), 1), 0.0f);
}

TEST_F(BuildingSitesTest, StoragePitOnlyWithoutOne)
{
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Pit), 0), 0.7f);
	TownData().storagePit = MakeAbode(k_Pit, false);
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Pit), 0), 0.0f);
}

TEST_F(BuildingSitesTest, GraveyardSharedBetweenItsSites)
{
	// two graveyard sites: r = b / s
	sites::AddBuildingSite(_town, MakeAbode(k_Graveyard, true));
	sites::AddBuildingSite(_town, MakeAbode(k_Graveyard, true));
	EXPECT_FLOAT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Graveyard), 0), 0.3f);
	TownData().graveyard = MakeAbode(k_Graveyard, false);
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Graveyard), 0), 0.0f);
}

TEST_F(BuildingSitesTest, FootballOffIsZero)
{
	EXPECT_EQ(plans::GetDesireToBeBuilt(_town, Info(k_Football), 0), 0.0f);
}

// ---- BuildBy / Built / MakeFunctional
// -------------------------------------------------------

TEST_F(BuildingSitesTest, BuildByUntilBuilt)
{
	const auto house = MakeAbode(k_House, true);
	const auto site = sites::AddBuildingSite(_town, house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(abodes::GetBuildingSite(house), site);
	EXPECT_TRUE(abodes::IsDrawBuilding(house));
	EXPECT_FALSE(abodes::IsBuilt(house));
	EXPECT_FALSE(ecs::abode_queries::IsBuilt(house));
	EXPECT_EQ(TownData().buildPulse, 1u); // set by AddBuildingSite
	ASSERT_EQ(sites::SitesOf(_town).size(), 1u);

	sites::BuildBy(site, 0.4f);
	EXPECT_FLOAT_EQ(abodes::GetPercentBuilt(house), 0.4f);
	EXPECT_FALSE(abodes::IsBuilt(house));
	sites::BuildBy(site, -1.0f); // below 0 -> 0
	EXPECT_EQ(abodes::GetPercentBuilt(house), 0.0f);
	sites::BuildBy(site, 0.5f);
	sites::BuildBy(site, 0.6f); // 1.1 >= 1 -> Built
	const auto& abode = Reg().Get<Abode>(house);
	EXPECT_TRUE(abodes::IsBuilt(house));
	EXPECT_EQ(abode.percentBuilt, 1.0f);
	EXPECT_EQ(abode.buildFlags & Abode::k_UnderConstruction, 0u);
	EXPECT_NE(abode.buildFlags & Abode::k_Built, 0u);
	EXPECT_TRUE(abode.addedToTownStats); // MakeFunctional
	EXPECT_TRUE(abode.buildingSite == entt::null);
	EXPECT_FALSE(sites::IsAvailable(site)); // Built's ToBeDeleted(0), through ecs::ToBeDeleted
	EXPECT_TRUE(sites::SitesOf(_town).empty());
	EXPECT_FALSE(Reg().Valid(site)); // the deferral off: destroyed at once
}

TEST_F(BuildingSitesTest, SetPercentBuiltBuildsAtOne)
{
	const auto house = MakeAbode(k_House, true);
	abodes::SetPercentBuilt(house, -0.5f);
	EXPECT_EQ(abodes::GetPercentBuilt(house), 0.0f);
	EXPECT_EQ(abodes::GetBuiltPercentage(house), 0.0f);
	EXPECT_TRUE(abodes::SetBuiltPercentage(house, 1.0f)); // CHL 22 -> Built
	EXPECT_TRUE(abodes::IsBuilt(house));
}

TEST_F(BuildingSitesTest, StoragePitMadeFunctionalWhenBuilt)
{
	const auto pit = MakeAbode(k_Pit, true);
	sites::AddBuildingSite(_town, pit);
	EXPECT_TRUE(TownData().storagePit == entt::null);
	sites::BuildBy(abodes::GetBuildingSite(pit), 1.0f);
	EXPECT_EQ(TownData().storagePit, pit); // the storage pit's MakeFunctional -> SetStoragePit
}

TEST_F(BuildingSitesTest, RepairBranch)
{
	const auto house = MakeAbode(k_House, false);
	ecs::life::SetLife(house, 0.5f);
	Reg().Get<Abode>(house).buildFlags |= Abode::k_NotRepaired;
	EXPECT_FALSE(abodes::IsRepaired(house));
	abodes::BuildBy(house, 0.3f); // built, not repaired -> IncreaseLife
	EXPECT_FLOAT_EQ(ecs::life::LifeOf(house), 0.8f);
	abodes::BuildBy(house, 0.3f); // capped at 1 -> Repaired: the not-repaired flag cleared, MakeFunctional
	EXPECT_EQ(ecs::life::LifeOf(house), 1.0f);
	EXPECT_EQ(Reg().Get<Abode>(house).buildFlags & Abode::k_NotRepaired, 0u);
}

// ---- builders and desire
// ---------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, BuildersNeeded)
{
	const auto site = sites::AddBuildingSite(_town, MakeAbode(k_House, true));
	EXPECT_EQ(sites::GetMaxBuilders(site), 4);
	EXPECT_EQ(sites::GetBuildersNeeded(site), 4);
	EXPECT_FLOAT_EQ(sites::GetDesireForVillagers(site), 1.0f);
	const auto a = Reg().Create();
	const auto b = Reg().Create();
	sites::AddBuilder(site, a);
	sites::AddBuilder(site, b);
	EXPECT_EQ(sites::GetBuildersNeeded(site), 2);
	EXPECT_FLOAT_EQ(sites::GetDesireForVillagers(site), 0.5f);
	EXPECT_EQ(Reg().Get<BuildingSite>(site).builders.front(), b); // the head first
	// RemoveBuilder: -1 once, also for a villager that is not there (literal)
	sites::RemoveBuilder(site, Reg().Create());
	EXPECT_EQ(sites::GetBuilderCount(site), 1);
	EXPECT_EQ(Reg().Get<BuildingSite>(site).builders.size(), 2u);
}

TEST_F(BuildingSitesTest, PruneBuiltAndRepaired)
{
	const auto house = MakeAbode(k_House, false); // built, life 1
	const auto site = sites::AddBuildingSite(_town, house);
	sites::PruneSites(_town); // built and repaired -> ToBeDeleted
	EXPECT_FALSE(sites::IsAvailable(site));
	EXPECT_TRUE(sites::SitesOf(_town).empty());
	EXPECT_TRUE(abodes::GetBuildingSite(house) == entt::null);
}

// ---- GetNearestEdge
// ------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, NearestEdgeIndex)
{
	auto& registry = Reg();
	const auto site = registry.Create();
	auto& s = registry.Assign<BuildingSite>(site);
	for (size_t i = 0; i < s.ring.size(); ++i)
	{
		s.ring.at(i) = glm::vec3(static_cast<float>(i), 0.0f, 2.0f * static_cast<float>(i));
	}
	int32_t index = -1;
	(void)sites::GetNearestEdge(site, 0.0f, index);
	EXPECT_EQ(index, 0); // angle == 0 -> 0
	(void)sites::GetNearestEdge(site, glm::pi<float>(), index);
	EXPECT_EQ(index, 64); // ftol(pi x 0.159155 x 128) = 64
	(void)sites::GetNearestEdge(site, 0.1f, index);
	EXPECT_EQ(index, 2);
	(void)sites::GetNearestEdge(site, 3.0f * glm::pi<float>(), index);
	// above 2 pi: - 2 pi; in float 3 pi - 2 pi is 3.1415925, just under pi (3.1415927), so ftol gives 63
	EXPECT_EQ(index, 63);
	(void)sites::GetNearestEdge(site, 7.0f * glm::pi<float>(), index);
	EXPECT_EQ(index, 64); // above 6 pi: pi
	(void)sites::GetNearestEdge(site, -7.0f * glm::pi<float>(), index);
	EXPECT_EQ(index, 0); // below -6 pi: 0
	// 2 pi itself is kept (only above it is reduced): ftol(2 pi x 0.159155 x 128) = 128, & 0x7F = 0
	const auto pos = sites::GetNearestEdge(site, glm::two_pi<float>(), index);
	EXPECT_EQ(index, 0);
	EXPECT_EQ(pos, (map_coords::MapCoords {0, 0, 0.0f}));
	const auto pos126 = sites::GetNearestEdge(site, 6.2f, index); // ftol(126.3) = 126
	EXPECT_EQ(index, 126);
	EXPECT_EQ(pos126.x, map_coords::ToFixed(126.0f));
	EXPECT_EQ(pos126.z, map_coords::ToFixed(252.0f));
	EXPECT_FALSE(sites::GetBuildPos(site, 128).has_value());
	EXPECT_TRUE(sites::GetBuildPos(site, 127).has_value());
}

// ---- GetPercentForDrawBuilding --------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, PercentForDrawBuilding)
{
	const auto unbuilt = MakeAbode(k_House, true);
	abodes::SetPercentBuilt(unbuilt, 0.3f);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(unbuilt), 0.3f); // not built: min(0.3, 1)
	const auto built = MakeAbode(k_House, false);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.98f); // no FragMesh: life x 0.98
	ecs::life::SetLife(built, 0.5f);
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.49f);
	sites::AddBuildingSite(_town, built); // a site but no FragMesh: still life x 0.98
	EXPECT_FLOAT_EQ(abodes::GetPercentForDrawBuilding(built), 0.49f);
	EXPECT_TRUE(abodes::IsDrawBuilding(built));
}

// ---- plans
// ------------------------------------------------------------------------------------------------------------

TEST_F(BuildingSitesTest, PlansListAndRepairDesire)
{
	PlannedAbode plan {k_House, glm::vec3(100.0f, 0.0f, 100.0f), 0.0f, 1.0f, false};
	EXPECT_EQ(plans::AddPlanned(_town, plan), 0u);
	plan.wasBuilt = true;
	EXPECT_EQ(plans::AddPlanned(_town, plan), 1u);
	EXPECT_EQ(plans::PlansOf(_town), 2u);
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);       // not built: 0
	EXPECT_FLOAT_EQ(plans::GetDesireToBeRepaired(_town, 1), 0.5f); // built: the info's desireToBeRepaired
	EXPECT_FALSE(plans::IsCivic(_town, 0));
	TownData().stats.freeAdultPlaces = 0;
	float best = 0.0f;
	const auto chosen = plans::GetBestPlanned(_town, best, 2);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, 0u); // ties: the first (oldest) one
	EXPECT_FLOAT_EQ(best, 0.5f);
	EXPECT_FALSE(plans::GetBestPlanned(_town, best, 4).has_value()); // mask 4: no civic plan
	plans::RemovePlanned(_town, 0);
	EXPECT_EQ(plans::PlansOf(_town), 1u);
}
} // namespace

TEST(ScaffoldAdjust, OnlyAnOldAnyBuildingScaffoldIsFixed)
{
	using openblack::ecs::scaffolds::CanStillBeAdjustedAfter;
	EXPECT_TRUE(CanStillBeAdjustedAfter(0, Scaffold::k_AnyAbode, 150));
	EXPECT_TRUE(CanStillBeAdjustedAfter(149, Scaffold::k_AnyAbode, 150));
	EXPECT_FALSE(CanStillBeAdjustedAfter(150, Scaffold::k_AnyAbode, 150));
	EXPECT_TRUE(CanStillBeAdjustedAfter(500, 3, 150)); // limited to one building: still adjustable
}
