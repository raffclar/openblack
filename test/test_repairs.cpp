/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building / town side of repairs and of the town emergency: ProcessTownRepairs (the plans and the abodes
// sharing one best), the rock-damaged house's ordinary site, MoveAbodeToPlannedAbodes and the rebuild plan it leaves
// (wasBuilt), SetInStateOfEmergency / ProcessTownEmergency (emergencyStartTurn, the worship saved in
// savedWorshipPercentage) and ReduceLife's emergency for an unbuilt storage pit. No game data: a hand-made info.dat,
// abodes without meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipPercentage.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;
namespace emergency = openblack::ecs::town_emergency;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Centre = static_cast<AbodeInfo>(2);
constexpr uint32_t k_TownId = 1;

class RepairsTest: public ::testing::Test
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
		house.desireToBeRepaired = 0.5f;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
		auto& pit = info->abode.at(static_cast<size_t>(k_Pit));
		pit.abodeType = AbodeType::StoragePit;
		pit.abodeNumber = AbodeNumber::StoragePit;
		pit.tribeType = Tribe::CELTIC;
		pit.desireToBeRepaired = 0.9f;
		pit.maxVillagerNeededToBuild = 4;
		pit.thresholdForStopBeingFunctional = 0.75f;
		auto& centre = info->abode.at(static_cast<size_t>(k_Centre));
		centre.abodeType = AbodeType::TownCentre;
		centre.abodeNumber = AbodeNumber::TownCentre;
		centre.tribeType = Tribe::CELTIC;
		centre.thresholdForStopBeingFunctional = 0.75f;
		info->town.thresholdToStartRepairing = 0.9f;
		info->town.gameTurnsAfterEmergencyVillagersReact = 1200;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		game_clock::SetTurn(0);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
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

	/// An abode of a town without a mesh; under construction as the plans make them. `people` inhabitants (bare
	/// entities: only their count is read here)
	entt::entity MakeAbode(AbodeInfo info, bool underConstruction, uint32_t people = 0, uint32_t townId = k_TownId)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& abode = registry.Assign<Abode>(e, Info(info).abodeNumber, townId, 0u, 0u);
		abode.info = info;
		if (underConstruction)
		{
			abode.buildFlags = Abode::k_UnderConstruction;
			abode.percentBuilt = 0.0f;
			abode.addedToTownStats = false;
		}
		for (uint32_t i = 0; i < people; ++i)
		{
			Reg().Get<Abode>(e).inhabitants.push_back(registry.Create());
		}
		return e;
	}

	/// A plan of the house's info; `rebuild` is wasBuilt
	plans::PlanIndex AddPlan(bool rebuild)
	{
		PlannedAbode plan {k_House, glm::vec3(100.0f, 0.0f, 100.0f), 0.0f, 1.0f, false};
		plan.wasBuilt = rebuild;
		return plans::AddPlanned(_town, plan);
	}

	/// The town's worship part, with a worship site that is not a live entity (SetWorshipPercentage keeps the value
	/// only with a site; GetWorshipersNeeded reads it only when valid)
	TownMagic& Magic()
	{
		auto& registry = Reg();
		if (registry.TryGet<TownMagic>(_town) == nullptr)
		{
			const auto gone = registry.Create();
			registry.Destroy(gone);
			registry.Assign<TownMagic>(_town).worshipSite = gone;
		}
		return registry.Get<TownMagic>(_town);
	}

	entt::entity _town {entt::null};
};

// ---- ProcessTownRepairs ---------------------------------------------------------------------------------------------

TEST_F(RepairsTest, RebuildPlanBeatsALessDamagedAbode)
{
	// plan wasBuilt: desireToBeRepaired = 0.5; house at life 0.5 with someone in:
	// ((1 - 0.5) x 0.5 + 0.5) x 0.5 = 0.375 < 0.5
	AddPlan(true);
	const auto house = MakeAbode(k_House, false, 1);
	ecs::life::SetLife(house, 0.5f);
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 0u);
	EXPECT_TRUE(choice.abode == entt::null);
}

TEST_F(RepairsTest, AbodeMustBeatTheBestPlanStrictly)
{
	// life 0: (1 x 0.5 + 0.5) x 0.5 = 0.5 == the plan's 0.5: the plan stays (the abode must be strictly above)
	AddPlan(true);
	const auto house = MakeAbode(k_House, false, 1);
	ecs::life::SetLife(house, 0.0f);
	auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_TRUE(choice.abode == entt::null);
	// a storage pit at 0.6: (0.4 x 0.5 + 0.5) x 0.9 = 0.63 > 0.5 -> the abode
	const auto pit = MakeAbode(k_Pit, false);
	ecs::life::SetLife(pit, 0.6f);
	choice = sites::ChooseTownRepair(_town);
	EXPECT_EQ(choice.abode, pit);
}

TEST_F(RepairsTest, PlansWithoutRebuildFlagAndTiesKeepTheFirst)
{
	AddPlan(false); // wasBuilt false: skipped
	AddPlan(true);
	AddPlan(true); // the same 0.5: the first one with it stays
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 1u);
	EXPECT_TRUE(choice.abode == entt::null);
}

TEST_F(RepairsTest, ProcessTownRepairsMakesARepairSiteOnce)
{
	const auto pit = MakeAbode(k_Pit, false);
	ecs::life::SetLife(pit, 0.6f);
	sites::ProcessTownRepairs(_town);
	const auto site = abodes::GetBuildingSite(pit);
	ASSERT_TRUE(site != entt::null);
	// the not-repaired build flag is set before AddBuildingSite, so the site copies it: a repair site
	EXPECT_NE(Reg().Get<Abode>(pit).buildFlags & Abode::k_NotRepaired, 0u);
	EXPECT_TRUE(sites::IsRepairSite(site));
	// GetBestRepairBuildingSite: GetDesireForVillagers (4 / 4) x 0.63 > 0
	EXPECT_EQ(sites::GetBestRepairBuildingSite(_town), site);
	// the next turn the abode has a site: nothing new
	sites::ProcessTownRepairs(_town);
	EXPECT_EQ(sites::SitesOf(_town).size(), 1u);
	EXPECT_EQ(abodes::GetBuildingSite(pit), site);
}

TEST_F(RepairsTest, NothingDamagedNothingChosen)
{
	MakeAbode(k_House, false, 1); // life 1 > 0.9: 0
	MakeAbode(k_House, false, 0); // empty
	const auto choice = sites::ChooseTownRepair(_town);
	EXPECT_FALSE(choice.plan.has_value());
	EXPECT_TRUE(choice.abode == entt::null);
	sites::ProcessTownRepairs(_town);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
}

// ---- the rock-damaged house -----------------------------------------------------------------------------------------

TEST_F(RepairsTest, RockSiteIsAnOrdinarySite)
{
	const auto house = MakeAbode(k_House, false, 1);
	const map_coords::MapCoords pos {0, 0, 0.0f};
	// ReduceLife: life 0.95 < 1 -> AddBuildingSite; the site's repair flag = bit 2 = 0
	EXPECT_FLOAT_EQ(abodes::ReduceLife(house, 0.05f, std::nullopt), 0.95f);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_FALSE(sites::IsRepairSite(site));
	EXPECT_NEAR(sites::GetRepairBase(site), 1.1f * 0.95f - 0.1f, 1e-6f);
	EXPECT_TRUE(sites::GetBestRepairBuildingSite(_town) == entt::null);
	// life > 0.9: GetDesireToBeRepaired 0 -> no builders needed; only a BUILDER disciple (includeFull) takes it
	EXPECT_FALSE(sites::NeedsBuilders(site));
	EXPECT_TRUE(sites::GetBestBuildingSite(_town, pos, false) == entt::null);
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, true), site);
	// life 0.85 <= 0.9: the ordinary builders (GetBestBuildingSite, no repair flag test)
	abodes::ReduceLife(house, 0.1f, std::nullopt);
	EXPECT_TRUE(sites::NeedsBuilders(site));
	EXPECT_EQ(sites::GetBestBuildingSite(_town, pos, false), site);
	// still never the repair desire's site, and ProcessTownRepairs skips an abode with a site
	EXPECT_TRUE(sites::GetBestRepairBuildingSite(_town) == entt::null);
	EXPECT_TRUE(sites::ChooseTownRepair(_town).abode == entt::null);
	EXPECT_EQ(sites::SitesOf(_town).size(), 1u);
}

TEST_F(RepairsTest, EmptyDamagedHouseNeedsNoBuilders)
{
	const auto house = MakeAbode(k_House, false, 0);
	abodes::ReduceLife(house, 0.5f, std::nullopt);
	const auto site = abodes::GetBuildingSite(house);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(abodes::GetDesireToBeRepaired(house), 0.0f); // an empty house
	EXPECT_FALSE(sites::NeedsBuilders(site));
}

// ---- MoveAbodeToPlannedAbodes and the plan made from an abode -------------------------------------------------------

TEST_F(RepairsTest, DestroyedBuiltAbodeLeavesARebuildPlan)
{
	game_clock::SetTurn(77);
	const auto house = MakeAbode(k_House, false, 1);
	Reg().Assign<Transform>(house, glm::vec3(100.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	const auto& plan = TownData().plannedAbodes.front();
	EXPECT_TRUE(plan.wasBuilt); // the abode's build flag 8
	EXPECT_FALSE(plan.townCentre);
	EXPECT_EQ(plan.info, k_House);
	EXPECT_EQ(plan.position, glm::vec3(100.0f, 0.0f, 50.0f));
	EXPECT_FLOAT_EQ(plan.yAngleRadians, 0.0f);
	EXPECT_FLOAT_EQ(plan.scale, 1.0f);
	EXPECT_EQ(plan.creationTurn, 77u);
	EXPECT_FLOAT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.5f); // wasBuilt -> the info's desireToBeRepaired
	// ProcessTownRepairs can now pick it (its conversion sets bit 2: CreatePlannedNoFixedCheck)
	const auto choice = sites::ChooseTownRepair(_town);
	ASSERT_TRUE(choice.plan.has_value());
	EXPECT_EQ(*choice.plan, 0u);
}

TEST_F(RepairsTest, DestroyedUnbuiltAbodeLeavesAnOrdinaryPlan)
{
	const auto house = MakeAbode(k_House, true);
	Reg().Assign<Transform>(house, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(abodes::MoveAbodeToPlannedAbodes(house));
	ASSERT_EQ(plans::PlansOf(_town), 1u);
	EXPECT_FALSE(TownData().plannedAbodes.front().wasBuilt);
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);
	EXPECT_FALSE(sites::ChooseTownRepair(_town).plan.has_value());
}

TEST_F(RepairsTest, NoTownNoPlan)
{
	const auto house = MakeAbode(k_House, false, 0, 99); // town 99 does not exist: GetTown 0 -> return 0
	Reg().Assign<Transform>(house, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(abodes::MoveAbodeToPlannedAbodes(house));
	EXPECT_EQ(plans::PlansOf(_town), 0u);
}

// ---- the town emergency ---------------------------------------------------------------------------------------------

TEST_F(RepairsTest, SetInStateOfEmergencyStartsAndRefreshes)
{
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 10u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	game_clock::SetTurn(30);
	emergency::SetInStateOfEmergency(_town); // refreshed
	EXPECT_EQ(TownData().emergencyStartTurn, 30u);
	game_clock::SetTurn(1229);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1199 < 1200
	game_clock::SetTurn(1230);
	EXPECT_FALSE(ecs::town_queries::IsInStateOfEmergency(TownData())); // 1200: over
}

TEST_F(RepairsTest, ProcessTownEmergencySavesAndRestoresTheWorship)
{
	Magic().worshipPercentage = 0.4f;
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	// in the emergency: a worship percentage != 0 -> saved in savedWorshipPercentage, SetWorshipPercentage(0)
	game_clock::SetTurn(20);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 10u);
	// attacked this turn (aggressorTurn == turn): refreshed; the saved value is kept (the percentage is 0 now)
	game_clock::SetTurn(50);
	TownData().aggressorTurn = 50;
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 50u);
	EXPECT_FLOAT_EQ(TownData().savedWorshipPercentage, 0.4f);
	// still in it at 1249 (1199 turns)
	game_clock::SetTurn(1249);
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 50u);
	// over: no fire -> the worship comes back, savedWorshipPercentage = emergencyStartTurn = 0
	game_clock::SetTurn(1250);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.4f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
}

TEST_F(RepairsTest, RestoreOnlyWhenTheWorshipIsZero)
{
	// out of the emergency with a saved value but a current percentage != 0: not restored, both cleared
	Magic().worshipPercentage = 0.2f;
	TownData().savedWorshipPercentage = 0.6f;
	TownData().emergencyStartTurn = 0;
	game_clock::SetTurn(5);
	emergency::ProcessTownEmergency(_town);
	EXPECT_FLOAT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.2f);
	EXPECT_EQ(TownData().savedWorshipPercentage, 0.0f);
}

TEST_F(RepairsTest, UnbuiltStoragePitAtZeroStartsTheEmergency)
{
	// a pit under construction whose percent reaches 0 drops its life 1 -> 0, crossing
	// the 0.75 threshold: StopBeingFunctional and SetInStateOfEmergency
	game_clock::SetTurn(100);
	const auto pit = MakeAbode(k_Pit, true);
	abodes::SetPercentBuilt(pit, 0.1f);
	EXPECT_EQ(abodes::ReduceLife(pit, 0.2f, std::nullopt), 0.0f);
	EXPECT_EQ(abodes::GetPercentBuilt(pit), 0.0f);
	EXPECT_EQ(TownData().emergencyStartTurn, 100u);
	EXPECT_TRUE(ecs::town_queries::IsInStateOfEmergency(TownData()));
	// a house does not cause it (CausesTownEmergencyIfDamaged is false for an abode)
	TownData().emergencyStartTurn = 0;
	const auto house = MakeAbode(k_House, false, 1);
	abodes::ReduceLife(house, 0.5f, std::nullopt);
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
}

TEST_F(RepairsTest, DamagedPitAboveTheThresholdNoEmergency)
{
	game_clock::SetTurn(7);
	const auto pit = MakeAbode(k_Pit, false);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.875 > 0.75: still functional
	EXPECT_EQ(TownData().emergencyStartTurn, 0u);
	abodes::ReduceLife(pit, 0.125f, std::nullopt); // 0.75: 0.75 >= l -> stops, emergency
	EXPECT_EQ(TownData().emergencyStartTurn, 7u);
}

TEST_F(RepairsTest, TownCentreStopBeingFunctionalStopsTheWorship)
{
	// a town centre that stops being functional sets its town's worship percentage to 0
	Magic().worshipPercentage = 0.5f;
	const auto centre = MakeAbode(k_Centre, false);
	abodes::StopBeingFunctional(centre, std::nullopt);
	EXPECT_EQ(worship::percentage::GetWorshipPercentage(_town), 0.0f);
}

TEST_F(RepairsTest, FieldLosesNoLife)
{
	// a field carries an Abode too; its ReduceLife only returns the life: no change, no site
	const auto field = MakeAbode(k_House, false, 1);
	Reg().Assign<Field>(field);
	EXPECT_EQ(abodes::ReduceLife(field, 0.5f, std::nullopt), 1.0f);
	EXPECT_EQ(ecs::life::LifeOf(field), 1.0f);
	EXPECT_TRUE(abodes::GetBuildingSite(field) == entt::null);
	EXPECT_TRUE(sites::SitesOf(_town).empty());
}

TEST_F(RepairsTest, UpdateAggressorRecordsAndRefreshesTheEmergency)
{
	game_clock::SetTurn(10);
	emergency::SetInStateOfEmergency(_town);
	// UpdateAggressor: no caused player -> the neutral one; aggressorTurn = the turn
	game_clock::SetTurn(40);
	emergency::UpdateAggressor(_town, std::nullopt);
	EXPECT_EQ(TownData().aggressor, PlayerNames::NEUTRAL);
	EXPECT_EQ(TownData().aggressorTurn, 40u);
	// ProcessTownEmergency: attacked this turn -> refreshed
	emergency::ProcessTownEmergency(_town);
	EXPECT_EQ(TownData().emergencyStartTurn, 40u);
	emergency::UpdateAggressor(_town, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(TownData().aggressor, PlayerNames::PLAYER_ONE);
}
} // namespace
