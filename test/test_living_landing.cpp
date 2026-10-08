/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The landing (docs/bw1-notes/villagers.md, "Soft drop and landing"): the original's rows from openblack's drawn
// body (living::OriginalRows), the villager's EndPhysics three poses, the animal's EndPhysics landType and heading, the
// DecomposeYXZ yaw drawn after it, the landed animation's clips and villager::SetYAngle (the wall hug's), ObjectYAngle (the
// object's y angle) and EndPhysicsWithoutBody. The reference bits come from an emulation of the original's x87
// arithmetic (GetYAngle(+x) + pi, wrapped, is 0xBFC90FDC, one bit off -pi/2).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerAnimations.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace living = openblack::ecs::living;

namespace
{
uint32_t Bits(float f)
{
	return std::bit_cast<uint32_t>(f);
}

/// the original's rows as glm columns (right, up, fwd)
glm::mat3 Rows(glm::vec3 right, glm::vec3 up, glm::vec3 fwd)
{
	return glm::mat3(right, up, fwd);
}

void ExpectNear(const glm::mat3& a, const glm::mat3& b, float eps)
{
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(a[c][r], b[c][r], eps) << "column " << c << " row " << r;
		}
	}
}
} // namespace

TEST(LivingLanding, OriginalRowsUndoTheDrawnQuarterTurn)
{
	// a villager drawn at y angle t (openblack's Transform = AngleY(t + pi / 2)) has the original's M = AngleY(t)
	for (const float t : {0.0f, 0.3f, -1.2f, 2.9f})
	{
		ExpectNear(living::OriginalRows(affine::AngleY(t + glm::half_pi<float>())), affine::AngleY(t), 1e-6f);
	}
	// exact: right = -column 2, up = column 1, fwd = column 0
	const glm::mat3 body(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(7.0f, 8.0f, 9.0f));
	const auto rows = living::OriginalRows(body);
	EXPECT_TRUE(rows[0] == glm::vec3(-7.0f, -8.0f, -9.0f));
	EXPECT_TRUE(rows[1] == glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_TRUE(rows[2] == glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(LivingLanding, VillagerPoseOnItsFeet)
{
	// right.y = 0: landType 0, Wrap(float(GetYAngle(fwd) + pi)); GetYAngle(0, 0, 1) = float(pi) (branch 3), + pi = 2 pi,
	// wrapped to 0 exactly; COMPASSION 0.1
	const auto pose = living::VillagerLandingPoseOf(Rows({1, 0, 0}, {0, 1, 0}, {0, 0, 1}));
	EXPECT_EQ(pose.landType, 0);
	EXPECT_EQ(Bits(pose.yaw), 0u);
	EXPECT_EQ(pose.desire, living::k_DesireCompassion);
	EXPECT_EQ(Bits(pose.amount), 0x3DCCCCCDu);
	// a flat matrix AngleY(t): the heading is t (the LH yaw of the fwd row)
	for (const float t : {0.4f, -2.0f, 3.0f})
	{
		EXPECT_NEAR(living::VillagerLandingPoseOf(affine::AngleY(t)).yaw, t, 1e-6f);
		EXPECT_NEAR(living::YawFromRotation(affine::AngleY(t)), t, 1e-6f);
	}
}

TEST(LivingLanding, VillagerPoseOnItsSides)
{
	// right.y < -0.5: landType 1, GetYAngle(up) stored as it is: up = +x -> float(pi / 2) (branch 2), no pi added
	const auto one = living::VillagerLandingPoseOf(Rows({0, -1, 0}, {1, 0, 0}, {0, 0, 1}));
	EXPECT_EQ(one.landType, 1);
	EXPECT_EQ(Bits(one.yaw), 0x3FC90FDBu);
	EXPECT_EQ(one.desire, living::k_DesireAnger);
	EXPECT_EQ(one.amount, 0.5f);
	// right.y > 0.5: landType 2, Wrap(float(pi / 2 + pi)) = 0xBFC90FDC (the 24-bit addition, then - 2 pi)
	const auto two = living::VillagerLandingPoseOf(Rows({0, 1, 0}, {1, 0, 0}, {0, 0, 1}));
	EXPECT_EQ(two.landType, 2);
	EXPECT_EQ(Bits(two.yaw), 0xBFC90FDCu);
	EXPECT_EQ(two.desire, living::k_DesireAnger);
	EXPECT_EQ(two.amount, 0.5f);
}

TEST(LivingLanding, VillagerPoseThresholds)
{
	const auto landType = [](float a) { return living::VillagerLandingPoseOf(Rows({0, a, 0}, {1, 0, 0}, {0, 0, 1})).landType; };
	EXPECT_EQ(landType(-0.5f), 0); // strict on both sides
	EXPECT_EQ(landType(0.5f), 0);
	EXPECT_EQ(landType(std::nextafter(-0.5f, -1.0f)), 1);
	EXPECT_EQ(landType(std::nextafter(0.5f, 1.0f)), 2);
	EXPECT_EQ(landType(std::numeric_limits<float>::quiet_NaN()), 1); // unordered goes to pose 1
}

TEST(LivingLanding, AnimalLandTypeIsReversed)
{
	EXPECT_EQ(living::AnimalLandType(0.6f), 1);
	EXPECT_EQ(living::AnimalLandType(-0.6f), 2);
	EXPECT_EQ(living::AnimalLandType(0.5f), 0);
	EXPECT_EQ(living::AnimalLandType(-0.5f), 0);
	EXPECT_EQ(living::AnimalLandType(std::numeric_limits<float>::quiet_NaN()), 2); // unordered goes to pose 2
	// the heading from the current fwd row: +z -> 0, +x -> 0xBFC90FDC
	EXPECT_EQ(Bits(living::AnimalLandingYaw({0, 0, 1})), 0u);
	EXPECT_EQ(Bits(living::AnimalLandingYaw({1, 0, 0})), 0xBFC90FDCu);
	// an animal drawn at t lands heading t
	for (const float t : {0.7f, -2.5f})
	{
		const auto rows = living::OriginalRows(living::DrawnRotation(t));
		EXPECT_NEAR(living::AnimalLandingYaw(rows[2]), t, 1e-6f);
		EXPECT_EQ(living::AnimalLandType(rows[0].y), 0);
	}
}

TEST(LivingLanding, DrawnRotation)
{
	EXPECT_TRUE(living::DrawnRotation(0.25f) == affine::AngleY(0.25f + glm::half_pi<float>()));
}

TEST(LivingLanding, LandedClips)
{
	EXPECT_EQ(ecs::VillagerLandedClip(0, true), 308);  // P_LANDED_FROM_FEET
	EXPECT_EQ(ecs::VillagerLandedClip(0, false), 309); // ..._CARRY_OBJECT
	EXPECT_EQ(ecs::VillagerLandedClip(1, true), 306);  // P_LANDED
	EXPECT_EQ(ecs::VillagerLandedClip(2, true), 307);  // P_LANDED_FROM_BACK
	EXPECT_EQ(ecs::VillagerLandedClip(3, false), 307); // no body: as 2
}

TEST(LivingLanding, VillagerSetYAngle)
{
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WallHug>(villager, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.0f);
	ecs::villager::SetYAngle(villager, 1.0f);
	// the y angle drawn at + pi / 2; GameAngle = ConvertAngle3DToGame(1) = 325 (325.949 truncated)
	EXPECT_TRUE(registry.Get<Transform>(villager).rotation == affine::AngleY(1.0f + glm::half_pi<float>()));
	EXPECT_EQ(ecs::villager::GetGameAngle(villager), gutils::ConvertAngle3DToGame(1.0f));
	EXPECT_EQ(ecs::villager::GetGameAngle(villager), 325);
	// a negative angle wraps through the mask (-0.5 -> 1886)
	ecs::villager::SetYAngle(villager, -0.5f);
	EXPECT_EQ(ecs::villager::GetGameAngle(villager), 1886);
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

TEST(LivingLanding, ObjectYAngle)
{
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = Locator::entitiesRegistry::value();
	// a villager turned by villager::SetGameAngle: its y angle = ConvertGameAngleTo3D(a), bit for bit
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WallHug>(villager, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, 0.0f);
	ecs::villager::SetGameAngle(villager, 300);
	EXPECT_EQ(Bits(living::ObjectYAngle(villager)), Bits(gutils::ConvertGameAngleTo3D(300)));
	// after SetYAngle(1.0) (y angle 1.0, GameAngle 325): no game angle draws it, so the drawn matrix's DecomposeYXZ yaw
	ecs::villager::SetYAngle(villager, 1.0f);
	EXPECT_NEAR(living::ObjectYAngle(villager), 1.0f, 1e-6f);
	// an animal drawn by FaceAngle from its game angle: exact (FaceAngle is DrawnRotation of ConvertGameAngleTo3D)
	const auto animal = registry.Create();
	registry.Assign<Transform>(animal, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<AnimalBrain>(animal).angle = 1500;
	ecs::animal_ai::detail::FaceAngle(registry.Get<Transform>(animal), 1500);
	EXPECT_TRUE(registry.Get<Transform>(animal).rotation == living::DrawnRotation(gutils::ConvertGameAngleTo3D(1500)));
	EXPECT_EQ(Bits(living::ObjectYAngle(animal)), Bits(gutils::ConvertGameAngleTo3D(1500)));
	// anything else: DecomposeYXZ's range (-pi, pi]
	const auto other = registry.Create();
	registry.Assign<Transform>(other, glm::vec3(0.0f), living::DrawnRotation(-2.5f), glm::vec3(1.0f));
	EXPECT_NEAR(living::ObjectYAngle(other), -2.5f, 1e-6f);
	registry.Get<Transform>(other).rotation = living::DrawnRotation(5.0f);
	EXPECT_NEAR(living::ObjectYAngle(other), 5.0f - glm::two_pi<float>(), 1e-6f);
	// no Transform: 0
	EXPECT_EQ(living::ObjectYAngle(registry.Create()), 0.0f);
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

TEST(LivingLanding, EndPhysicsWithoutBody)
{
	// the villager's and the animal's EndPhysics without a body: landType 3, the villager's clip 307 P_LANDED_FROM_BACK
	EXPECT_EQ(living::k_LandTypeWithoutBody, 3);
	EXPECT_EQ(ecs::VillagerLandedClip(static_cast<uint8_t>(living::k_LandTypeWithoutBody), true), 307);
	// not a Living: nothing (neither redrawn nor grounded)
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	const glm::mat3 tilted(glm::vec3(0.0f, 0.6f, 0.8f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.8f, -0.6f));
	registry.Assign<Transform>(object, glm::vec3(1.0f, 7.0f, 2.0f), tilted, glm::vec3(1.0f));
	living::EndPhysicsWithoutBody(object);
	EXPECT_TRUE(registry.Get<Transform>(object).rotation == tilted);
	EXPECT_EQ(registry.Get<Transform>(object).position.y, 7.0f);
	living::EndPhysicsWithoutBody(entt::null);
	Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}
