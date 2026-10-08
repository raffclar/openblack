/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The food and wood miracles: the hand's grain raise (HandStateGrain), SpellResource's cost per grain, the pile
// helpers of Pot::AddResourceToPos and the sprinkle effect's emission (UR_HandSprinkle + UR_WillowWisp).

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/PotResource.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "InfoConstants.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellResource.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"

using namespace openblack;

namespace
{
/// SF_Food.txt without its sound and its sprite details (the classes that make and move the grains)
constexpr std::string_view k_Sprinkle = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionTrueWhenEnabled EventConditionTrueWhenEnabled0
BEGINPROPERTIES
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_WillowWisp UR_WillowWisp_HandGlow
BEGINPROPERTIES
PROPERTY AddCastVelToInitPos BOOL 1
PROPERTY DieAge FLOAT 2
PROPERTY EmitConditionOfParent PERSIS_PNTR EventConditionTrueWhenEnabled0
PROPERTY EmitDueToMoving BOOL 1
PROPERTY EmitDueToMovingDist FLOAT 2.5
PROPERTY Group INTEGER 1
PROPERTY MaxAtoms INTEGER 36
PROPERTY MaxSpeed FLOAT 30
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RandomSpeed FLOAT 0
PROPERTY Speed FLOAT 1.0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_HandSprinkle UR_HandSprinkle0
BEGINPROPERTIES
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY InitSpeedYHumanPlayerCasting FLOAT -20.0
PROPERTY AngleToRaise FLOAT 1.07257
PROPERTY KeyPoints ARRAY SIZE 8 0 0 0.2 1 0.8 1 1 0
PROPERTY Group INTEGER 0
PROPERTY HeightToRaise FLOAT 10
PROPERTY ClampHand BOOL 1
PROPERTY TotalTime FLOAT 4
ENDPROPERTIES
ENDCLASS
)";

/// A spell behind the effect: a human player's hand cast (not this computer's interface, so the hand is left alone)
class HumanSink final: public psys::SpellSink
{
public:
	int SpellEvent(const psys::SpellEventInfo& /*event*/) override { return 1; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	[[nodiscard]] bool IsHumanPlayerCasting() const override { return true; }
};

size_t Grains(const psys::Effect& effect)
{
	// every atom but the source one (group 0)
	return effect.AtomCount() - 1;
}
} // namespace

TEST(FoodWood, grainSplineKeyPoints)
{
	using namespace ecs::systems;
	// the ctor's ends are natural (1e30): see HandGrain.h
	const auto spline = hand_grain::BuildSpline(1e30f, 1e30f);
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.0f), 0.0f, 1e-6f);
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.2f), 1.0f, 1e-6f);
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.8f), 1.0f, 1e-6f);
	EXPECT_NEAR(hand_grain::Evaluate(spline, 1.0f), 0.0f, 1e-6f);
	// symmetric, and a little over 1 between the two middle key points
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.1f), hand_grain::Evaluate(spline, 0.9f), 1e-5f);
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.5f), 1.6136f, 1e-3f);
	// natural ends: the second derivative there is 0, so the slope is not
	EXPECT_NEAR(hand_grain::Evaluate(spline, 0.001f) / 0.001f, 5.455f, 0.05f);
}

TEST(FoodWood, grainRaiseLoopsOverTotalTime)
{
	using namespace ecs::systems;
	hand_grain::Reset();
	hand_grain::Start(true, 4.0f, 10.0f, 1.07257f, true);
	const auto spline = hand_grain::BuildSpline(1e30f, 1e30f);
	for (int turn = 1; turn <= 20; ++turn)
	{
		hand_grain::GameTurnUpdate(0.1f);
	}
	// t = 20 x 0.1 / 4 = 0.5 after 2 s: the peak of the raise
	auto state = hand_grain::Debug();
	EXPECT_NEAR(state.x, 0.5f, 1e-4f);
	EXPECT_NEAR(state.y, 10.0f * hand_grain::Evaluate(spline, 0.5f), 1e-3f);
	EXPECT_NEAR(state.z, 1.07257f * hand_grain::Evaluate(spline, 0.5f), 1e-4f);
	ASSERT_TRUE(hand_grain::ClampedPosition().has_value());
	// past t = 1 it starts again at 0 (loop), height 0
	for (int turn = 21; turn <= 41; ++turn)
	{
		hand_grain::GameTurnUpdate(0.1f);
	}
	state = hand_grain::Debug();
	EXPECT_TRUE(hand_grain::Active());
	EXPECT_NEAR(state.x, 0.0f, 1e-6f);
	EXPECT_NEAR(state.y, 0.0f, 1e-6f);
	hand_grain::Stop();
	EXPECT_FALSE(hand_grain::Active());
	EXPECT_FALSE(hand_grain::ClampedPosition().has_value());
	hand_grain::Reset();
}

TEST(FoodWood, resourceEventCosts)
{
	GMagicResourceInfo food {};
	food.resourceType = ResourceType::Food;
	food.resourceAmountFirstEvent = 200;
	food.resourceAmountPerEvent = 18;
	food.costPerUnit = 7;
	const auto first = magic::ResourceEvent(food, false);
	EXPECT_EQ(first.units, 200u);
	EXPECT_FLOAT_EQ(first.chants, 1400.0f);
	const auto next = magic::ResourceEvent(food, true);
	EXPECT_EQ(next.units, 18u);
	EXPECT_FLOAT_EQ(next.chants, 126.0f);
	EXPECT_TRUE(magic::HasEnoughChantsForResourceRecast(food, 1400.0f));
	EXPECT_FALSE(magic::HasEnoughChantsForResourceRecast(food, 1399.5f));
	GMagicResourceInfo wood {};
	wood.resourceType = ResourceType::Wood;
	wood.resourceAmountFirstEvent = 500;
	wood.resourceAmountPerEvent = 20;
	wood.costPerUnit = 3;
	EXPECT_FLOAT_EQ(magic::ResourceEvent(wood, false).chants, 1500.0f);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(wood, true).chants, 60.0f);
}

TEST(FoodWood, pileHelpers)
{
	using namespace ecs::pot_resource;
	// the pile's sound sample
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 199, 5), 82);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 200, 3), 76);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 18, 6), 77);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 100, 7), 93);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 500, 11), 91);
	// PileFoodProportionRaised: 200 of 1000 -> p = 0.24 -> 1 - 0.76^2
	EXPECT_NEAR(PileFoodProportionRaised(200, 1000), 1.0f - 0.76f * 0.76f, 1e-6f);
	// an empty pile: p == 0 skips the floor, so 0
	EXPECT_EQ(PileFoodProportionRaised(0, 1000), 0.0f);
	EXPECT_NEAR(PileFoodProportionRaised(5000, 1000), 1.0f, 1e-6f);
}

TEST(FoodWood, sprinkleEmitsAtTheRateEvenWhenStill)
{
	// UR_WillowWisp: max(dt x MaxAtoms / DieAge, moved / EmitDueToMovingDist) atoms per step: 18 per second
	// from a hand that does not move, while the spell is enabled
	const auto file = psys::File::Parse(k_Sprinkle, "SF_FoodTest");
	ASSERT_TRUE(file.has_value());
	// the float array (KeyPoints) does not stop the parse, and the objects after it are read
	const auto* sprinkle = file->Find("UR_HandSprinkle0");
	ASSERT_NE(sprinkle, nullptr);
	const auto& keyPoints = sprinkle->properties.at("KeyPoints").numbers;
	ASSERT_EQ(keyPoints.size(), 8u);
	EXPECT_FLOAT_EQ(keyPoints[2], 0.2f);
	EXPECT_FLOAT_EQ(sprinkle->Float("TotalTime", 0.0f), 4.0f);
	auto shared = std::make_shared<const psys::File>(*file);
	HumanSink sink;
	psys::Effect effect(shared, glm::vec3(100.0f, 30.0f, 100.0f), 1.0f);
	effect.SetSink(&sink);
	psys::ProcessInfo info;
	info.handPos = glm::vec3(100.0f, 30.0f, 100.0f);
	info.enabled = true;
	for (int step = 0; step < 10; ++step)
	{
		effect.SetProcessInfo(info);
		effect.Step(0.1f);
	}
	// 10 x 1.8 = 18 emitted; an atom is made while the total - 1 is above the count
	EXPECT_GE(Grains(effect), 16u);
	EXPECT_LE(Grains(effect), 17u);
	// not enabled (the cast button let go): nothing more
	const auto before = Grains(effect);
	info.enabled = false;
	for (int step = 0; step < 10; ++step)
	{
		effect.SetProcessInfo(info);
		effect.Step(0.1f);
	}
	EXPECT_EQ(Grains(effect), before);
	// moving 25 m in a step: 10 grains in it (25 / 2.5)
	info.enabled = true;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	const auto still = Grains(effect);
	info.handPos.x += 25.0f;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	EXPECT_GE(Grains(effect) - still, 9u);
	EXPECT_LE(Grains(effect) - still, 11u);
}

/// With OPENBLACK_GAME_PATH set to the install: the real food and wood rows
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto* food = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Food);
	ASSERT_NE(food, nullptr);
	EXPECT_EQ(food->resourceType, ResourceType::Food);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, false).chants, 1400.0f);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, true).chants, 126.0f);
	EXPECT_EQ(food->poisoned, 0u);
	const auto* foodPowerUp = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::FoodPowerUpOne);
	ASSERT_NE(foodPowerUp, nullptr);
	EXPECT_EQ(foodPowerUp->resourceAmountPerEvent, 20u);
	EXPECT_EQ(foodPowerUp->poisoned, 1u);
	const auto* wood = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Wood);
	ASSERT_NE(wood, nullptr);
	EXPECT_EQ(wood->resourceType, ResourceType::Wood);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*wood, false).chants, 1500.0f);
	EXPECT_EQ(magic::ResourceEvent(*wood, true).units, 20u);
	EXPECT_EQ(info->pot[static_cast<size_t>(PotInfo::MagicFood)].maxAmountInPot, 1000u);
	EXPECT_EQ(static_cast<int>(info->pot[static_cast<size_t>(PotInfo::MagicWood)].nextPotForResource), 19);
}

TEST(HandMultiPickUp, RampsWithTheSquareOfTheTurns)
{
	using openblack::ecs::systems::hand_detail::PickUpAmount;
	const auto at = [](uint32_t perTurn, uint32_t perTurnEnd, float ticks, uint32_t turns) {
		const auto step = PickUpAmount(perTurn, perTurnEnd, ticks, turns);
		return std::pair {step.t, step.amount};
	};
	EXPECT_EQ(at(10, 50, 10.0f, 0), std::pair(0.0f, 10u));
	EXPECT_EQ(at(10, 50, 10.0f, 5), std::pair(0.5f, 20u));
	EXPECT_EQ(at(10, 50, 8.0f, 3), std::pair(0.375f, 15u)); // 10 + 40 x 0.140625, truncated
	EXPECT_EQ(at(10, 50, 10.0f, 10), std::pair(1.0f, 50u));
	EXPECT_EQ(at(10, 50, 10.0f, 20), std::pair(1.0f, 50u));        // t stops at 1
	EXPECT_EQ(at(10, 50, 0.0f, 5), std::pair(1.0f, 50u));          // no ramp time
	EXPECT_EQ(at(10, 50, 0.0f, 0), std::pair(0.0f, 10u));          // 0 / 0 gives 0
	EXPECT_EQ(at(50, 10, 10.0f, 5), std::pair(0.5f, 1073741824u)); // the unsigned difference wraps, as before
}
