/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numbers>

#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "Scenery/AppleBarrelRules.h"

using namespace openblack;

namespace
{
/// A building's turn as the land's buildings are turned from the game's angle
glm::mat3 TurnedBy(float yAngle)
{
	return glm::mat3(glm::eulerAngleY(-yAngle));
}
} // namespace

TEST(AppleBarrel, PitsAreMeasuredFromTheTempleOrAFixedPlace)
{
	EXPECT_EQ(apple_barrel::MeasuredFrom(glm::vec2(1800.0f, 2600.0f)), glm::vec2(1800.0f, 2600.0f));
	EXPECT_EQ(apple_barrel::MeasuredFrom(std::nullopt), glm::vec2(256.0f, 256.0f));
}

TEST(AppleBarrel, TheNearestPitCarriesTheBarrel)
{
	const std::array<glm::vec2, 3> stores {glm::vec2(100.0f, 0.0f), glm::vec2(0.0f, 30.0f), glm::vec2(-40.0f, 0.0f)};
	EXPECT_EQ(apple_barrel::NearestStore(stores, glm::vec2(0.0f)), 1U);
	EXPECT_EQ(apple_barrel::NearestStore(stores, glm::vec2(90.0f, 0.0f)), 0U);
}

TEST(AppleBarrel, TheFirstOfEquallyNearPitsCarriesIt)
{
	const std::array<glm::vec2, 2> stores {glm::vec2(10.0f, 0.0f), glm::vec2(-10.0f, 0.0f)};
	EXPECT_EQ(apple_barrel::NearestStore(stores, glm::vec2(0.0f)), 0U);
}

TEST(AppleBarrel, NoPitOrNoneNearEnoughCarriesNothing)
{
	EXPECT_FALSE(apple_barrel::NearestStore({}, glm::vec2(0.0f)).has_value());
	const std::array<glm::vec2, 1> far {glm::vec2(apple_barrel::k_FarthestStore, 0.0f)};
	EXPECT_FALSE(apple_barrel::NearestStore(far, glm::vec2(0.0f)).has_value());
	const std::array<glm::vec2, 1> justNear {glm::vec2(apple_barrel::k_FarthestStore - 1.0f, 0.0f)};
	EXPECT_EQ(apple_barrel::NearestStore(justNear, glm::vec2(0.0f)), 0U);
}

TEST(AppleBarrel, TheBarrelStandsAtThePitsEdgeTheWayItFaces)
{
	const glm::vec2 store {1826.79f, 2641.36f};
	// Unturned, out along x
	const auto unturned = apple_barrel::BarrelSpot(store, TurnedBy(0.0f), 9.0f);
	EXPECT_NEAR(unturned.x, store.x + 9.0f, 1e-3f);
	EXPECT_NEAR(unturned.y, store.y, 1e-3f);
	// The game's angle takes x towards z: cos and sin of the angle
	const float angle = 1.2f;
	const auto turned = apple_barrel::BarrelSpot(store, TurnedBy(angle), 9.0f);
	EXPECT_NEAR(turned.x, store.x + (std::cos(angle) * 9.0f), 1e-3f);
	EXPECT_NEAR(turned.y, store.y + (std::sin(angle) * 9.0f), 1e-3f);
	const auto quarter = apple_barrel::BarrelSpot(store, TurnedBy(std::numbers::pi_v<float> / 2.0f), 4.0f);
	EXPECT_NEAR(quarter.x, store.x, 1e-3f);
	EXPECT_NEAR(quarter.y, store.y + 4.0f, 1e-3f);
}

TEST(AppleBarrel, APitWithNoReachLeavesTheBarrelAtItsMiddle)
{
	const glm::vec2 store {5.0f, 6.0f};
	EXPECT_EQ(apple_barrel::BarrelSpot(store, TurnedBy(0.7f), 0.0f), store);
}

TEST(AppleBarrel, TheWormSitsOnTheBarrelsPoint)
{
	EXPECT_EQ(apple_barrel::WormSpot(glm::vec3(10.0f, 20.0f, 30.0f), glm::vec3(0.1f, 1.5f, -0.2f)),
	          glm::vec3(10.1f, 21.5f, 29.8f));
}

TEST(AppleBarrel, TheWormLoopsRoundItsClip)
{
	EXPECT_EQ(apple_barrel::StepWorm(0, 16, 1000, true), 16U);
	EXPECT_EQ(apple_barrel::StepWorm(990, 16, 1000, true), 6U);
	EXPECT_EQ(apple_barrel::StepWorm(999, 1, 1000, true), 0U);
	EXPECT_EQ(apple_barrel::StepWorm(500, 0, 1000, true), 500U);
}

TEST(AppleBarrel, AClipThatDoesNotLoopStopsShortOfItsEnd)
{
	EXPECT_EQ(apple_barrel::StepWorm(990, 16, 1000, false), 999U);
	EXPECT_EQ(apple_barrel::StepWorm(100, 16, 1000, false), 116U);
}

TEST(AppleBarrel, AClipWithNoPlayTimeStaysAtItsStart)
{
	EXPECT_EQ(apple_barrel::StepWorm(10, 16, 0, true), 0U);
	EXPECT_EQ(apple_barrel::StepWorm(10, 16, 0, false), 0U);
}
