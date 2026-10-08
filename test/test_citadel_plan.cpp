/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The citadel as a plan: the planned citadel heart (a plan only), BUILD_BUILDING's
// ForceBuildingOfPlannedAtPos converting it at once (CreatePlannedNoFixedCheck: the heart at 0 %, in the map cells,
// found by GetNearestCitadel as CALL_NEAR(CITADEL) right after it in Land 1), the CitadelBuildingSite's six piles
// (CreatePilesOfWood / GetResourcePosAndYAngle), AddResource and the heart's Built; and the temple's outside, blended
// first as the heart is made and stepped by every citadel process. No game data: a hand-made info.dat, no island, no
// meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Influence/InfluenceState.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/TempleExteriorSystem.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "Worship/Citadel.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace sites = openblack::ecs::building_sites;
namespace plans = openblack::ecs::plans;
namespace abodes = openblack::ecs::abodes;
namespace map_coords = openblack::map_coords;
using openblack::ecs::archetypes::CitadelArchetype;

namespace
{
constexpr uint32_t k_TownId = 1;
/// Land 1's temple (FollowUs L52474 BUILD_BUILDING((1915.05, 0, 2508.89), 1.0))
const glm::vec3 k_TemplePos {1915.05f, 0.0f, 2508.89f};

class CitadelPlanTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the conversion and HeartBuilt log at info level (SPDLOG_LOGGER_INFO needs the logger)
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		auto info = std::make_unique<InfoConstants>();
		// "Citadel Heart": MaxVillagerNeededToBuild 100, WoodValue 5, DesireToBeBuilt 1.0,
		// DesireToBeRepaired 5.0, StartLife 1.0
		auto& heart = info->citadelHeart;
		heart.maxVillagerNeededToBuild = 100;
		heart.woodValue = 5;
		heart.desireToBeBuilt = 1.0f;
		heart.desireToBeRepaired = 5.0f;
		heart.startLife = 1.0f;
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.potType = PotType::PileWood;
		wood.resourceType = ResourceType::Wood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		const auto townPos = k_TemplePos + glm::vec3(100.0f, 0.0f, 0.0f);
		registry.Assign<Transform>(_town, townPos, glm::mat3(1.0f), glm::vec3(1.0f));
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

	/// The one temple entity (components::CitadelHeart), or null
	static entt::entity TheHeart()
	{
		entt::entity found = entt::null;
		Reg().Each<const CitadelHeart>([&found](entt::entity e, const CitadelHeart&) { found = e; });
		return found;
	}

	/// CREATE_PLANNED_CITADEL then BUILD_BUILDING at the same point (desire 1.0 x 5)
	entt::entity PlanAndBuild()
	{
		CitadelArchetype::CreatePlan(_town, k_TemplePos, 0, 0.0f, 1.0f);
		sites::ForceBuildingOfPlannedAtPos(map_coords::FromMetres(glm::vec2(k_TemplePos.x, k_TemplePos.z)), 5.0f);
		return TheHeart();
	}

	entt::entity _town {entt::null};
};

// ---- the plan -------------------------------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, PlanIsOnlyAPlan)
{
	CitadelArchetype::CreatePlan(_town, k_TemplePos, 0, 0.5f, 1.25f);
	ASSERT_EQ(TownData().plannedAbodes.size(), 1u);
	const auto& plan = TownData().plannedAbodes.front();
	EXPECT_TRUE(plan.citadelHeart);
	EXPECT_FLOAT_EQ(plan.yAngleRadians, 0.5f);
	EXPECT_FLOAT_EQ(plan.scale, 1.25f);
	EXPECT_FALSE(plan.wasBuilt);
	// no temple, nothing drawn, nothing in the map cells, no citadel for the player
	EXPECT_EQ(Reg().Size<Temple>(), 0u);
	EXPECT_TRUE(TheHeart() == entt::null);
	EXPECT_TRUE(worship::citadel::Of(PlayerNames::PLAYER_ONE) == entt::null);
	// GetAbodeType = 0x804, IsCivic = 0, GetDesireToBeRepaired = 0 (not built)
	EXPECT_EQ(plans::GetAbodeType(_town, 0), AbodeType::Citadel);
	EXPECT_FALSE(plans::IsCivic(_town, 0));
	EXPECT_EQ(plans::GetDesireToBeRepaired(_town, 0), 0.0f);
	// GetBestPlanned(mask 4) takes it: 0x804's default case, b = DesireToBeBuilt 1.0, no site of the type
	float best = 0.0f;
	const auto chosen = plans::GetBestPlanned(_town, best, 4);
	ASSERT_TRUE(chosen.has_value());
	EXPECT_EQ(*chosen, 0u);
	EXPECT_FLOAT_EQ(best, 1.0f);
}

TEST(CitadelFlattening, LandOneCellsAtSeventyMetres)
{
	// Land 1's heart cell (191, 250) is at 44; the axis cells at 7 cells (d = 70 exactly) keep their altitude:
	// in float precision (70 - 35) x 0.0285714f is exactly 1, so a x 1 + 0 x 44 = a (one unit low in double)
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(-7, 0, 18, 44), 18);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(0, -7, 22, 44), 22);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(0, 7, 31, 44), 31);
	// within 35 m the centre's altitude, beyond 70 m the cell's own
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(3, 0, 10, 44), 44);
	EXPECT_EQ(CitadelArchetype::FlattenedAltitude(8, 0, 10, 44), 10);
}

TEST_F(CitadelPlanTest, PlanNeedsATown)
{
	CitadelArchetype::CreatePlan(entt::null, k_TemplePos, 0, 0.0f, 1.0f);
	EXPECT_TRUE(TownData().plannedAbodes.empty());
}

// ---- BUILD_BUILDING: the conversion, synchronous --------------------------------------------------------------------

TEST_F(CitadelPlanTest, BuildBuildingConvertsAtOnce)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	// the plan is gone, the heart is the player's citadel, under construction at 0 %
	EXPECT_TRUE(TownData().plannedAbodes.empty());
	EXPECT_EQ(worship::citadel::Of(PlayerNames::PLAYER_ONE), heart);
	EXPECT_EQ(Reg().Get<const Temple>(heart).owner, PlayerNames::PLAYER_ONE);
	const auto& part = Reg().Get<const CitadelPartBuild>(heart);
	EXPECT_EQ(part.buildFlags, CitadelPartBuild::k_UnderConstruction);
	EXPECT_EQ(part.percentBuilt, 0.0f);
	EXPECT_FALSE(abodes::IsBuilt(heart));
	EXPECT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.0f);
	// SetLife(StartLife); the heart's town = the plan's town
	EXPECT_EQ(ecs::life::LifeOf(heart), 1.0f);
	EXPECT_EQ(Reg().Get<const CitadelHeart>(heart).town, _town);
	// in the map cells (InsertMapObject) and found as the player's citadel near the point, as Land 1's
	// CALL_NEAR(CITADEL, 5000, pos, 5.0) at L52490 must
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
	EXPECT_EQ(ecs::map_cells::GetNearestCitadel(ecs::object::MapCoordsOf(heart), 5.0f), heart);
	// at 0 % nothing of the temple is drawn (openblack: NotDrawn); the entrance (drawn as nothing) is not in the cells
	EXPECT_TRUE(Reg().AllOf<NotDrawn>(heart));
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	ASSERT_TRUE(entrance != entt::null);
	EXPECT_EQ(Reg().Get<const CitadelEntrance>(entrance).heart, heart);
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(entrance));
}

TEST_F(CitadelPlanTest, ConversionMakesTheCitadelBuildingSite)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	ASSERT_EQ(TownData().buildingSites.size(), 1u);
	const auto site = TownData().buildingSites.front();
	EXPECT_EQ(Reg().Get<const CitadelPartBuild>(heart).buildingSite, site);
	EXPECT_EQ(abodes::GetBuildingSite(heart), site);
	ASSERT_TRUE(Reg().AllOf<CitadelBuildingSite>(site));
	// ForceBuildingOfPlannedAtPos: the site's desire = desire x 5; not a repair site
	EXPECT_FLOAT_EQ(Reg().Get<const BuildingSite>(site).desireBoost, 5.0f);
	EXPECT_FALSE(sites::IsRepairSite(site));
	// GetTown -> the heart's GetTown = 0
	EXPECT_TRUE(sites::GetTown(site) == entt::null);
	// the info's MaxVillagerNeededToBuild; the wood value 5 x GetScale (the plan's 1.0) / the neutral TribalPower[5]
	// (1)
	EXPECT_EQ(sites::GetMaxBuilders(site), 100);
	EXPECT_FLOAT_EQ(sites::GetWoodValue(site), 5.0f);
}

// ---- the six piles: CreatePilesOfWood -------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, SixEmptyPilesTwentyTwoMetresOut)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto site = TownData().buildingSites.front();
	const auto& piles = Reg().Get<const CitadelBuildingSite>(site).piles;
	const auto root = ecs::object::MapCoordsOf(heart);
	for (size_t i = 0; i < piles.size(); ++i)
	{
		const auto pile = piles.at(i);
		ASSERT_TRUE(pile != entt::null) << i;
		ASSERT_TRUE(Reg().Valid(pile)) << i;
		EXPECT_EQ(Reg().Get<const Pot>(pile).amount, 0u) << i;
		EXPECT_TRUE(sites::IsLinkedToThisBuildingSite(site, pile)) << i;
		EXPECT_EQ(sites::SiteOfPile(pile), site) << i;
		// heart angle 0 + i x 2 pi / 7 - 1.1424, 22 m from the heart's MapCoords (GetPosFromAngle)
		const auto a =
		    static_cast<float>(static_cast<double>(i) * static_cast<double>(0.8975979f) - static_cast<double>(1.1424f));
		const auto expected = map_coords::ToMetres(root + gutils::GetPosFromAngle(a, 22.0f));
		const auto& at = Reg().Get<const Transform>(pile).position;
		EXPECT_NEAR(at.x, expected.x, 1e-3f) << i;
		EXPECT_NEAR(at.z, expected.y, 1e-3f) << i;
		EXPECT_NEAR(glm::length(glm::vec2(at.x - k_TemplePos.x, at.z - k_TemplePos.z)), 22.0f, 0.01f) << i;
	}
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 0u);
}

TEST_F(CitadelPlanTest, AddResourceGoesToTheNearestPile)
{
	PlanAndBuild();
	const auto site = TownData().buildingSites.front();
	const auto& piles = Reg().Get<const CitadelBuildingSite>(site).piles;
	// AddResource: pos NULL adds nothing (the script's ADD_RESOURCE passes none)
	EXPECT_EQ(sites::AddResource(site, ResourceType::Wood, 10, nullptr), 0u);
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 0u);
	// at slot 3's position: that pile takes it
	const auto at = ecs::object::MapCoordsOf(piles.at(3));
	EXPECT_EQ(sites::GetPileWood(site, &at), piles.at(3));
	EXPECT_EQ(sites::AddResource(site, ResourceType::Wood, 10, &at), 10u);
	EXPECT_EQ(Reg().Get<const Pot>(piles.at(3)).amount, 10u);
	EXPECT_EQ(sites::GetResource(site, ResourceType::Wood), 10u);
	// the town's wood total is not moved: GetTown is 0 (the stats count only the sites of the town)
	EXPECT_EQ(sites::GetWoodForStats(site), 10u);
	// RemoveResource without an interface: the slots in order
	EXPECT_EQ(sites::RemoveResource(site, ResourceType::Wood, 4), 4u);
	EXPECT_EQ(Reg().Get<const Pot>(piles.at(3)).amount, 6u);
}

// ---- the heart's Built ----------------------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, SetPropertyBuiltPercentageAndBuilt)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto site = TownData().buildingSites.front();
	// Land 1: SET_PROPERTY(22, 0.375), then the citadel's update copies it into the 3D object's percent
	EXPECT_TRUE(abodes::SetBuiltPercentage(heart, 0.375f));
	EXPECT_FLOAT_EQ(*abodes::GetBuiltPercentage(heart), 0.375f);
	EXPECT_FALSE(abodes::IsBuilt(heart));
	worship::citadel::Process(heart);
	EXPECT_FLOAT_EQ(Reg().Get<const CitadelHeart>(heart).drawPercent, 0.375f);
	// the builders finish it: BuildBy up to 1 -> Built
	abodes::BuildBy(heart, 0.7f);
	EXPECT_TRUE(abodes::IsBuilt(heart));
	const auto& part = Reg().Get<const CitadelPartBuild>(heart);
	EXPECT_EQ(part.buildFlags & CitadelPartBuild::k_Built, CitadelPartBuild::k_Built);
	EXPECT_EQ(part.buildFlags & CitadelPartBuild::k_UnderConstruction, 0u);
	EXPECT_EQ(part.percentBuilt, 1.0f);
	// Built: the site deleted (out of the town, its empty piles gone); SetLife(1.0)
	EXPECT_TRUE(part.buildingSite == entt::null);
	EXPECT_TRUE(TownData().buildingSites.empty());
	EXPECT_FALSE(sites::IsAvailable(site));
	EXPECT_EQ(ecs::life::LifeOf(heart), 1.0f);
	// the next citadel update: the whole temple again
	worship::citadel::Process(heart);
	EXPECT_EQ(Reg().Get<const CitadelHeart>(heart).drawPercent, 1.0f);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(heart));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
}

// ---- CREATE_CITADEL -------------------------------------------------------------------------------------------------

TEST_F(CitadelPlanTest, CreateCitadelIsBuilt)
{
	const auto heart = CitadelArchetype::Create(k_TemplePos, PlayerNames::PLAYER_ONE, glm::mat4(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(heart != entt::null);
	EXPECT_TRUE(abodes::IsBuilt(heart));
	EXPECT_EQ(Reg().Get<const CitadelPartBuild>(heart).buildFlags, CitadelPartBuild::k_Built);
	EXPECT_TRUE(abodes::GetBuildingSite(heart) == entt::null);
	EXPECT_FALSE(Reg().AllOf<NotDrawn>(heart));
	EXPECT_TRUE(worship::citadel::HasLivingHeart(heart));
}

// ---- SET_INTERFACE_CITADEL 414 / the entrance's tap -----------------------------------------------------------------

TEST_F(CitadelPlanTest, BuiltPercentagePropertyOfTheHeart)
{
	// GET_PROPERTY 22 (a building's percent built) as Land 1's CheckCitadel (L53118) reads it
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	EXPECT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.0f);
	abodes::BuildBy(heart, 0.25f);
	EXPECT_FLOAT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.25f);
	EXPECT_TRUE(abodes::SetBuiltPercentage(heart, 0.375f));
	EXPECT_FLOAT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.375f);
}

TEST_F(CitadelPlanTest, EntranceTapRegisteredOnce)
{
	PlanAndBuild();
	const auto handlers = ecs::hand_tap::Handlers().size();
	CitadelArchetype::Create(k_TemplePos + glm::vec3(300.0f, 0.0f, 0.0f), PlayerNames::PLAYER_TWO, glm::mat4(1.0f),
	                         glm::vec3(1.0f));
	EXPECT_EQ(ecs::hand_tap::Handlers().size(), handlers);
	const auto entrance = Reg().Get<const CitadelHeart>(TheHeart()).entrance;
	EXPECT_NE(ecs::hand_tap::Find(entrance), nullptr);
}

TEST_F(CitadelPlanTest, InterfaceCitadelGatesTheEntrance)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	worship::citadel::ResetInterfaceCitadel(); // the script reset: 1
	EXPECT_TRUE(worship::citadel::EntranceValidToTap(entrance));
	worship::citadel::SetInterfaceCitadel(0); // Land 1 L52501
	EXPECT_FALSE(worship::citadel::EntranceValidToTap(entrance));
	EXPECT_EQ(worship::citadel::EntranceTap(entrance, true), 1u);
	worship::citadel::ResetInterfaceCitadel();
}

TEST_F(CitadelPlanTest, TheEntranceIsCollidedOnceItsTempleIsFullyBuilt)
{
	// the entrance's mesh is never drawn: the heart's draw collides it once the temple's draw percent reaches 1
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	EXPECT_FALSE(worship::citadel::EntranceCollides(entrance)); // at 0 %
	worship::citadel::SetHeartDrawPercent(heart, 0.99f);
	EXPECT_FALSE(worship::citadel::EntranceCollides(entrance));
	worship::citadel::SetHeartDrawPercent(heart, 1.0f);
	EXPECT_TRUE(worship::citadel::EntranceCollides(entrance));
	// not the heart itself, nor an entrance whose heart is gone
	EXPECT_FALSE(worship::citadel::EntranceCollides(heart));
	EXPECT_FALSE(worship::citadel::EntranceCollides(entt::null));
	const auto orphan = Reg().Create();
	Reg().Assign<CitadelEntrance>(orphan, entt::null);
	EXPECT_FALSE(worship::citadel::EntranceCollides(orphan));
}

TEST_F(CitadelPlanTest, APressOnTheEntranceTapsItUnlessTheScriptLockedIt)
{
	// the press on an object that is only tappable taps it: an abode always, an entrance while SET_INTERFACE_CITADEL
	// leaves it valid to tap
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	worship::citadel::ResetInterfaceCitadel();
	EXPECT_TRUE(worship::citadel::IsEntranceValidToTap(entrance));
	EXPECT_TRUE(ecs::hand_tap::ValidToTap(entrance, ecs::pot_resource::Dropper {true, PlayerNames::PLAYER_ONE, true}));
	worship::citadel::SetInterfaceCitadel(0); // Land 1 L52501
	EXPECT_FALSE(worship::citadel::IsEntranceValidToTap(entrance));
	worship::citadel::ResetInterfaceCitadel();
	// the heart is not the entrance
	EXPECT_FALSE(worship::citadel::IsEntranceValidToTap(heart));
	EXPECT_FALSE(worship::citadel::IsEntranceValidToTap(entt::null));
}

TEST_F(CitadelPlanTest, TheEntranceTellsItsOwnPlayerToEnterOnceItsTempleIsBuilt)
{
	using worship::citadel::EntranceToolTip;
	using worship::citadel::EntranceToolTipFor;
	// under construction: the hand goes on as over any object
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	EXPECT_EQ(EntranceToolTipFor(entrance, PlayerNames::PLAYER_ONE), EntranceToolTip::NotEntrance);
	abodes::BuildBy(heart, 1.0f);
	ASSERT_TRUE(abodes::IsBuilt(heart));
	// built: its own player is told to enter, another player gets nothing, whatever the script's lock
	EXPECT_EQ(EntranceToolTipFor(entrance, PlayerNames::PLAYER_ONE), EntranceToolTip::Enter);
	EXPECT_EQ(EntranceToolTipFor(entrance, PlayerNames::PLAYER_TWO), EntranceToolTip::Nothing);
	worship::citadel::SetInterfaceCitadel(0);
	EXPECT_EQ(EntranceToolTipFor(entrance, PlayerNames::PLAYER_ONE), EntranceToolTip::Enter);
	worship::citadel::ResetInterfaceCitadel();
	// another player's built temple
	const auto other = CitadelArchetype::Create(k_TemplePos + glm::vec3(300.0f, 0.0f, 0.0f), PlayerNames::PLAYER_TWO,
	                                            glm::mat4(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(other != entt::null);
	const auto otherEntrance = Reg().Get<const CitadelHeart>(other).entrance;
	EXPECT_EQ(EntranceToolTipFor(otherEntrance, PlayerNames::PLAYER_ONE), EntranceToolTip::Nothing);
	EXPECT_EQ(EntranceToolTipFor(otherEntrance, PlayerNames::PLAYER_TWO), EntranceToolTip::Enter);
	// not an entrance: the heart itself, nothing, an entrance whose heart is gone
	EXPECT_EQ(EntranceToolTipFor(heart, PlayerNames::PLAYER_ONE), EntranceToolTip::NotEntrance);
	EXPECT_EQ(EntranceToolTipFor(entt::null, PlayerNames::PLAYER_ONE), EntranceToolTip::NotEntrance);
	const auto orphan = Reg().Create();
	Reg().Assign<CitadelEntrance>(orphan, entt::null);
	EXPECT_EQ(EntranceToolTipFor(orphan, PlayerNames::PLAYER_ONE), EntranceToolTip::NotEntrance);
}

TEST_F(CitadelPlanTest, DeletingTheHeartTakesItsEntrance)
{
	// the heart's ToBeDeleted: the entrance's ToBeDeleted(now), then the building site's
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	const auto entrance = Reg().Get<const CitadelHeart>(heart).entrance;
	ASSERT_TRUE(entrance != entt::null);
	const auto site = abodes::GetBuildingSite(heart);
	ASSERT_TRUE(site != entt::null);
	abodes::OnToBeDeleted(heart, true);
	EXPECT_FALSE(Reg().Valid(entrance));
	EXPECT_FALSE(Reg().Valid(site));
}

// ---- the temple's outside: its first blend as the heart is made, then a step every turn -----------------------------

/// Records what the heart's creation and the citadel's process ask of the temples' outsides
class FakeTempleExterior final: public ecs::systems::TempleExteriorSystemInterface
{
public:
	struct StepCall
	{
		entt::entity heart;
		float alignmentTarget;
		float sizeTarget;
	};

	void Create(entt::entity heart) override
	{
		created.push_back(heart);
		inMapWhenCreated = ecs::map_cells::IsObjectInMap(heart);
	}
	bool Step(entt::entity heart, float alignmentTarget, float sizeTarget) override
	{
		steps.push_back({heart, alignmentTarget, sizeTarget});
		return blendDue;
	}
	void Blend(entt::entity heart) override
	{
		blends.push_back(heart);
		inMapWhenBlended = ecs::map_cells::IsObjectInMap(heart);
	}
	[[nodiscard]] std::optional<TempleExteriorMorph::State> GetLook(entt::entity) const override { return std::nullopt; }
	void SnapAlignment(entt::entity, float) override {}

	std::vector<entt::entity> created;
	std::vector<StepCall> steps;
	std::vector<entt::entity> blends;
	bool blendDue {false};
	bool inMapWhenCreated {true};
	bool inMapWhenBlended {true};
};

class CitadelOutsideTest: public CitadelPlanTest
{
protected:
	void SetUp() override
	{
		CitadelPlanTest::SetUp();
		_outside = &static_cast<FakeTempleExterior&>(Locator::templeExteriorSystem::emplace<FakeTempleExterior>());
	}

	test::RestoreService<Locator::templeExteriorSystem> _restoreOutside;
	FakeTempleExterior* _outside {nullptr};
};

TEST_F(CitadelOutsideTest, APlanHasNoOutside)
{
	CitadelArchetype::CreatePlan(_town, k_TemplePos, 0, 0.0f, 1.0f);
	EXPECT_TRUE(_outside->created.empty());
}

TEST_F(CitadelOutsideTest, TheConvertedPlanIsBlendedAtOnceAtNoneBuilt)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	// once, as it is made, before the heart enters the map cells and before any turn
	ASSERT_EQ(_outside->created.size(), 1u);
	EXPECT_EQ(_outside->created.front(), heart);
	EXPECT_FALSE(_outside->inMapWhenCreated);
	EXPECT_TRUE(_outside->steps.empty());
	EXPECT_EQ(abodes::GetBuiltPercentage(heart).value_or(-1.0f), 0.0f);
}

TEST_F(CitadelOutsideTest, CreateCitadelIsBlendedAtOnce)
{
	const auto heart = CitadelArchetype::Create(k_TemplePos, PlayerNames::PLAYER_ONE, glm::mat4(1.0f), glm::vec3(1.0f));
	ASSERT_EQ(_outside->created.size(), 1u);
	EXPECT_EQ(_outside->created.front(), heart);
}

TEST_F(CitadelOutsideTest, EveryTurnStepsBuiltOrNotOnLandOne)
{
	influence::detail::MapGlobals().landNumber = 1;
	// powers that would give another share elsewhere: land 1 always gives the small one
	influence::detail::Globals().power.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 300.0f;
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	for (int turn = 0; turn < 3; ++turn)
	{
		worship::citadel::Process(heart);
	}
	// a new game's alignment 0 is the middle; twice the small share. No blend while the step says none is due
	ASSERT_EQ(_outside->steps.size(), 3u);
	for (const auto& step : _outside->steps)
	{
		EXPECT_EQ(step.heart, heart);
		EXPECT_FLOAT_EQ(step.alignmentTarget, 0.5f);
		EXPECT_FLOAT_EQ(step.sizeTarget, 0.02f);
	}
	EXPECT_TRUE(_outside->blends.empty());
	EXPECT_FALSE(abodes::IsBuilt(heart));
}

TEST_F(CitadelOutsideTest, TheTargetsFollowTheAlignmentAndTheShareOfInfluence)
{
	influence::detail::MapGlobals().landNumber = 2;
	ecs::effects::alignment::SetClamped(PlayerNames::PLAYER_ONE, 0.6f);
	auto& power = influence::detail::Globals().power;
	power.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)) = 30.0f;
	power.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)) = 50.0f;
	// the neutral player's power counts in the sum too
	power.at(static_cast<size_t>(PlayerNames::NEUTRAL)) = 20.0f;
	const auto heart = CitadelArchetype::Create(k_TemplePos, PlayerNames::PLAYER_ONE, glm::mat4(1.0f), glm::vec3(1.0f));
	worship::citadel::Process(heart);
	ASSERT_EQ(_outside->steps.size(), 1u);
	EXPECT_FLOAT_EQ(_outside->steps.front().alignmentTarget, 0.8f);
	EXPECT_FLOAT_EQ(_outside->steps.front().sizeTarget, 0.6f);
	// nobody with any power: the small share
	power.fill(0.0f);
	worship::citadel::Process(heart);
	ASSERT_EQ(_outside->steps.size(), 2u);
	EXPECT_FLOAT_EQ(_outside->steps.back().sizeTarget, 0.02f);
}

TEST_F(CitadelOutsideTest, ABlendIsDoneOutOfTheMapCells)
{
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	ASSERT_TRUE(ecs::map_cells::IsObjectInMap(heart));
	_outside->blendDue = true;
	worship::citadel::Process(heart);
	ASSERT_EQ(_outside->blends.size(), 1u);
	EXPECT_EQ(_outside->blends.front(), heart);
	EXPECT_FALSE(_outside->inMapWhenBlended);
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
}

TEST_F(CitadelOutsideTest, TheRoutePlanArmsFollowTheOwnersAlignment)
{
	// a player system of its own: the alignments set here do not reach the other tests
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	auto& players = static_cast<ecs::systems::PlayerSystem&>(Locator::playerSystem::emplace<ecs::systems::PlayerSystem>());
	const auto heart = CitadelArchetype::Create(k_TemplePos, PlayerNames::PLAYER_TWO, glm::mat4(1.0f), glm::vec3(1.0f));
	// the circles the heart adds: the centre, six on each of the first two arms, then `count` on each of the other five
	const auto circles = [heart]() {
		auto holder = std::make_unique<route_planner::ObstacleGrid>();
		int count = 0;
		ecs::route_plan_world::AddToRoutePlan(
		    heart, *holder, 0,
		    [](void* context, entt::entity, const route_planner::Point2D&, float, int32_t) { ++*static_cast<int*>(context); },
		    &count);
		return count;
	};
	constexpr int k_Fixed = 1 + 2 * 6;
	// a new game: 4 per arm
	EXPECT_EQ(circles(), k_Fixed + 5 * 4);
	// another player's alignment does not count: only the owner's
	players.Alignment(PlayerNames::PLAYER_ONE).value = -1.0f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 4);
	// below -0.3: 3; below -0.7: 2; the thresholds themselves are not below
	players.Alignment(PlayerNames::PLAYER_TWO).value = -0.3f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 4);
	players.Alignment(PlayerNames::PLAYER_TWO).value = -0.5f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 3);
	players.Alignment(PlayerNames::PLAYER_TWO).value = -0.7f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 3);
	players.Alignment(PlayerNames::PLAYER_TWO).value = -0.8f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 2);
	// a good owner keeps 4
	players.Alignment(PlayerNames::PLAYER_TWO).value = 1.0f;
	EXPECT_EQ(circles(), k_Fixed + 5 * 4);
}

TEST_F(CitadelPlanTest, TheRealOutsideStartsNeutralAndSmallAndIsBlendedAtOnce)
{
	// The system itself, with no meshes to read: its look is taken as blended all the same, as the game's turns go
	const test::RestoreService<Locator::templeExteriorSystem> restore;
	Locator::templeExteriorSystem::emplace<ecs::systems::TempleExteriorSystem>();
	influence::detail::MapGlobals().landNumber = 1;
	ecs::effects::alignment::SetClamped(PlayerNames::PLAYER_ONE, 1.0f);
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	ASSERT_TRUE(Reg().AllOf<TempleExterior>(heart));
	const auto& look = Reg().Get<const TempleExterior>(heart).look;
	EXPECT_EQ(look.alignment, 0.5f);
	EXPECT_EQ(look.size, 0.0f);
	EXPECT_FALSE(look.neverBlended);
	EXPECT_EQ(look.blendedAlignment, 0.5f);
	// the turns: 0.516, then 0.532 is blended
	worship::citadel::Process(heart);
	EXPECT_FLOAT_EQ(Reg().Get<const TempleExterior>(heart).look.alignment, 0.516f);
	EXPECT_EQ(Reg().Get<const TempleExterior>(heart).look.blendedAlignment, 0.5f);
	worship::citadel::Process(heart);
	EXPECT_FLOAT_EQ(Reg().Get<const TempleExterior>(heart).look.blendedAlignment, 0.532f);
	EXPECT_EQ(Reg().Get<const TempleExterior>(heart).look.alignmentTarget, 0.9999f);
	EXPECT_FLOAT_EQ(Reg().Get<const TempleExterior>(heart).look.sizeTarget, 0.02f);
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
}

TEST_F(CitadelPlanTest, TheRealOutsideSnapsItsAlignmentForTheDebugWindowAndTheTurnsGoOnFromThere)
{
	const test::RestoreService<Locator::templeExteriorSystem> restore;
	auto& exterior = Locator::templeExteriorSystem::emplace<ecs::systems::TempleExteriorSystem>();
	influence::detail::MapGlobals().landNumber = 1;
	ecs::effects::alignment::SetClamped(PlayerNames::PLAYER_ONE, 1.0f);
	const auto heart = PlanAndBuild();
	ASSERT_TRUE(heart != entt::null);
	EXPECT_FALSE(exterior.GetLook(entt::null).has_value());
	ASSERT_TRUE(exterior.GetLook(heart).has_value());
	EXPECT_EQ(exterior.GetLook(heart)->alignment, 0.5f);
	// at once where a step a turn would take 32 turns, blended again, and back in the map cells
	exterior.SnapAlignment(heart, 0.9999f);
	const auto look = exterior.GetLook(heart);
	ASSERT_TRUE(look.has_value());
	EXPECT_EQ(look->alignment, 0.9999f);
	EXPECT_EQ(look->alignmentTarget, 0.9999f);
	EXPECT_EQ(look->blendedAlignment, 0.9999f);
	EXPECT_EQ(look->size, 0.0f);
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(heart));
	// the next turn finds it there
	worship::citadel::Process(heart);
	EXPECT_EQ(exterior.GetLook(heart)->alignment, 0.9999f);
	// a heart with no outside is left alone
	exterior.SnapAlignment(entt::null, 0.0f);
}
} // namespace
