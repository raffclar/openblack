/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// DeleteObjectAndTakeResource of the storage pit and the worship site: the return values, the
// storage pit's REACTION 22 and the object deleted. No game data: no entities map, so
// CreateReaction does not spread. Then the deed a storage pit reports to its creature after taking a
// resource, with the random draw of food that belonged to another player, and the owner the hand records at a pick-up
// (a pile's player, a fish farm's town's owner or none), and the interface a put-down gives the pit.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/FishFarms.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/TakeResource.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
class TakeResourceTest: public ::testing::Test
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
		openblack::test::EmplaceWorldSystems();
		reactions::Clear();
		game_clock::SetTurn(1234);
	}
	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(TakeResourceTest, StoragePitReactsWithThePlayerOfTheInterface)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_TWO, true};
	// returns true
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, hand));
	// CreateReaction(the pit, 0x16, the interface's player, stamped)
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	const auto* reaction = reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(static_cast<int>(reaction->type), 0x16);
	EXPECT_EQ(reaction->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(reaction->turnCreated, 1234u); // the last argument 1: stamped
	EXPECT_EQ(reactions::All().size(), 1u);
	// the object goes
	EXPECT_FALSE(Reg().Valid(object));
}

TEST_F(TakeResourceTest, StoragePitWithoutInterfaceIsNeutral)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	// no interface -> no player: neutral
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, {}));
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(reactions::Find(id)->player, PlayerNames::NEUTRAL);
}

TEST_F(TakeResourceTest, WorshipSiteTakesWithoutReaction)
{
	const auto site = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	// returns true, and no CreateReaction
	EXPECT_TRUE(worship::site::DeleteObjectAndTakeResource(site, object, hand));
	EXPECT_TRUE(reactions::All().empty());
	EXPECT_FALSE(Reg().Valid(object));
}

namespace
{
using ecs::StoragePitDeed;
using ecs::StoragePitStore;

/// Counts the GameRand draws (each must be GameRand(2)) and answers them in turn with `answers`, then 0
struct CountedDraws
{
	game_random::testing::ScopedState state;
	uint32_t draws {0};
	std::vector<uint32_t> answers;

	explicit CountedDraws(std::vector<uint32_t> values = {})
	    : answers(std::move(values))
	{
		game_random::testing::SetGameRand(
		    [this](uint32_t n) -> uint32_t {
			    EXPECT_EQ(n, 2u);
			    const uint32_t value = draws < answers.size() ? answers[draws] : 0;
			    ++draws;
			    return value;
		    },
		    nullptr);
	}
};

/// An interface of `player` putting down what belonged to `owner`
ecs::pot_resource::Dropper DropperOf(PlayerNames player, PlayerNames owner)
{
	ecs::pot_resource::Dropper dropper {true, player, false};
	dropper.sourceOwner = owner;
	return dropper;
}

entt::entity MakePit()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::StoragePit>(pit);
	return pit;
}
} // namespace

TEST_F(TakeResourceTest, FoodOfAnotherPlayerDrawsOnceForTheStealDeed)
{
	CountedDraws rand({1, 0});
	const auto pit = MakePit();
	const auto dropper = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// one GameRand(2) per deposit: not 0 -> stealing from a storage pit, 0 -> from a farm
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, dropper),
	          StoragePitDeed::StealFoodFromStoragePit);
	EXPECT_EQ(rand.draws, 1u);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, dropper),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 2u);
}

TEST_F(TakeResourceTest, OwnFoodAndAnyWoodDrawNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	// the player's own food
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::PutFoodInStoragePit);
	// wood, the player's own or another's
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE)),
	          StoragePitDeed::PutWoodInStoragePit);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::StealWoodFromStoragePit);
	EXPECT_EQ(rand.draws, 0u);
}

TEST_F(TakeResourceTest, WoodOnAnUnbuiltPitIsBuildingSiteWood)
{
	CountedDraws rand;
	const auto pit = MakePit();
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::Abode>(pit).buildingSite = site;
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::PutWoodInBuildingSite);
	EXPECT_EQ(rand.draws, 0u);
	// food on the same pit still goes the pit's way, with its draw
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, NoInterfaceOrNoPitDrawsNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	ecs::pot_resource::Dropper none;
	none.sourceOwner = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, none), std::nullopt);
	const auto other = Reg().Create(); // a worship site or a workshop: not a storage pit
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(other, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          std::nullopt);
	EXPECT_EQ(rand.draws, 0u);
}

TEST_F(TakeResourceTest, ADepositThatTakesNothingDrawsNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	const auto object = Reg().Create(); // no resource: the pit takes nothing, so the creature is not told
	EXPECT_TRUE(ecs::take_resource::StoragePit(pit, object, DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)));
	EXPECT_EQ(rand.draws, 0u);
}

namespace
{
/// A town of `id` owned by `owner`, listed in the registry's towns
entt::entity MakeTown(uint32_t id, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town, id).owner = owner;
	registry.Context().towns[id] = town;
	return town;
}

/// A storage pit of the town `townId` (none: Abode::k_NoTown) with a food pile and no wood piles; returns {pit, pile}
std::pair<entt::entity, entt::entity> MakePitWithPile(uint32_t townId)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::Abode>(pit).townId = townId;
	const auto pile = registry.Create();
	registry.Assign<ecs::components::Pot>(pile).type = PotInfo::StoragePitFoodPile;
	registry.Assign<ecs::components::Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& store = registry.Assign<ecs::components::StoragePit>(pit);
	store.woodPiles.fill(entt::null);
	store.foodPile = pile;
	return {pit, pile};
}

/// A loose pile (not part of any store), its own player `owner` when given (a magic pile's maker)
entt::entity MakeLoosePile(std::optional<PlayerNames> owner = std::nullopt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pile = registry.Create();
	auto& pot = registry.Assign<ecs::components::Pot>(pile);
	if (owner)
	{
		pot.owner = *owner;
	}
	return pile;
}

/// A fish farm of `town` (entt::null: none, as the fish puzzle's farms)
entt::entity MakeFishFarm(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = registry.Create();
	registry.Assign<ecs::components::FishFarm>(farm).town = town;
	return farm;
}
} // namespace

TEST_F(TakeResourceTest, PickUpFromAPileRecordsThePilesPlayer)
{
	MakeTown(4, PlayerNames::PLAYER_THREE);
	// a pile of a storage pit: the pit's town's owner
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakePitWithPile(4).second), PlayerNames::PLAYER_THREE);
	// a pit without a town: the neutral player
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakePitWithPile(ecs::components::Abode::k_NoTown).second),
	          PlayerNames::NEUTRAL);
	// a magic pile: its maker; any other loose pile: the neutral player
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakeLoosePile(PlayerNames::PLAYER_TWO)), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakeLoosePile()), PlayerNames::NEUTRAL);
	// not a pot
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(Reg().Create()), PlayerNames::NEUTRAL);
}

TEST_F(TakeResourceTest, PickUpFromAFishFarmRecordsItsTownsOwnerOrNone)
{
	const auto town = MakeTown(5, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::fish_farms::PlayerOf(MakeFishFarm(town)), PlayerNames::PLAYER_TWO);
	// no town: no owner at all, not the neutral player
	EXPECT_EQ(ecs::fish_farms::PlayerOf(MakeFishFarm(entt::null)), std::nullopt);
}

TEST_F(TakeResourceTest, TheDrawFollowsTheRecordedOwner)
{
	CountedDraws rand;
	MakeTown(6, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(6);
	// food scooped from the player's own pit and put back: the player's own, no draw
	ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	hand.sourceOwner = ecs::object_resources::PlayerOfPile(pile);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::PutFoodInStoragePit);
	EXPECT_EQ(rand.draws, 0u);
	// food from a loose pile (the neutral player's): not the player's own, one draw
	hand.sourceOwner = ecs::object_resources::PlayerOfPile(MakeLoosePile());
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 1u);
	// fish from a farm with no town: no owner, one draw
	hand.sourceOwner = ecs::fish_farms::PlayerOf(MakeFishFarm(entt::null));
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 2u);
	// an interface that has picked nothing up yet records no owner: the same
	const ecs::pot_resource::Dropper fresh {true, PlayerNames::PLAYER_ONE, true};
	EXPECT_EQ(fresh.sourceOwner, std::nullopt);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood, fresh),
	          StoragePitDeed::StealWoodFromStoragePit);
	EXPECT_EQ(rand.draws, 2u); // wood never draws
}

namespace
{
/// What a deposit into a pit of a town reaches from an interface, set up as the game has them: the info (the pit's food
/// pile with no cap, every multiplier 0, so no belief is drawn) and the day / night clock (the town's desire). Both are
/// put back when it goes
class InterfaceDepositServices
{
public:
	InterfaceDepositServices()
	{
		auto info = std::make_unique<InfoConstants>();
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = static_cast<PotInfo>(19);
		Locator::infoConstants::reset(info.release());
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
	test::RestoreService<Locator::dayNightClock> _clock;
};
} // namespace

TEST_F(TakeResourceTest, APutDownIntoAPitGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(7, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(7);
	// food the hand took from another player's store, put down on the pit of the dropper's own town
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 50, hand), 50u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 50u);
	// the pit's AddResource had the interface: the deed at the end of its town's side draws, then the put-down's own
	EXPECT_EQ(rand.draws, 2u);
}

TEST_F(TakeResourceTest, APutDownOnAPitsPileGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(8, PlayerNames::PLAYER_ONE);
	const auto pile = MakePitWithPile(8).second;
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// the pile hands it to its pit with the interface: the pit's town side and its deed; a pile tells the creature
	// nothing itself
	EXPECT_EQ(ecs::pot_resource::PotStructureAddResource(pile, ResourceType::Food, 30, false, hand), 30u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 30u);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, APutDownIntoAPitWithoutATownOnlyTellsItsOwnDeed)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// no town: no town side and no deed of its own, the put-down's deed only
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 20, hand), 20u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 20u);
	EXPECT_EQ(rand.draws, 1u);
	// no interface: nothing is told
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 5, {}), 5u);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, APoisonedPutDownPoisonsThePit)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	// unpoisoned food leaves the pit clean
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 10, {}), 10u);
	EXPECT_FALSE(ecs::object_resources::IsPoisoned(pit));
	// poisoned food put down on the pit poisons it
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 10, {}, true), 10u);
	EXPECT_TRUE(Reg().Get<ecs::components::Pot>(pile).poisoned);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
}

TEST_F(TakeResourceTest, APoisonedPutDownOnAPitsPilePoisonsThePit)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	// the pile hands the poison on to its pit with the resource
	EXPECT_EQ(ecs::pot_resource::PotStructureAddResource(pile, ResourceType::Food, 25, true), 25u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 25u);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
}

TEST_F(TakeResourceTest, AResourceGivenToAPitsPileGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(9, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(9);
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// a pile asked through the objects' AddResource passes the interface on: the pit's town side and its deed draw once
	EXPECT_EQ(ecs::object_resources::AddResource(pile, ResourceType::Food, 40, hand, true), 40u);
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Food), 40u);
	EXPECT_EQ(rand.draws, 1u);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
	// without an interface the pit's town is told nothing
	EXPECT_EQ(ecs::object_resources::AddResource(pile, ResourceType::Food, 5), 5u);
	EXPECT_EQ(rand.draws, 1u);
}
