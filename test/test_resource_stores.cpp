/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which objects store which resource (ecs::resource_stores): the table by class, a structure's pile asking its
// structure for its own type, and what an animal and a fence are worth. No game data: fake info rows and a fake
// registry of a storage pit with its six piles, a workshop with its pile, buildings with and without a building site.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
namespace stores = openblack::ecs::resource_stores;
using stores::StoreKind;

TEST(ResourceStoreTable, AStoragePitTakesAnyType)
{
	for (const auto type : {ResourceType::Food, ResourceType::Wood, ResourceType::Any, ResourceType::None})
	{
		EXPECT_TRUE(stores::IsStoreForType(StoreKind::StoragePit, type, false));
	}
}

TEST(ResourceStoreTable, ABuildingTakesWoodOnlyOnItsBuildingSite)
{
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::Abode, ResourceType::Wood, false));
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::Abode, ResourceType::Wood, true));
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::Abode, ResourceType::Food, true));
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::Abode, ResourceType::Any, true));
}

TEST(ResourceStoreTable, AWorkshopTakesWoodAndAny)
{
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::Workshop, ResourceType::Wood, false));
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::Workshop, ResourceType::Any, false));
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::Workshop, ResourceType::Food, false));
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::Workshop, ResourceType::Food, true));
}

TEST(ResourceStoreTable, AWorshipSiteTakesFoodAndWoodOnlyWithASite)
{
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::WorshipSite, ResourceType::Food, false));
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::WorshipSite, ResourceType::Any, false));
	EXPECT_FALSE(stores::IsStoreForType(StoreKind::WorshipSite, ResourceType::Wood, false));
	EXPECT_TRUE(stores::IsStoreForType(StoreKind::WorshipSite, ResourceType::Wood, true));
}

TEST(ResourceStoreTable, LoosePotsAndOtherObjectsTakeNothing)
{
	for (const auto type : {ResourceType::Food, ResourceType::Wood, ResourceType::Any})
	{
		EXPECT_FALSE(stores::IsStoreForType(StoreKind::LoosePot, type, true));
		EXPECT_FALSE(stores::IsStoreForType(StoreKind::None, type, true));
	}
}

TEST(ResourceStoreTable, AStructuresPileTakesItsOwnTypeOrAny)
{
	// a storage pit's wood pile
	EXPECT_TRUE(stores::PileIsStoreForType(true, ResourceType::Wood, ResourceType::Wood));
	EXPECT_FALSE(stores::PileIsStoreForType(true, ResourceType::Food, ResourceType::Wood));
	EXPECT_TRUE(stores::PileIsStoreForType(true, ResourceType::Any, ResourceType::Wood));
	// its structure does not take the type: nothing
	EXPECT_FALSE(stores::PileIsStoreForType(false, ResourceType::Wood, ResourceType::Wood));
}

TEST(ResourceValues, AnAnimalIsWorthItsFoodOnlyAsMeatOrVegetable)
{
	EXPECT_EQ(stores::AnimalFood(1, 1200.0f), 1200u); // meat
	EXPECT_EQ(stores::AnimalFood(2, 1200.0f), 1200u); // vegetable
	EXPECT_EQ(stores::AnimalFood(3, 1200.0f), 1200u);
	EXPECT_EQ(stores::AnimalFood(0, 1200.0f), 0u);
	EXPECT_EQ(stores::AnimalFood(4, 1200.0f), 0u); // a grazer only
	EXPECT_EQ(stores::AnimalFood(5, 1200.0f), 1200u);
	EXPECT_EQ(stores::AnimalFood(1, 99.9f), 99u); // truncated
}

TEST(ResourceValues, AFenceIsWorthItsWoodByLifeAndScaleCubed)
{
	EXPECT_EQ(stores::FenceWood(25, 1.0f, 1.0f), 25u);
	EXPECT_EQ(stores::FenceWood(25, 1.0f, 0.9f), 18u); // 18.225
	EXPECT_EQ(stores::FenceWood(25, 0.5f, 1.0f), 12u); // 12.5
	EXPECT_EQ(stores::FenceWood(25, 0.0f, 1.0f), 0u);
}

namespace
{
class ResourceStoresTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->pot)
		{
			row.resourceType = ResourceType::None;
		}
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).resourceType = ResourceType::Food;
		for (auto pile = PotInfo::WoodPile_1; pile <= PotInfo::WoodPile_5;
		     pile = static_cast<PotInfo>(static_cast<int32_t>(pile) + 1))
		{
			info->pot.at(static_cast<size_t>(pile)).resourceType = ResourceType::Wood;
		}
		info->pot.at(static_cast<size_t>(PotInfo::MagicWood)).resourceType = ResourceType::Wood;
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakePot(PotInfo type)
	{
		const auto pot = Reg().Create();
		Reg().Assign<ecs::components::Pot>(pot).type = type;
		Reg().Assign<ecs::components::Transform>(pot, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return pot;
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(ResourceStoresTest, APitAndItsPilesAnswerByThePilesType)
{
	const auto pit = Reg().Create();
	Reg().Assign<ecs::components::Abode>(pit);
	auto& store = Reg().Assign<ecs::components::StoragePit>(pit);
	for (size_t i = 0; i < store.woodPiles.size(); ++i)
	{
		store.woodPiles.at(i) =
		    MakePot(static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + static_cast<int32_t>(i)));
	}
	store.foodPile = MakePot(PotInfo::StoragePitFoodPile);
	const auto woodPile = Reg().Get<ecs::components::StoragePit>(pit).woodPiles.at(2);
	const auto foodPile = Reg().Get<ecs::components::StoragePit>(pit).foodPile;

	EXPECT_EQ(stores::KindOf(pit), StoreKind::StoragePit);
	EXPECT_TRUE(stores::IsResourceStore(pit, ResourceType::Food));
	EXPECT_TRUE(stores::IsResourceStore(pit, ResourceType::Wood));
	EXPECT_EQ(stores::KindOf(woodPile), StoreKind::StructurePile);
	EXPECT_TRUE(stores::IsResourceStore(woodPile, ResourceType::Wood));
	EXPECT_TRUE(stores::IsResourceStore(woodPile, ResourceType::Any));
	EXPECT_FALSE(stores::IsResourceStore(woodPile, ResourceType::Food));
	EXPECT_TRUE(stores::IsResourceStore(foodPile, ResourceType::Food));
	EXPECT_FALSE(stores::IsResourceStore(foodPile, ResourceType::Wood));
}

TEST_F(ResourceStoresTest, ALoosePileAndAPlainObjectStoreNothing)
{
	const auto pile = MakePot(PotInfo::MagicWood);
	EXPECT_EQ(stores::KindOf(pile), StoreKind::LoosePot);
	EXPECT_FALSE(stores::IsResourceStore(pile, ResourceType::Wood));
	const auto thing = Reg().Create();
	EXPECT_EQ(stores::KindOf(thing), StoreKind::None);
	EXPECT_FALSE(stores::IsResourceStore(thing, ResourceType::Wood));
	EXPECT_FALSE(stores::IsResourceStore(entt::null, ResourceType::Wood));
}

TEST_F(ResourceStoresTest, AWorkshopsPileTakesWoodThroughItsWorkshop)
{
	const auto workshop = Reg().Create();
	Reg().Assign<ecs::components::Abode>(workshop);
	const auto pile = MakePot(PotInfo::MagicWood);
	Reg().Assign<ecs::components::Workshop>(workshop).woodPile = pile;
	EXPECT_EQ(stores::KindOf(workshop), StoreKind::Workshop);
	EXPECT_TRUE(stores::IsResourceStore(workshop, ResourceType::Wood));
	EXPECT_FALSE(stores::IsResourceStore(workshop, ResourceType::Food));
	EXPECT_EQ(stores::KindOf(pile), StoreKind::StructurePile);
	EXPECT_TRUE(stores::IsResourceStore(pile, ResourceType::Wood));
	EXPECT_FALSE(stores::IsResourceStore(pile, ResourceType::Food));
}

TEST_F(ResourceStoresTest, ABuildingTakesWoodWhileItHasABuildingSite)
{
	const auto abode = Reg().Create();
	Reg().Assign<ecs::components::Abode>(abode);
	EXPECT_EQ(stores::KindOf(abode), StoreKind::Abode);
	EXPECT_FALSE(stores::IsResourceStore(abode, ResourceType::Wood));
	// nothing given is taken without a site, and the object stays
	const auto object = Reg().Create();
	EXPECT_FALSE(stores::DeleteObjectAndTakeResource(abode, object, {}));
	EXPECT_TRUE(Reg().Valid(object));
	Reg().Get<ecs::components::Abode>(abode).buildingSite = Reg().Create();
	EXPECT_TRUE(stores::IsResourceStore(abode, ResourceType::Wood));
	EXPECT_FALSE(stores::IsResourceStore(abode, ResourceType::Food));
}

TEST_F(ResourceStoresTest, AWorshipSiteTakesFood)
{
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::WorshipSite>(site);
	EXPECT_EQ(stores::KindOf(site), StoreKind::WorshipSite);
	EXPECT_TRUE(stores::IsResourceStore(site, ResourceType::Food));
	EXPECT_FALSE(stores::IsResourceStore(site, ResourceType::Wood));
}
