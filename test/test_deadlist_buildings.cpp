/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The buildings' dead list: abodes::OnToBeDeleted, the buildings' class part of ToBeDeleted (the abode's, the storage pit's,
// the graveyard's, the creche's, the field's and the worship site's), and the behaviour it changes with the deferral off:
// - the graveyard / creche hand-on that ends at null (DeleteDependants runs twice);
// - the storage pit's SetStoragePit and its piles, the temporary pots of the town's turn (step 17);
// - the reactions and the town (RemoveStructureFromTown).
// No game data: a hand-made info.dat, abodes without meshes. ecs::ToBeDeleted calls OnToBeDeleted once the abode
// hook is in; here it is called directly.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace abodes = openblack::ecs::abodes;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
constexpr auto k_House = static_cast<AbodeInfo>(0);
constexpr auto k_Pit = static_cast<AbodeInfo>(1);
constexpr auto k_Graveyard = static_cast<AbodeInfo>(2);
constexpr auto k_Creche = static_cast<AbodeInfo>(3);
constexpr auto k_Field = static_cast<AbodeInfo>(4);
constexpr uint32_t k_TownId = 1;

class DeadlistBuildingsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		const auto set = [&info](AbodeInfo index, AbodeType type, AbodeNumber number) {
			auto& abode = info->abode.at(static_cast<size_t>(index));
			abode.abodeType = type;
			abode.abodeNumber = number;
			abode.tribeType = Tribe::CELTIC;
			abode.thresholdForStopBeingFunctional = 0.75f;
		};
		set(k_House, AbodeType::LivingQuarters, AbodeNumber::A);
		set(k_Pit, AbodeType::StoragePit, AbodeNumber::StoragePit);
		set(k_Graveyard, AbodeType::Graveyard, AbodeNumber::Graveyard);
		set(k_Creche, AbodeType::Creche, AbodeNumber::Creche);
		set(k_Field, AbodeType::Field, AbodeNumber::Field);
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
		reactions::Clear();
	}

	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }

	/// A built abode of the town (life 1: functional), without a mesh nor a Transform (MoveAbodeToPlannedAbodes then
	/// makes no plan)
	entt::entity MakeAbode(AbodeInfo info)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		const auto& record = Locator::infoConstants::value().abode.at(static_cast<size_t>(info));
		registry.Assign<Abode>(e, record.abodeNumber, k_TownId, 0u, 0u).info = info;
		return e;
	}

	/// A storage pit with its six pile slots empty
	entt::entity MakePit()
	{
		const auto pit = MakeAbode(k_Pit);
		auto& slots = Reg().Assign<StoragePit>(pit);
		slots.foodPile = entt::null;
		slots.woodPiles.fill(entt::null);
		return pit;
	}

	/// A pot of a pot info row with `amount`
	static entt::entity MakePot(uint16_t amount)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& pot = registry.Assign<Pot>(e);
		pot.amount = amount;
		pot.maxAmount = 100;
		pot.type = PotInfo::StoragePitFoodPile;
		return e;
	}

	entt::entity _town {entt::null};
};

// ---- the town's graveyard and creche (DeleteDependants twice, literal) ----------------------------------------------

TEST_F(DeadlistBuildingsTest, DeletingTheTownsGraveyardLeavesItNull)
{
	// 1st pass (the graveyard's): the town's graveyard handed to the other functional graveyard; 2nd pass (the
	// abode's): the town's graveyard is no longer this one -> SetGraveyard(0) twice
	const auto first = MakeAbode(k_Graveyard);
	Reg().Assign<Graveyard>(first);
	const auto second = MakeAbode(k_Graveyard);
	Reg().Assign<Graveyard>(second);
	TownData().graveyard = first;
	abodes::OnToBeDeleted(first);
	EXPECT_TRUE(TownData().graveyard == entt::null);
}

TEST_F(DeadlistBuildingsTest, DeletingAnyCrecheClearsTheTownsCreche)
{
	// the creche's DeleteDependants: the town's creche = found, unconditionally; found = 0 when the town's creche is
	// another
	const auto towns = MakeAbode(k_Creche);
	const auto other = MakeAbode(k_Creche);
	TownData().creche = towns;
	abodes::OnToBeDeleted(other);
	EXPECT_TRUE(TownData().creche == entt::null);
}

TEST_F(DeadlistBuildingsTest, DeletingTheTownsCrecheEndsNullToo)
{
	const auto towns = MakeAbode(k_Creche);
	MakeAbode(k_Creche);
	TownData().creche = towns;
	abodes::OnToBeDeleted(towns);
	EXPECT_TRUE(TownData().creche == entt::null);
}

// ---- the storage pit: SetStoragePit, its piles, the temporary pots --------------------------------------------------

TEST_F(DeadlistBuildingsTest, DeletingThePitHandsItOnAndDeletesItsPilesAndEmptyTemporaryPots)
{
	const auto pit = MakePit();
	const auto other = MakePit();
	TownData().storagePit = pit;
	const auto food = MakePot(10);
	const auto wood = MakePot(3);
	Reg().Get<StoragePit>(pit).foodPile = food;
	Reg().Get<StoragePit>(pit).woodPiles.at(0) = wood;
	const auto temporary = MakePot(0);
	TownData().temporaryPots.at(0) = temporary;

	abodes::OnToBeDeleted(pit);

	// the storage pit's DeleteDependants -> SetStoragePit(the other pit): its empty temporary pot goes
	EXPECT_EQ(TownData().storagePit, other);
	EXPECT_TRUE(TownData().temporaryPots.at(0) == entt::null);
	EXPECT_FALSE(Reg().Valid(temporary));
	// each available pile ToBeDeleted, its slot 0
	EXPECT_TRUE(Reg().Get<StoragePit>(pit).foodPile == entt::null);
	EXPECT_TRUE(Reg().Get<StoragePit>(pit).woodPiles.at(0) == entt::null);
	EXPECT_FALSE(Reg().Valid(food));
	EXPECT_FALSE(Reg().Valid(wood));
}

TEST_F(DeadlistBuildingsTest, TownTurnDeletesEmptyTemporaryPotsWithAFunctionalPit)
{
	// the town's turn: available, empty and a functional pit -> ToBeDeleted, slot 0; holding -> stays
	TownData().storagePit = MakePit();
	const auto empty = MakePot(0);
	const auto holding = MakePot(5);
	TownData().temporaryPots = {empty, holding};
	ecs::town_stores::ProcessTemporaryPots(_town);
	EXPECT_TRUE(TownData().temporaryPots.at(0) == entt::null);
	EXPECT_FALSE(Reg().Valid(empty));
	EXPECT_EQ(TownData().temporaryPots.at(1), holding);
	// not available -> the slot 0
	Reg().Destroy(holding);
	ecs::town_stores::ProcessTemporaryPots(_town);
	EXPECT_TRUE(TownData().temporaryPots.at(1) == entt::null);
}

TEST_F(DeadlistBuildingsTest, TemporaryPotsStayWithoutAPit)
{
	const auto empty = MakePot(0);
	TownData().temporaryPots.at(0) = empty;
	ecs::town_stores::ProcessTemporaryPots(_town);
	EXPECT_EQ(TownData().temporaryPots.at(0), empty);
}

// ---- the abode's ToBeDeleted: the town and the reactions ------------------------------------------------------------

TEST_F(DeadlistBuildingsTest, AHouseLeavesItsTownAndItsReactionsGo)
{
	const auto house = MakeAbode(k_House);
	reactions::CreateReaction(house, openblack::Reaction::LookAtObject, PlayerNames::NEUTRAL, false);
	ASSERT_NE(reactions::GetReactionInitiatedBy(house), 0u);
	ASSERT_EQ(ecs::abode_villagers::TownOf(house), _town);

	abodes::OnToBeDeleted(house);

	// RemoveStructureFromTown: no town id; RemoveAllReactionsInitiatedByObject
	EXPECT_EQ(Reg().Get<Abode>(house).townId, Abode::k_NoTown);
	EXPECT_TRUE(ecs::abode_villagers::TownOf(house) == entt::null);
	EXPECT_EQ(reactions::GetReactionInitiatedBy(house), 0u);
	// OnToBeDeleted unlinks only: the entity stays for ecs::ToBeDeleted (still available, marked last)
	EXPECT_TRUE(ecs::IsAvailable(house));
}

TEST_F(DeadlistBuildingsTest, DestroyedByEffectTwiceDoesNothingTheSecondTime)
{
	// IsAvailable -> ToBeDeleted(0); the deferral is off, so the house goes at once
	const auto house = MakeAbode(k_House);
	abodes::DestroyedByEffect(house);
	EXPECT_FALSE(Reg().Valid(house));
	abodes::DestroyedByEffect(house);
	EXPECT_FALSE(Reg().Valid(house));
}

TEST_F(DeadlistBuildingsTest, AbodeQueriesIsAvailableIsTheGameThingOne)
{
	// the generic IsAvailable for buildings too
	const auto house = MakeAbode(k_House);
	EXPECT_TRUE(ecs::abode_queries::IsAvailable(house));
	EXPECT_FALSE(ecs::abode_queries::IsAvailable(entt::null));
	Reg().Destroy(house);
	EXPECT_FALSE(ecs::abode_queries::IsAvailable(house));
}

TEST_F(DeadlistBuildingsTest, AFieldsDependantsRunOnce)
{
	// the field's ToBeDeleted -> its dependencies deleted once; the on_destroy<Field> listener then skips it
	const auto field = MakeAbode(k_Field);
	Reg().Assign<Field>(field, static_cast<int>(k_TownId));
	EXPECT_FALSE(Reg().Get<Field>(field).dependantsDeleted);
	abodes::OnToBeDeleted(field);
	EXPECT_TRUE(Reg().Get<Field>(field).dependantsDeleted);
}

// ---- the worship site's ToBeDeleted ---------------------------------------------------------------------------------

TEST_F(DeadlistBuildingsTest, AWorshipSiteIsForgottenByItsCitadelAndTowns)
{
	auto& registry = Reg();
	const auto citadel = registry.Create();
	registry.Assign<CitadelWorship>(citadel);
	const auto site = registry.Create();
	auto& data = registry.Assign<WorshipSite>(site);
	data.citadel = citadel;
	data.slot = 2;
	data.towns = {_town};
	const auto totem = registry.Create();
	registry.Assign<WorshipTotem>(totem, site);
	data.totem = totem;
	const auto pot = MakePot(4);
	registry.Get<WorshipSite>(site).foodPot = pot;
	registry.Get<CitadelWorship>(citadel).sites.at(2) = site;
	registry.Assign<TownMagic>(_town).worshipSite = site;

	abodes::OnToBeDeleted(site);

	// the totem available: ToBeDeleted; each town's worship site cleared; the citadel's site slot
	// cleared; the town list emptied; the food pot ToBeDeleted and cleared
	EXPECT_FALSE(registry.Valid(totem));
	EXPECT_TRUE(registry.Get<TownMagic>(_town).worshipSite == entt::null);
	EXPECT_TRUE(registry.Get<CitadelWorship>(citadel).sites.at(2) == entt::null);
	EXPECT_TRUE(registry.Get<WorshipSite>(site).towns.empty());
	EXPECT_TRUE(registry.Get<WorshipSite>(site).foodPot == entt::null);
	EXPECT_FALSE(registry.Valid(pot));
}

TEST_F(DeadlistBuildingsTest, AnythingElseIsIgnored)
{
	const auto pot = MakePot(1);
	abodes::OnToBeDeleted(pot);
	EXPECT_TRUE(Reg().Valid(pot));
	abodes::OnToBeDeleted(entt::null);
}
} // namespace
