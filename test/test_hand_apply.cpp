/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A held object given to the object under the hand on the action press (ecs::held_apply): the decision by held class
// and target, then the giving on a fake registry: a storage pit with its six piles, loose piles and plain objects,
// with fake info rows. No game data and no hand: the hand's own part (leaving the hand) is a callback here.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/HeldApply.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/StoragePitStore.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
namespace held_apply = openblack::ecs::held_apply;
using held_apply::ApplyAction;
using held_apply::HeldKind;
using held_apply::TargetFacts;

TEST(HeldClassApply, ATreeIsGivenOnlyToAStoreOfWood)
{
	for (const auto kind : {HeldKind::Tree, HeldKind::DeadTree})
	{
		EXPECT_EQ(held_apply::HeldClassApply(kind, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
		EXPECT_EQ(held_apply::HeldClassApply(kind, TargetFacts {}), ApplyAction::None);
	}
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Other, TargetFacts {.storesHeldType = true}), ApplyAction::None);
}

TEST(HeldClassApply, APotIsGivenToAStoreOfItsTypeElseMergedIntoAPotOfItsType)
{
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
	// a pile of a store is both: the store first
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.storesHeldType = true, .sameTypePot = true}),
	          ApplyAction::GiveToStore);
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.sameTypePot = true}), ApplyAction::MergeIntoPot);
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {}), ApplyAction::None);
}

TEST(HeldClassApply, AThrownPotGoesIntoWhatItHitsOnlyWhenBothAreAvailable)
{
	EXPECT_TRUE(held_apply::PotImpactTakes(true, true, TargetFacts {.storesHeldType = true}));
	EXPECT_TRUE(held_apply::PotImpactTakes(true, true, TargetFacts {.sameTypePot = true}));
	EXPECT_FALSE(held_apply::PotImpactTakes(true, true, TargetFacts {}));
	EXPECT_FALSE(held_apply::PotImpactTakes(false, true, TargetFacts {.storesHeldType = true}));
	EXPECT_FALSE(held_apply::PotImpactTakes(true, false, TargetFacts {.storesHeldType = true}));
}

TEST(HeldClassApply, AnAnimalIsGivenOnlyToAStoragePit)
{
	const TargetFacts pit {.storesHeldType = true, .storagePit = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Animal, pit));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, pit), ApplyAction::GiveToStore);
	// a worship site stores food, but the press gives an animal only to a pit
	const TargetFacts site {.storesHeldType = true};
	EXPECT_FALSE(held_apply::HeldClassValid(HeldKind::Animal, site));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, site), ApplyAction::None);
	// an indestructible animal over a pit: the press is valid, the apply does nothing
	const TargetFacts guarded {.storesHeldType = true, .storagePit = true, .heldIndestructible = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Animal, guarded));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, guarded), ApplyAction::None);
}

TEST(HeldClassApply, AThrownAnimalGoesIntoAFoodStoreOnlyWhenBothAreAvailable)
{
	EXPECT_TRUE(held_apply::AnimalImpactTakes(true, true, true));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(true, true, false));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(false, true, true));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(true, false, true));
}

TEST(HeldClassApply, AFenceIsGivenToAStoreOfWoodUnlessIndestructible)
{
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Fence, TargetFacts {.storesHeldType = true}));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Fence, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
	EXPECT_FALSE(held_apply::HeldClassValid(HeldKind::Fence, TargetFacts {}));
	const TargetFacts guarded {.storesHeldType = true, .heldIndestructible = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Fence, guarded));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Fence, guarded), ApplyAction::None);
}

TEST(HeldClassApply, AThrownFenceGoesIntoAWoodStoreUnlessIndestructible)
{
	EXPECT_TRUE(held_apply::FenceImpactTakes(true, false, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(false, false, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(true, true, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(true, false, false));
}

TEST(HeldClassApply, OnlyAnActionDoneConsumesTheObject)
{
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::GiveToStore, true), held_apply::k_ApplyConsumed);
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::GiveToStore, false), 0);
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::None, true), 0);
}

namespace
{
constexpr auto k_TreeType = static_cast<TreeInfo>(3);
constexpr uint32_t k_TreeWood = 400;
constexpr auto k_CowType = static_cast<AnimalInfo>(1);
constexpr auto k_TortoiseType = static_cast<AnimalInfo>(2);

class HeldApplyTest: public ::testing::Test
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
		ecs::effects::reactions::Clear();
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->pot)
		{
			row.resourceType = ResourceType::None;
			row.nextPotForResource = static_cast<PotInfo>(19); // no cap
		}
		const auto set = [&info](PotInfo pot, ResourceType type, PotType potType) {
			auto& row = info->pot.at(static_cast<size_t>(pot));
			row.resourceType = type;
			row.potType = potType;
		};
		set(PotInfo::StoragePitFoodPile, ResourceType::Food, PotType::PileFood);
		for (int i = 0; i < 5; ++i)
		{
			set(static_cast<PotInfo>(static_cast<int>(PotInfo::WoodPile_1) + i), ResourceType::Wood, PotType::PileWood);
		}
		set(PotInfo::MagicWood, ResourceType::Wood, PotType::PileWood);
		set(PotInfo::HandWood, ResourceType::Wood, PotType::Pot);
		set(PotInfo::HandFood, ResourceType::Food, PotType::Pot);
		set(PotInfo::MagicFood, ResourceType::Food, PotType::PileFood);
		info->tree.at(static_cast<size_t>(k_TreeType)).woodValue = k_TreeWood;
		auto& cow = info->animal.at(static_cast<size_t>(k_CowType));
		cow.foodValue = 1200.0f;
		cow.foodType = FoodType::Meat;
		auto& tortoise = info->animal.at(static_cast<size_t>(k_TortoiseType));
		tortoise.foodValue = 300.0f;
		tortoise.foodType = FoodType::Graze;
		auto& fence = info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::CeltFenceShort));
		fence.meshId = MeshId::BuildingCelticFenceShort;
		fence.woodValue = 25;
		info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).meshId = static_cast<MeshId>(0);
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		ecs::effects::reactions::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakePot(PotInfo type, uint16_t amount = 0)
	{
		const auto pot = Reg().Create();
		auto& component = Reg().Assign<ecs::components::Pot>(pot);
		component.type = type;
		component.amount = amount;
		Reg().Assign<ecs::components::Transform>(pot, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return pot;
	}

	/// A storage pit of no town with its five wood piles and its food pile
	static entt::entity MakePit()
	{
		const auto pit = Reg().Create();
		Reg().Assign<ecs::components::Abode>(pit);
		Reg().Assign<ecs::components::Transform>(pit, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& store = Reg().Assign<ecs::components::StoragePit>(pit);
		for (int i = 0; i < 5; ++i)
		{
			store.woodPiles.at(static_cast<size_t>(i)) =
			    MakePot(static_cast<PotInfo>(static_cast<int>(PotInfo::WoodPile_1) + i));
		}
		store.foodPile = MakePot(PotInfo::StoragePitFoodPile);
		return pit;
	}

	/// A dead tree of the fake row, at scale 1: worth k_TreeWood
	static entt::entity MakeDeadTree()
	{
		const auto tree = Reg().Create();
		Reg().Assign<ecs::components::DeadTree>(tree, ecs::components::DeadTree {k_TreeType});
		Reg().Assign<ecs::components::Transform>(tree, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return tree;
	}

	static entt::entity MakeAnimal(AnimalInfo type)
	{
		const auto animal = Reg().Create();
		Reg().Assign<ecs::components::Animal>(animal).type = type;
		Reg().Assign<ecs::components::Transform>(animal, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return animal;
	}

	static entt::entity MakeStatic(MobileStaticInfo type, float scale = 1.0f)
	{
		const auto object = Reg().Create();
		Reg().Assign<ecs::components::MobileStatic>(object).type = type;
		Reg().Assign<ecs::components::Transform>(object, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(scale));
		return object;
	}

	static ecs::components::StoragePit& Pit(entt::entity pit) { return Reg().Get<ecs::components::StoragePit>(pit); }

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(HeldApplyTest, ATreeOverAPitGoesIntoItsWood)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	EXPECT_EQ(held_apply::KindOfHeld(tree), HeldKind::DeadTree);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(tree, pit));
	int left = 0;
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pit, hand, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(Pit(pit).woodPiles.at(0)).amount, k_TreeWood); // pile 1 first
	EXPECT_FALSE(Reg().Valid(tree));
	// the pit's reaction to the hand putting something in it
	EXPECT_NE(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit), 0u);
}

TEST_F(HeldApplyTest, ATreeOverAPitsWoodPileGoesIntoThePit)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	const auto pile = Pit(pit).woodPiles.at(3);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(tree, pile));
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pile, {}, {}), held_apply::k_ApplyConsumed);
	// the pile hands it to its pit, which fills from pile 1
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(Pit(pit).woodPiles.at(0)).amount, k_TreeWood);
	EXPECT_FALSE(Reg().Valid(tree));
}

TEST_F(HeldApplyTest, ATreeOverAFoodPileALoosePileOrNothingIsNotGiven)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	const auto loose = MakePot(PotInfo::MagicWood, 50);
	const auto plain = Reg().Create();
	for (const auto target : {Pit(pit).foodPile, loose, plain, entt::entity {entt::null}})
	{
		EXPECT_FALSE(held_apply::ValidToApplyThisToObject(tree, target));
		int left = 0;
		EXPECT_EQ(held_apply::ApplyThisToObject(tree, target, {}, [&left]() { ++left; }), 0);
		EXPECT_EQ(left, 0); // still held
	}
	EXPECT_TRUE(Reg().Valid(tree));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(loose).amount, 50u);
}

TEST_F(HeldApplyTest, AHandPotOfWoodOverAPitGoesWholeIntoItsTotal)
{
	const auto pit = MakePit();
	const auto pot = MakePot(PotInfo::HandWood, 7000);
	EXPECT_EQ(held_apply::KindOfHeld(pot), HeldKind::Pot);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(pot, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(pot, pit, {}, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	// the whole amount joins the pit's single total; no pile is made beside it
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 7000u);
	EXPECT_FALSE(Reg().Valid(pot));
	size_t pots = 0;
	Reg().Each<const ecs::components::Pot>([&pots](entt::entity, const ecs::components::Pot&) { ++pots; });
	EXPECT_EQ(pots, 6u); // the pit's six piles only
}

TEST_F(HeldApplyTest, AHandPotOfFoodOverAPitsWoodPileIsNotGiven)
{
	const auto pit = MakePit();
	const auto pot = MakePot(PotInfo::HandFood, 300);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(pot, Pit(pit).woodPiles.at(0)));
	// over its food pile it is
	EXPECT_TRUE(held_apply::ValidToApplyThisToObject(pot, Pit(pit).foodPile));
}

TEST_F(HeldApplyTest, AHandPotOfWoodOverALooseWoodPileMergesIntoIt)
{
	const auto pile = MakePot(PotInfo::MagicWood, 100);
	ecs::map_cells::InsertMapObject(pile);
	const auto pot = MakePot(PotInfo::HandWood, 250); // at the pile's own point
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(pot, pile));
	EXPECT_EQ(held_apply::ApplyThisToObject(pot, pile, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 350u);
	EXPECT_FALSE(Reg().Valid(pot));
	// a loose pile of food is not a pot of its type
	const auto food = MakePot(PotInfo::MagicFood, 10);
	const auto other = MakePot(PotInfo::HandWood, 5);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(other, food));
}

TEST_F(HeldApplyTest, AnAnimalThrownIntoAPitIsItsFoodWithTheThrowersPlayer)
{
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	const ecs::pot_resource::Dropper thrower {true, PlayerNames::PLAYER_TWO, false};
	EXPECT_TRUE(ecs::resource_stores::IsResourceStore(pit, ResourceType::Food));
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, cow, thrower));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
	EXPECT_FALSE(Reg().Valid(cow));
	const auto id = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(ecs::effects::reactions::Find(id)->player, PlayerNames::PLAYER_TWO);
}

TEST_F(HeldApplyTest, AnAnimalThrownWithNoInterfaceIsTheNeutralPlayers)
{
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, cow, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
	const auto id = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(ecs::effects::reactions::Find(id)->player, PlayerNames::NEUTRAL);
}

TEST_F(HeldApplyTest, AnAnimalWorthNoFoodIsStillTakenAndGoes)
{
	const auto pit = MakePit();
	const auto tortoise = MakeAnimal(k_TortoiseType); // a grazer only: no food value
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, tortoise, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 0u);
	EXPECT_FALSE(Reg().Valid(tortoise));
}

TEST_F(HeldApplyTest, AHeldAnimalIsGivenOnlyToAPitAndNotWhenIndestructible)
{
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	EXPECT_EQ(held_apply::KindOfHeld(cow), HeldKind::Animal);
	// not to the pit's food pile: only to the pit itself
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(cow, Pit(pit).foodPile));
	Reg().Assign<ecs::components::Indestructible>(cow);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(cow, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(cow, pit, {}, [&left]() { ++left; }), 0);
	EXPECT_EQ(left, 0);
	EXPECT_TRUE(Reg().Valid(cow)); // still held
	Reg().Remove<ecs::components::Indestructible>(cow);
	EXPECT_EQ(held_apply::ApplyThisToObject(cow, pit, {}, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
}

TEST_F(HeldApplyTest, ACelticFenceOverAPitIsItsWood)
{
	const auto pit = MakePit();
	const auto fence = MakeStatic(MobileStaticInfo::CeltFenceShort);
	EXPECT_EQ(held_apply::KindOfHeld(fence), HeldKind::Fence);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(fence, pit));
	EXPECT_EQ(held_apply::ApplyThisToObject(fence, pit, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 25u);
	EXPECT_FALSE(Reg().Valid(fence));
	// scaled 0.9: 25 x 0.9^3 = 18.2
	const auto small = MakeStatic(MobileStaticInfo::CeltFenceShort, 0.9f);
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, small, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 43u);
}

TEST_F(HeldApplyTest, AnotherStaticObjectOrAnIndestructibleFenceIsNotGiven)
{
	const auto pit = MakePit();
	const auto rock = MakeStatic(MobileStaticInfo::Rock);
	EXPECT_EQ(held_apply::KindOfHeld(rock), HeldKind::Other);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(rock, pit));
	const auto fence = MakeStatic(MobileStaticInfo::CeltFenceShort);
	Reg().Assign<ecs::components::Indestructible>(fence);
	EXPECT_TRUE(held_apply::ValidToApplyThisToObject(fence, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(fence, pit, {}, [&left]() { ++left; }), 0);
	EXPECT_EQ(left, 0);
	EXPECT_TRUE(Reg().Valid(fence));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 0u);
}

TEST_F(HeldApplyTest, ATakenObjectLeavesAGhostForHalfASecond)
{
	ecs::object_ghosts::Clear();
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	Reg().Assign<ecs::components::Mesh>(tree, entt::id_type {77}, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pit, {}, {}), held_apply::k_ApplyConsumed);
	ASSERT_EQ(ecs::object_ghosts::Count(), 1u);
	const auto draws = ecs::object_ghosts::Instances();
	ASSERT_EQ(draws.size(), 2u);
	// the first draw with the moving offset at t = 500, the second at no offset in mode 10, both at the tree's place
	EXPECT_EQ(draws[0].meshId, 77u);
	EXPECT_EQ(draws[0].uv, graphics::frame_anim::GoolooFrame(500.0f).uv);
	EXPECT_FALSE(draws[0].mode.has_value());
	EXPECT_EQ(draws[1].uv, glm::vec2(0.0f));
	EXPECT_EQ(draws[1].mode, graphics::render_modes::Mode::AlphaTexturedAlphaAdditiveChroma);
	EXPECT_EQ(glm::vec3(draws[0].model[3]), glm::vec3(10.0f, 5.0f, 10.0f));
	// it melts with the frames and is gone after 500 ms
	ecs::object_ghosts::Update(250.0f);
	EXPECT_EQ(ecs::object_ghosts::Count(), 1u);
	EXPECT_EQ(ecs::object_ghosts::Instances()[0].uv, graphics::frame_anim::GoolooFrame(250.0f).uv);
	ecs::object_ghosts::Update(250.0f);
	EXPECT_EQ(ecs::object_ghosts::Count(), 0u);
	// an object without a mesh leaves none
	const auto bare = MakeDeadTree();
	EXPECT_EQ(held_apply::ApplyThisToObject(bare, pit, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(ecs::object_ghosts::Count(), 0u);
}
