/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro opcodes: affine::GetYAngleBetween (SET_FOCUS), the villager speed without the factor (SET_PROPERTY
// Speed, SetVillagerSpeedInMetres), the animal scale for an age (SET_PROPERTY Age) and living::SetFocus.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <bit>
#include <deque>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandomTesting.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingAngle.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// the float the callers store
uint32_t BitsOf(double a)
{
	return std::bit_cast<uint32_t>(static_cast<float>(a));
}
} // namespace

TEST(ScriptLivingProps, GetYAngleBetween)
{
	const glm::vec3 o(0.0f);
	// atan2(dz, dx): +x 0, +z float(pi / 2), -x float(pi); y is not read
	EXPECT_EQ(BitsOf(affine::GetYAngleBetween(o, {1.0f, 5.0f, 0.0f})), 0u);
	EXPECT_EQ(BitsOf(affine::GetYAngleBetween(o, {0.0f, 0.0f, 1.0f})), 0x3FC90FDBu);
	EXPECT_EQ(BitsOf(affine::GetYAngleBetween(o, {-1.0f, 0.0f, 0.0f})), 0x40490FDBu);
	// negative: + 2 pi, one 24-bit rounding of the sum: -z 4.712389, (-1, -1) 3.926991
	EXPECT_EQ(BitsOf(affine::GetYAngleBetween(o, {0.0f, 0.0f, -1.0f})), 0x4096CBE4u);
	EXPECT_EQ(BitsOf(affine::GetYAngleBetween(o, {-1.0f, 0.0f, -1.0f})), 0x407B53D2u);
	// the differences are float: from (10, 0, 20) to (11, 0, 21) is pi / 4
	EXPECT_EQ(static_cast<float>(affine::GetYAngleBetween({10.0f, 0.0f, 20.0f}, {11.0f, 0.0f, 21.0f})),
	          glm::quarter_pi<float>());
	// not negative: the extended-precision atan2 as it is; negative: the 24-bit sum
	EXPECT_EQ(affine::GetYAngleBetween(o, {0.0f, 0.0f, 1.0f}), std::atan2(1.0, 0.0));
	EXPECT_EQ(affine::GetYAngleBetween(o, {0.0f, 0.0f, -1.0f}), static_cast<double>(std::bit_cast<float>(0x4096CBE4u)));
	// one point: the same angles without the subtraction
	EXPECT_EQ(BitsOf(affine::GetYAngleOfXZ({0.0f, 9.0f, -1.0f})), 0x4096CBE4u);
	EXPECT_EQ(BitsOf(affine::GetYAngleOfXZ({-1.0f, 0.0f, 0.0f})), 0x40490FDBu);
}

TEST(ScriptLivingProps, AnimalScaleForAge)
{
	GAnimalInfo info {};
	info.grownUpAge = 5;
	info.altitudeNormal = 0.25f;
	for (size_t i = 0; i < info.ageToScale.values.size(); ++i)
	{
		info.ageToScale.values.at(i) = 0.3f + 0.05f * static_cast<float>(i);
	}
	// InitialiseScale: young ageToScale[age - 1] (age 0: the value before the table, altitudeNormal); adult 0.9
	EXPECT_EQ(ecs::animal_ai::InitialScaleForAge(info, 3), info.ageToScale.values.at(2));
	EXPECT_EQ(ecs::animal_ai::InitialScaleForAge(info, 0), 0.25f);
	EXPECT_EQ(ecs::animal_ai::InitialScaleForAge(info, 5), 0.9f);

	std::vector<float> ranges;
	std::deque<float> draws;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&](float range) {
		                                  ranges.push_back(range);
		                                  if (draws.empty())
		                                  {
			                                  return 0.0f;
		                                  }
		                                  const float r = draws.front();
		                                  draws.pop_front();
		                                  return r;
	                                  });
	// young: current + GameFloatRand((ageToScale[age + 1] - current) x 0.75)
	draws = {0.01f};
	EXPECT_EQ(ecs::animal_ai::ScaleForAge(info, 3, 0.4f), 0.4f + 0.01f);
	ASSERT_EQ(ranges.size(), 1u);
	EXPECT_EQ(ranges.at(0), (info.ageToScale.values.at(4) - 0.4f) * 0.75f);
	// adult under t: a second roll, (0.05 - r) + 1
	ranges.clear();
	draws = {0.02f, 0.07f};
	EXPECT_EQ(ecs::animal_ai::ScaleForAge(info, 7, 0.9f), (0.05f - 0.07f) + 1.0f);
	EXPECT_EQ(ranges, std::vector<float>({0.1f, 0.1f}));
	// adult already above t: one draw, the scale kept
	ranges.clear();
	draws = {0.09f};
	EXPECT_EQ(ecs::animal_ai::ScaleForAge(info, 7, 1.2f), 1.2f);
	EXPECT_EQ(ranges.size(), 1u);
	game_random::testing::SetGameRand({}, {});
}

class ScriptLivingPropsVillager: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->villager.at(0).grownUpAge = 13;
		info->villager.at(0).oldAge = 60;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeVillager()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& info = Locator::infoConstants::value().villager.at(0);
		const auto e = registry.Create();
		auto& v = registry.Assign<Villager>(e);
		v.tribe = info.tribeType;
		v.number = info.villagerNumber;
		v.life = 0.5f;
		v.town = entt::null;
		v.abode = entt::null;
		registry.Assign<Transform>(e, glm::vec3(60.0f, 0.0f, 60.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		return e;
	}
};

TEST_F(ScriptLivingPropsVillager, SpeedInMetresHasNoFactor)
{
	const auto e = MakeVillager();
	// ftol(3 / 10 x 65536) = 19660, factor 1: WallHug::speed = ToMetres(19660), read back as it is
	ecs::SetVillagerSpeedInMetres(e, 3.0f);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<WallHug>(e).speed, map_coords::ToMetres(19660));
	EXPECT_EQ(ecs::VillagerSpeedInMetres(e), map_coords::ToMetres(19660));
	// 0.7 m: 4587; a negative speed clamps to 0
	ecs::SetVillagerSpeedInMetres(e, 0.7f);
	EXPECT_EQ(ecs::VillagerSpeedInMetres(e), map_coords::ToMetres(4587));
	ecs::SetVillagerSpeedInMetres(e, -1.0f);
	EXPECT_EQ(ecs::VillagerSpeedInMetres(e), 0.0f);
}

TEST_F(ScriptLivingPropsVillager, SetFocusSnapsTheYaw)
{
	const auto e = MakeVillager();
	// facing +z from (60, 60): yaw float(pi / 2), drawn AngleY(yaw + pi / 2), WallHug's game angle 512
	ASSERT_TRUE(ecs::living::SetFocus(e, {60.0f, 3.0f, 80.0f}));
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(registry.Get<Transform>(e).rotation == affine::AngleY(glm::half_pi<float>() + glm::half_pi<float>()));
	EXPECT_EQ(registry.Get<WallHug>(e).yAngle, gutils::ConvertGameAngleTo3D(512));
	// not a villager or an animal: nothing
	const auto other = registry.Create();
	registry.Assign<Transform>(other, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(ecs::living::SetFocus(other, {1.0f, 0.0f, 0.0f}));
	EXPECT_TRUE(registry.Get<Transform>(other).rotation == glm::mat3(1.0f));
}

TEST(ScriptLivingProps, ForcedClipBounds)
{
	// OVERRIDE_STATE_ANIMATION 068: the bounds of the message and of the entry used
	EXPECT_TRUE(ecs::living::IsInvalidForcedClip(0));
	EXPECT_TRUE(ecs::living::IsInvalidForcedClip(-3));
	EXPECT_TRUE(ecs::living::IsInvalidForcedClip(441));
	EXPECT_FALSE(ecs::living::IsInvalidForcedClip(1));
	EXPECT_FALSE(ecs::living::IsInvalidForcedClip(440));
	EXPECT_EQ(ecs::living::ForcedClipIndex(0), 0); // 0 is "invalid" but still entry 0
	EXPECT_EQ(ecs::living::ForcedClipIndex(-3), 0);
	EXPECT_EQ(ecs::living::ForcedClipIndex(441), 0);
	EXPECT_EQ(ecs::living::ForcedClipIndex(418), 418);
}

TEST_F(ScriptLivingPropsVillager, SpeedAndAgeProperties)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto e = MakeVillager();
	// a Living in a living state (163 DECIDE_WHAT_TO_DO), not in the physics: GetSpeedInMetres
	registry.Assign<ecs::components::LivingAction>(e, static_cast<VillagerStates>(163), static_cast<uint16_t>(0));
	ecs::SetVillagerSpeedInMetres(e, 3.0f);
	ASSERT_TRUE(ecs::living::SpeedProperty(e).has_value());
	EXPECT_EQ(*ecs::living::SpeedProperty(e), map_coords::ToMetres(19660));
	// dead (the dead status bit): 0
	registry.Get<Villager>(e).status |= Villager::k_StatusDead;
	EXPECT_EQ(*ecs::living::SpeedProperty(e), 0.0f);
	// Age: (turn - birth) / 1500, as a float
	registry.Get<Villager>(e).birthTurn = 0;
	game_clock::SetTurn(1500u * 7u + 10u);
	ASSERT_TRUE(ecs::living::AgeProperty(e).has_value());
	EXPECT_EQ(*ecs::living::AgeProperty(e), 7.0f);
	// in the physics without a PhysicsObject: GetSpeedInMetres even when dead
	EXPECT_EQ(*ecs::living::SpeedProperty(e, true, nullptr), map_coords::ToMetres(19660));
	// not a Living: nothing (the caller logs "Not used on non living objects")
	const auto other = registry.Create();
	EXPECT_FALSE(ecs::living::SpeedProperty(other).has_value());
	EXPECT_FALSE(ecs::living::AgeProperty(other).has_value());
	// but a flying rock (any Object in the physics with its PhysicsObject): |velocity|, (z * z + y * y) + x * x
	const auto rock = registry.Create();
	ecs::physics::PhysicsObject po;
	po.entity = rock;
	po.body.velocity = glm::vec3(3.0f, 4.0f, 12.0f);
	ASSERT_TRUE(ecs::living::SpeedProperty(rock, true, &po).has_value());
	EXPECT_EQ(*ecs::living::SpeedProperty(rock, true, &po), 13.0f);
	EXPECT_FALSE(ecs::living::SpeedProperty(rock, true, nullptr).has_value());
}
