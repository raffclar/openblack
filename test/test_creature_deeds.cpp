/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDeeds.h"
#include "ECS/Systems/InfluenceSystemInterface.h"

using namespace openblack;
using namespace openblack::creature_deeds;

namespace
{
uint32_t Zero(uint32_t /*range*/)
{
	return 0;
}
uint32_t One(uint32_t /*range*/)
{
	return 1;
}
} // namespace

TEST(CreatureDeeds, WoodToABuildingThatGoesUpIsAlwaysTheBuildingSiteDeed)
{
	for (const auto receiver : {Receiver::StoragePit, Receiver::WorshipSite, Receiver::Workshop, Receiver::Other})
	{
		EXPECT_EQ(GiftDeed({.receiver = receiver, .goingUp = true, .resource = ResourceType::Wood}, Zero),
		          k_PutWoodInBuildingSite);
	}
	// Food to a building that goes up is not
	EXPECT_FALSE(
	    GiftDeed({.receiver = Receiver::Other, .goingUp = true, .resource = ResourceType::Food, .ownResource = true}, Zero)
	        .has_value());
}

TEST(CreatureDeeds, AStoragePitGiftIsAPutOrATheftBySourceOfTheResource)
{
	const Gift ownFood {.receiver = Receiver::StoragePit, .resource = ResourceType::Food, .ownResource = true};
	EXPECT_EQ(GiftDeed(ownFood, Zero), k_PutFoodInStoragePit);
	Gift stolenFood = ownFood;
	stolenFood.ownResource = false;
	EXPECT_EQ(GiftDeed(stolenFood, Zero), k_StealFoodFromFarm);
	EXPECT_EQ(GiftDeed(stolenFood, One), k_StealFoodFromStoragePit);
	const Gift ownWood {.receiver = Receiver::StoragePit, .resource = ResourceType::Wood, .ownResource = true};
	EXPECT_EQ(GiftDeed(ownWood, Zero), k_PutWoodInStoragePit);
	Gift stolenWood = ownWood;
	stolenWood.ownResource = false;
	EXPECT_EQ(GiftDeed(stolenWood, Zero), k_StealWoodFromStoragePit);
}

TEST(CreatureDeeds, WorshipSitesTakeFoodAndWorkshopsWood)
{
	EXPECT_EQ(GiftDeed({.receiver = Receiver::WorshipSite, .resource = ResourceType::Food}, Zero), k_PutFoodInWorshipSite);
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::WorshipSite, .resource = ResourceType::Wood}, Zero).has_value());
	EXPECT_EQ(GiftDeed({.receiver = Receiver::Workshop, .resource = ResourceType::Wood}, Zero), k_PutWoodInWorkshop);
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Workshop, .resource = ResourceType::Food}, Zero).has_value());
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Other, .resource = ResourceType::Food}, Zero).has_value());
}

TEST(CreatureDeeds, DisciplesFollowTheSeaDeedByTheirType)
{
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Farmer), 22u);
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Craftsman), 29u);
	// From the trader on, each type shows the deed of the next (vanilla bug)
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Trader), 30u);
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Worship), k_TakeObjectHome);
}

TEST(CreatureDeeds, ALandingNearTheTempleIsTakenHomeOrStolen)
{
	const Landing home {.player = PlayerNames::PLAYER_ONE, .nearOwnTemple = true};
	EXPECT_EQ(LandingDeed(home), k_TakeObjectHome);
	Landing own = home;
	own.thingsTown = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(LandingDeed(own), k_TakeObjectHome);
	Landing stolen = home;
	stolen.thingsTown = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(LandingDeed(stolen), k_StealObjectAndPutByTemple);
	// Away from the temple, only a thing of another player's town put by the player's own town is a deed
	stolen.nearOwnTemple = false;
	EXPECT_FALSE(LandingDeed(stolen).has_value());
	stolen.nearestTown = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(LandingDeed(stolen), k_StealObjectAndPutInTown);
	Landing nothing {.player = PlayerNames::PLAYER_ONE, .nearestTown = PlayerNames::PLAYER_ONE};
	EXPECT_FALSE(LandingDeed(nothing).has_value());
}

TEST(CreatureDeeds, TheMostInfluentialPlayerNeedsMoreThanNoInfluenceAndTheFirstWinsATie)
{
	using ecs::systems::influence_rules::MostInfluentialPlayer;
	using ecs::systems::influence_rules::PlayerInfluenceAt;
	EXPECT_EQ(MostInfluentialPlayer(std::array<PlayerInfluenceAt, 0> {}), PlayerNames::NEUTRAL);
	const std::array none {PlayerInfluenceAt {.player = PlayerNames::PLAYER_ONE, .influence = 0.0f}};
	EXPECT_EQ(MostInfluentialPlayer(none), PlayerNames::NEUTRAL);
	const std::array tie {PlayerInfluenceAt {.player = PlayerNames::PLAYER_ONE, .influence = 0.5f},
	                      PlayerInfluenceAt {.player = PlayerNames::PLAYER_TWO, .influence = 0.5f},
	                      PlayerInfluenceAt {.player = PlayerNames::PLAYER_THREE, .influence = 0.4f}};
	EXPECT_EQ(MostInfluentialPlayer(tie), PlayerNames::PLAYER_ONE);
}
