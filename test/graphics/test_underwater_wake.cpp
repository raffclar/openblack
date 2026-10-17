/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the wakes, rings and splashes on the experiment's water

#include <cmath>
#include <cstdint>

#include <array>
#include <numbers>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/UnderwaterWake.h"

namespace wake = openblack::graphics::underwater::wake;

TEST(UnderwaterWake, StillOrDryThingsWriteNothing)
{
	const auto still = wake::WriteFor({.position = {0.0f, 0.0f}, .velocity = {0.0f, 0.0f}, .radius = 2.0f, .submerged = 1.0f});
	EXPECT_FLOAT_EQ(still.depth, 0.0f);
	EXPECT_FLOAT_EQ(still.foam, 0.0f);
	const auto dry = wake::WriteFor({.velocity = {3.0f, 0.0f}, .radius = 2.0f, .submerged = 0.0f});
	EXPECT_FLOAT_EQ(dry.depth, 0.0f);
	EXPECT_FLOAT_EQ(dry.foam, 0.0f);
}

TEST(UnderwaterWake, FasterBiggerAndDeeperThingsWriteMore)
{
	const wake::Mover slow {.velocity = {1.0f, 0.0f}, .radius = 1.0f, .submerged = 1.0f};
	auto fast = slow;
	fast.velocity = {3.0f, 0.0f};
	auto big = slow;
	big.radius = 2.0f;
	big.submerged = 2.0f;
	EXPECT_GT(wake::WriteFor(fast).depth, wake::WriteFor(slow).depth);
	EXPECT_GT(wake::WriteFor(fast).foam, wake::WriteFor(slow).foam);
	EXPECT_GT(wake::WriteFor(big).depth, wake::WriteFor(slow).depth);
	// Faster than the full speed writes no more
	auto faster = slow;
	faster.velocity = {wake::k_FullWakeSpeed * 2.0f, 0.0f};
	auto full = slow;
	full.velocity = {wake::k_FullWakeSpeed, 0.0f};
	EXPECT_FLOAT_EQ(wake::WriteFor(faster).depth, wake::WriteFor(full).depth);
	// A huge thing pushes no deeper than the most
	const wake::Mover huge {.velocity = {9.0f, 0.0f}, .radius = 40.0f, .submerged = 40.0f};
	EXPECT_LE(wake::WriteFor(huge).depth, wake::k_MostDepth);
	// A thing that bobs up and down makes rings
	const wake::Mover bobbing {.verticalSpeed = 1.5f, .radius = 1.0f, .submerged = 0.5f};
	EXPECT_GT(wake::WriteFor(bobbing).depth, 0.0f);
}

TEST(UnderwaterWake, SplashesGrowWithSpeedSizeAndMass)
{
	EXPECT_FALSE(wake::SplashFor({.downSpeed = wake::k_LeastSplashSpeed * 0.5f, .radius = 1.0f, .mass = 50.0f}).has_value());
	EXPECT_FALSE(wake::SplashFor({.downSpeed = 10.0f, .radius = 0.0f, .mass = 50.0f}).has_value());
	const auto slow = wake::SplashFor({.downSpeed = 4.0f, .radius = 0.5f, .mass = 20.0f});
	const auto fast = wake::SplashFor({.downSpeed = 12.0f, .radius = 0.5f, .mass = 20.0f});
	const auto heavy = wake::SplashFor({.downSpeed = 4.0f, .radius = 0.5f, .mass = 500.0f});
	const auto wide = wake::SplashFor({.downSpeed = 4.0f, .radius = 3.0f, .mass = 20.0f});
	ASSERT_TRUE(slow.has_value() && fast.has_value() && heavy.has_value() && wide.has_value());
	EXPECT_GT(fast->crownHeight, slow->crownHeight);
	EXPECT_GT(fast->drops, slow->drops);
	EXPECT_GT(fast->dropSpeed, slow->dropSpeed);
	EXPECT_GT(fast->seconds, slow->seconds);
	EXPECT_GT(heavy->crownHeight, slow->crownHeight);
	EXPECT_GT(heavy->foam, slow->foam);
	EXPECT_GT(wide->crownRadius, slow->crownRadius);
	// A thing that skims throws more spray than one that drops at the same speed down
	const auto skims = wake::SplashFor({.downSpeed = 4.0f, .acrossSpeed = 12.0f, .radius = 0.5f, .mass = 20.0f});
	ASSERT_TRUE(skims.has_value());
	EXPECT_GT(skims->drops, slow->drops);
	// A huge fall has its caps
	const auto huge = wake::SplashFor({.downSpeed = 200.0f, .radius = 50.0f, .mass = 1e6f});
	ASSERT_TRUE(huge.has_value());
	EXPECT_EQ(huge->drops, wake::k_MostDrops);
	EXPECT_FLOAT_EQ(huge->crownHeight, wake::k_HighestCrown);
	EXPECT_FLOAT_EQ(huge->ringDepth, wake::k_DeepestRing);
	// A slow fall still makes rings
	EXPECT_GT(wake::RingDepthFor({.downSpeed = 1.0f, .radius = 1.0f, .mass = 20.0f}), 0.0f);
	EXPECT_FLOAT_EQ(wake::RingDepthFor({.downSpeed = 0.0f, .radius = 1.0f, .mass = 20.0f}), 0.0f);
}

TEST(UnderwaterWake, SplashSizeFollowsTheFourthRootOfTheEnergy)
{
	// From the same height (10 units, about 14 units a second): a pebble, a villager and a boulder
	constexpr float k_Speed = 14.0f;
	const auto pebble = wake::SplashFor({.downSpeed = k_Speed, .radius = 0.05f, .mass = 0.1f});
	const auto villager = wake::SplashFor({.downSpeed = k_Speed, .radius = 0.3f, .mass = 70.0f});
	const auto boulder = wake::SplashFor({.downSpeed = k_Speed, .radius = 1.5f, .mass = 3000.0f});
	ASSERT_TRUE(pebble.has_value() && villager.has_value() && boulder.has_value());
	EXPECT_LT(pebble->crownHeight, 0.4f);
	EXPECT_GT(villager->crownHeight, 0.8f);
	EXPECT_LT(villager->crownHeight, 2.5f);
	EXPECT_GT(boulder->crownHeight, 2.5f);
	EXPECT_LT(pebble->drops, 10u);
	// Ten thousand times the energy makes a hole ten times as wide
	const wake::Impact small {.downSpeed = 10.0f, .radius = 1.0f, .mass = 1.0f};
	const wake::Impact large {.downSpeed = 10.0f, .radius = 1.0f, .mass = 10000.0f};
	EXPECT_NEAR(wake::CavityRadius(large) / wake::CavityRadius(small), 10.0f, 1e-3f);
	// A half of the mass times the speed squared, the speed in metres a second
	const float metresASecond = 10.0f / wake::k_UnitsPerMetre;
	EXPECT_NEAR(wake::ImpactEnergy(small), 0.5f * metresASecond * metresASecond, 1e-2f);
}

TEST(UnderwaterWake, KernelPullsEveryWaveBackAndLongWavesGoFaster)
{
	const auto kernel = wake::WaveKernel();
	EXPECT_NEAR(kernel.at(0), 1.0f, 0.15f);
	// No wave of the field grows by itself: the kernel pulls back every wave length, across and along a diagonal
	for (int i = 0; i <= 64; ++i)
	{
		const float k = std::numbers::pi_v<float> * static_cast<float>(i) / 64.0f;
		EXPECT_GE(wake::KernelResponse(kernel, k), 0.0f) << k;
	}
	// The waves of the field disperse as on deep water: the speed of a wave is its turn rate over its wave number, and it
	// is higher for a longer wave
	const float gravity = wake::GravityTerm(kernel, 1.0f / wake::k_StepsPerSecond, wake::k_FieldSpacing);
	EXPECT_GT(gravity, 0.0f);
	const auto speed = [&kernel, gravity](float lengthInTexels) {
		const float k = 2.0f * std::numbers::pi_v<float> / lengthInTexels;
		return std::sqrt(gravity * wake::KernelResponse(kernel, k)) / k;
	};
	EXPECT_GT(speed(32.0f), speed(16.0f));
	EXPECT_GT(speed(16.0f), speed(10.0f));
	// At the reference length the speed is the deep water speed of the experiment's waves
	const float length = wake::k_ReferenceLength * wake::k_FieldSpacing;
	const float deep = std::sqrt(wake::k_Gravity * length / (2.0f * std::numbers::pi_v<float>));
	const float field = speed(wake::k_ReferenceLength) * wake::k_FieldSpacing * wake::k_StepsPerSecond;
	EXPECT_NEAR(field, deep, deep * 0.01f);
	// Each step is stable: the pull back is less than the scheme can take
	for (int i = 0; i <= 64; ++i)
	{
		const float k = std::numbers::pi_v<float> * static_cast<float>(i) / 64.0f;
		EXPECT_LT(gravity * wake::KernelResponse(kernel, k), 2.0f);
	}
}

TEST(UnderwaterWake, FieldFollowsTheCameraByWholeTexels)
{
	EXPECT_EQ(wake::FieldCell({0.1f, -0.1f}), glm::ivec2(0, -1));
	EXPECT_EQ(wake::FieldCell({1.0f, 0.5f}), glm::ivec2(4, 2));
	const auto origin = wake::FieldOrigin({4, 2});
	EXPECT_FLOAT_EQ(origin.x, (4.0f - wake::k_FieldSide / 2.0f) * wake::k_FieldSpacing);
	EXPECT_FLOAT_EQ(origin.y, (2.0f - wake::k_FieldSide / 2.0f) * wake::k_FieldSpacing);
	// The centre is the point the camera looks at, up to the reach
	const auto near = wake::FieldCentre({0.0f, 10.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 40.0f);
	EXPECT_FLOAT_EQ(near.x, 10.0f);
	const auto far = wake::FieldCentre({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 400.0f}, 40.0f);
	EXPECT_FLOAT_EQ(far.y, 40.0f);
}

TEST(UnderwaterWake, FieldStepsSixtyTimesASecondAndSettles)
{
	wake::FieldClock clock;
	// Nothing written: no steps
	auto plan = clock.Plan(0.01f, {0, 0}, 0.0f, 0.0f);
	EXPECT_EQ(plan.steps, 0u);
	EXPECT_TRUE(clock.IsFlat());
	// The first write starts the field again from flat, with one step at once
	plan = clock.Plan(0.01f, {0, 0}, 0.3f, 0.5f);
	EXPECT_TRUE(plan.reset);
	EXPECT_EQ(plan.steps, 1u);
	EXPECT_FALSE(clock.IsFlat());
	// At 200 frames a second, 60 steps in a second
	uint32_t steps = 0;
	for (int i = 0; i < 200; ++i)
	{
		const auto next = clock.Plan(0.005f, {0, 0}, 0.3f, 0.5f);
		EXPECT_FALSE(next.reset);
		steps += next.steps;
	}
	EXPECT_NEAR(static_cast<float>(steps), wake::k_StepsPerSecond, 1.0f);
	// A long frame makes no more than the most steps
	EXPECT_EQ(clock.Plan(0.5f, {0, 0}, 0.3f, 0.5f).steps, wake::k_MostStepsPerFrame);
	// The centre moves: the shift is in texels, at the next step
	const auto still = clock.Plan(0.001f, {3, -2}, 0.3f, 0.5f);
	EXPECT_EQ(still.steps, 0u);
	EXPECT_EQ(still.shift, glm::ivec2(0));
	EXPECT_EQ(clock.Plan(0.02f, {3, -2}, 0.3f, 0.5f).shift, glm::ivec2(3, -2));
	// Nothing more written: the field steps until it settles, then stops
	const float settle = wake::SettleSeconds(0.3f, 0.5f);
	EXPECT_GT(settle, 1.0f);
	float quiet = 0.0f;
	while (!clock.IsFlat() && quiet < 100.0f)
	{
		static_cast<void>(clock.Plan(0.01f, {3, -2}, 0.0f, 0.0f));
		quiet += 0.01f;
	}
	EXPECT_TRUE(clock.IsFlat());
	EXPECT_NEAR(quiet, settle, 0.05f);
	EXPECT_EQ(clock.Plan(0.01f, {3, -2}, 0.0f, 0.0f).steps, 0u);
	EXPECT_FLOAT_EQ(wake::SettleSeconds(0.0f, 0.0f), 0.0f);
}

TEST(UnderwaterWake, HullsFloatAndBoatsArePointedAtTheBow)
{
	// A box 10 long along x, 4 wide, from 1 under the origin to 3 over it, turned a quarter turn about the up axis
	const auto model = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, 50.0f)),
	                               std::numbers::pi_v<float> * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto hull = wake::HullFor(model, {-5.0f, -1.0f, -2.0f}, {5.0f, 3.0f, 2.0f}, 0.0f, true);
	ASSERT_TRUE(hull.has_value());
	EXPECT_NEAR(hull->draft, 1.0f, 1e-4f);
	EXPECT_NEAR(hull->half.x, 5.0f, 1e-4f);
	EXPECT_NEAR(hull->half.y, 2.0f, 1e-4f);
	EXPECT_NEAR(std::abs(hull->forward.y), 1.0f, 1e-4f);
	EXPECT_TRUE(hull->boat);
	// Out of the water, or all under it: no hull
	EXPECT_FALSE(wake::HullFor(model, {-5.0f, -1.0f, -2.0f}, {5.0f, 3.0f, 2.0f}, -2.0f, true).has_value());
	EXPECT_FALSE(wake::HullFor(model, {-5.0f, -1.0f, -2.0f}, {5.0f, 3.0f, 2.0f}, 4.0f, true).has_value());

	const wake::Hull boat {.centre = {0.0f, 0.0f}, .forward = {1.0f, 0.0f}, .half = {5.0f, 2.0f}, .draft = 1.0f};
	EXPECT_LT(wake::HullDistance(boat, {0.0f, 0.0f}), 0.0f);
	EXPECT_NEAR(wake::HullDistance(boat, {0.0f, 2.0f}), 0.0f, 1e-4f);
	EXPECT_GT(wake::HullDistance(boat, {0.0f, 3.0f}), 0.0f);
	EXPECT_GT(wake::HullDistance(boat, {6.0f, 0.0f}), 0.0f);
	// Near the bow the hull is narrower than near the stern
	EXPECT_GT(wake::HullDistance(boat, {4.0f, 1.0f}), wake::HullDistance(boat, {-4.0f, 1.0f}));
}

TEST(UnderwaterWake, TheMaskFollowsTheWaterLine)
{
	// A hull that is 4 wide at its keel and 10 wide at its top, 6 high
	const std::array<glm::vec3, 6> points {{{-2.0f, 0.0f, -5.0f},
	                                        {2.0f, 0.0f, 5.0f},
	                                        {-3.5f, 3.0f, -6.0f},
	                                        {3.5f, 3.0f, 6.0f},
	                                        {-5.0f, 6.0f, -8.0f},
	                                        {5.0f, 6.0f, 8.0f}}};
	const auto line = wake::WaterlineOf(points, 3.0f, 0.5f);
	ASSERT_TRUE(line.has_value());
	EXPECT_FLOAT_EQ(line->low.x, -3.5f);
	EXPECT_FLOAT_EQ(line->high.x, 3.5f);
	EXPECT_FLOAT_EQ(line->high.y, 6.0f);
	EXPECT_FALSE(wake::WaterlineOf(points, 10.0f, 0.5f).has_value());
}
