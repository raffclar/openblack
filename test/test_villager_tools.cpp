/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerHeldObject.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
void ExpectNear(const glm::vec3& a, const glm::vec3& b)
{
	EXPECT_NEAR(a.x, b.x, 1e-5f);
	EXPECT_NEAR(a.y, b.y, 1e-5f);
	EXPECT_NEAR(a.z, b.z, 1e-5f);
}

/// A villager with nothing to show, no tool from its states, its loads under what shows (50 wood, 100 food)
villager_animation::CarryInputs Plain()
{
	return {.scriptHeld = false,
	        .life = 1.0f,
	        .lifeWhenCrawlsWounded = 0.15f,
	        .wood = 0,
	        .food = 0,
	        .minWoodToShowGraphic = 50,
	        .minFoodToShowGraphic = 100,
	        .woodKind = 0,
	        .building = false,
	        .finalStateCarries = 0,
	        .topStateCarries = 0};
}
} // namespace

TEST(VillagerTools, EveryObjectHasItsOwnHeldModel)
{
	using villager_held::MeshOf;
	EXPECT_EQ(MeshOf(CarriedObject::Axe), MeshId::ObjectAxe);
	EXPECT_EQ(MeshOf(CarriedObject::FishingRod), MeshId::ObjectFishingRod);
	EXPECT_EQ(MeshOf(CarriedObject::Crook), MeshId::ObjectCrook);
	EXPECT_EQ(MeshOf(CarriedObject::Saw), MeshId::ObjectSaw);
	EXPECT_EQ(MeshOf(CarriedObject::Bag), MeshId::ObjectBag);
	EXPECT_EQ(MeshOf(CarriedObject::Ball), MeshId::ObjectBall);
	EXPECT_EQ(MeshOf(CarriedObject::Hammer), MeshId::ObjectHammer);
	EXPECT_EQ(MeshOf(CarriedObject::MalletHeavy), MeshId::ObjectMalletHeavy);
	EXPECT_EQ(MeshOf(CarriedObject::Scythe), MeshId::ObjectScythe);
	EXPECT_EQ(MeshOf(CarriedObject::Spade), MeshId::ObjectSpade);
	EXPECT_EQ(MeshOf(CarriedObject::Wood), MeshId::ObjectWoodInHand);
	EXPECT_EQ(MeshOf(CarriedObject::Tree_1), MeshId::ObjectBranchEvergreenInHand);
	EXPECT_EQ(MeshOf(CarriedObject::Tree_2), MeshId::ObjectBranchFruitInHand);
	EXPECT_EQ(MeshOf(CarriedObject::Tree_3), MeshId::ObjectBranchHardWoodInHand);
	EXPECT_EQ(MeshOf(CarriedObject::None), std::nullopt);
	EXPECT_EQ(MeshOf(CarriedObject::NoChange), std::nullopt);
}

TEST(VillagerTools, LoadsShowOnlyAboveWhatShows)
{
	using villager_animation::CarriedObjectOf;
	auto in = Plain();
	in.wood = 50;
	in.food = 100;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::None);
	in.food = 101;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Bag);
	// Wood over what shows wins over any food
	in.wood = 51;
	in.food = 500;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Wood);
	in.woodKind = 3;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Tree_3);
}

TEST(VillagerTools, BuildersShowNoFoodAndTheWoundedNoLoad)
{
	using villager_animation::CarriedObjectOf;
	auto in = Plain();
	in.food = 200;
	in.building = true;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::None);
	// A builder's wood still shows
	in.wood = 60;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Wood);
	// Badly wounded it shows no load, only the tool of its state
	in = Plain();
	in.wood = 60;
	in.life = 0.15f;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::None);
	in.finalStateCarries = static_cast<int32_t>(CarriedObject::Axe);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Axe);
}

TEST(VillagerTools, TheTopStateOverridesTheFinalStateWhichOverridesTheLoad)
{
	using villager_animation::CarriedObjectOf;
	auto in = Plain();
	in.food = 200;
	in.finalStateCarries = static_cast<int32_t>(CarriedObject::Axe);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Axe);
	// A state that names "nothing" puts the tool and the load away
	in.topStateCarries = static_cast<int32_t>(CarriedObject::None);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::None);
	in.topStateCarries = static_cast<int32_t>(CarriedObject::FishingRod);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::FishingRod);
	// A script keeps what it has
	in.scriptHeld = true;
	EXPECT_EQ(CarriedObjectOf(in), std::nullopt);
}

TEST(VillagerTools, TheHeldThingSitsOnTheGripTurnedToIt)
{
	// A grip a little way out from the villager, turned a quarter about the up axis
	const auto grip = glm::translate(glm::mat4(1.0f), glm::vec3(0.3f, 1.2f, 0.1f)) * glm::eulerAngleY(1.0f);
	const auto villager = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f)) * glm::eulerAngleY(-0.5f) *
	                      glm::scale(glm::mat4(1.0f), glm::vec3(1.2f));
	const auto held = villager_held::HeldMatrix(villager, grip);
	const auto gripped = villager * grip;
	// It keeps the grip's place
	ExpectNear(glm::vec3(held[3]), glm::vec3(gripped[3]));
	// Its sideways axis runs against the grip's, its up axis against the grip's forward and its forward against the grip's
	// up
	ExpectNear(glm::vec3(held[0]), -glm::vec3(gripped[0]));
	ExpectNear(glm::vec3(held[1]), -glm::vec3(gripped[2]));
	ExpectNear(glm::vec3(held[2]), -glm::vec3(gripped[1]));
	// It is not mirrored, and keeps the villager's size
	EXPECT_GT(glm::determinant(glm::mat3(held)), 0.0f);
	EXPECT_NEAR(glm::length(glm::vec3(held[0])), 1.2f, 1e-5f);
}

TEST(VillagerTools, AHeldThingFadesByItselfElseWithTheVillager)
{
	using villager_held::HeldAlpha;
	EXPECT_EQ(HeldAlpha(std::nullopt, std::nullopt), std::nullopt);
	EXPECT_EQ(HeldAlpha(std::nullopt, uint8_t {120}), uint8_t {120});
	EXPECT_EQ(HeldAlpha(uint8_t {40}, uint8_t {120}), uint8_t {40});
	EXPECT_EQ(HeldAlpha(uint8_t {0}, std::nullopt), uint8_t {0});
}

TEST(VillagerTools, OnlyShownWoodOverWhatShowsFalls)
{
	using villager_held::DropsLog;
	EXPECT_TRUE(DropsLog(CarriedObject::Tree_1, 51, 50));
	EXPECT_FALSE(DropsLog(CarriedObject::Tree_1, 50, 50));
	// Wood a villager doesn't show stays with it
	EXPECT_FALSE(DropsLog(CarriedObject::None, 300, 50));
	EXPECT_FALSE(DropsLog(CarriedObject::NoChange, 300, 50));
	// Any shown thing lets the wood fall, a tool or a bag too
	EXPECT_TRUE(DropsLog(CarriedObject::Hammer, 300, 50));
	EXPECT_TRUE(DropsLog(CarriedObject::Bag, 300, 50));
	// A load wrapped below nothing never falls
	EXPECT_FALSE(DropsLog(CarriedObject::Wood, -5, 50));
}

TEST(VillagerTools, TheLogHoldsTheCarriedWood)
{
	constexpr float k_PineWood = 400.0f;
	const float multiplier = villager_held::LogWoodMultiplier(100, k_PineWood);
	EXPECT_FLOAT_EQ(multiplier, 0.25f);
	// A whole pine's worth at that multiplier is the wood carried
	EXPECT_FLOAT_EQ(k_PineWood * multiplier, 100.0f);
}

TEST(VillagerTools, AFallenTreeGivesTheKindOfLogItIsDrawnAs)
{
	using villager_held::WoodKindOfFallenTree;
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectWoodInHand, CarriedTreeType::Evergreen), 0);
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectBranchEvergreenInHand, CarriedTreeType::Hardwood), 1);
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectBranchFruitInHand, CarriedTreeType::Evergreen), 2);
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectBranchHardWoodInHand, CarriedTreeType::Evergreen), 3);
	// Drawn as anything else (a tool, or its tree's own model), it gives its tree's kind: a dropped log is a pine's
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectHammer, CarriedTreeType::Evergreen), 1);
	EXPECT_EQ(WoodKindOfFallenTree(MeshId::ObjectAxe, CarriedTreeType::Hardwood), 3);
}
