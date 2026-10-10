/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/IntroRules.h"

using namespace openblack::ecs;
namespace light = openblack::ecs::intro_rules::light;

namespace
{
/// Draws are the middle of their range, so that a test can tell the sizes the flicker gives
float Middle(float a, float b)
{
	return (a + b) * 0.5f;
}

light::Light FallingLight()
{
	return light::Make(intro_rules::k_BoySpot, intro_rules::k_LightWay, Middle);
}
} // namespace

TEST(IntroRules, TheSettingDownHandFacesAnEighthAndFifteenDegreesRound)
{
	EXPECT_NEAR(intro_rules::PutDownYaw(), -1.0471976f, 1e-6f);
}

TEST(IntroRules, AClipPlayedOnceHoldsItsEndAndALoopingOneComesRound)
{
	bool cameRound = false;
	// The lifting clip, 766 ms played once, holds its last millisecond
	EXPECT_EQ(intro_rules::AdvanceClip(700, 100, 766, false, cameRound), 765);
	EXPECT_FALSE(cameRound);
	EXPECT_EQ(intro_rules::AdvanceClip(765, 100, 766, false, cameRound), 765);
	EXPECT_FALSE(cameRound);
	// The setting-down clip, 3066 ms looping, comes round at its end
	EXPECT_EQ(intro_rules::AdvanceClip(3000, 100, 3066, true, cameRound), 34);
	EXPECT_TRUE(cameRound);
	EXPECT_EQ(intro_rules::AdvanceClip(2379, 100, 3066, true, cameRound), 2479);
	EXPECT_FALSE(cameRound);
}

TEST(IntroRules, TheLightStartsFourKilometresUpItsWayAndAimsAtTheBoy)
{
	const auto made = FallingLight();
	EXPECT_NEAR(made.way.x, -0.69714f, 1e-4f);
	EXPECT_NEAR(made.way.y, -0.16731f, 1e-4f);
	EXPECT_NEAR(made.way.z, -0.69714f, 1e-4f);
	EXPECT_NEAR(made.start.x, 4204.75f, 0.01f);
	EXPECT_NEAR(made.start.y, 669.3f, 0.1f);
	EXPECT_NEAR(made.start.z, 4841.92f, 0.01f);
	EXPECT_EQ(made.state, light::State::Falling);
}

TEST(IntroRules, TheLightsSpritesAreRoundInFrontLargerBehindWithAGlowAndAStreak)
{
	const auto made = FallingLight();
	for (size_t i = 0; i < 6; ++i)
	{
		EXPECT_EQ(made.sprites.at(i).cell, 48) << i;
	}
	for (size_t i = 6; i < 19; ++i)
	{
		EXPECT_EQ(made.sprites.at(i).cell, 50) << i;
	}
	EXPECT_EQ(made.sprites.at(19).cell, 49);
	// Sizes fall off towards the back; the trailing ones twice as large
	EXPECT_FLOAT_EQ(made.sprites.at(0).halfWidth, 17.25f);
	EXPECT_FLOAT_EQ(made.sprites.at(5).halfWidth, 13.5f);
	EXPECT_FLOAT_EQ(made.sprites.at(6).halfWidth, 25.5f);
	EXPECT_FLOAT_EQ(made.sprites.at(17).halfWidth, 9.0f);
	// Fainter towards the back
	EXPECT_EQ(made.sprites.at(0).argb, 0xF2FFFFFFu);
	EXPECT_EQ(made.sprites.at(17).argb, 0x19FFFFFFu);
	// The glow and the thin streak
	EXPECT_EQ(made.sprites.at(18).argb, 0x28FFFFFFu);
	EXPECT_FLOAT_EQ(made.sprites.at(18).halfWidth, 80.0f);
	EXPECT_EQ(made.sprites.at(19).argb, 0x0EFFFFFFu);
	EXPECT_FLOAT_EQ(made.sprites.at(19).halfWidth, 350.0f);
	EXPECT_FLOAT_EQ(made.sprites.at(19).halfWidth * made.sprites.at(19).heightFactor, 17.5f);
	EXPECT_NEAR(made.sprites.at(19).angle, -0.3926991f, 1e-6f);
}

TEST(IntroRules, TheLightFallsSteadilyAndArrivesTenMetresShort)
{
	auto falling = FallingLight();
	falling.elapsed = 1000;
	const auto second = light::HeadPosition(falling);
	// The engine's inverse square root leaves the way a hair over unit length
	EXPECT_NEAR(glm::length(second - falling.start), 450.0f, 0.05f);
	EXPECT_FALSE(falling.arrived);
	falling.elapsed = 8866;
	static_cast<void>(light::HeadPosition(falling));
	EXPECT_FALSE(falling.arrived);
	falling.elapsed = 8867;
	const auto landed = light::HeadPosition(falling);
	EXPECT_TRUE(falling.arrived);
	EXPECT_NEAR(landed.x, 1423.31f, 0.01f);
	EXPECT_NEAR(landed.y, 1.67f, 0.01f);
	EXPECT_NEAR(landed.z, 2060.47f, 0.01f);
}

TEST(IntroRules, TheChasingCameraIsBehindAndAboveTheLight)
{
	auto falling = FallingLight();
	light::Frame frame;
	light::Step(falling, 100, Middle, frame);
	ASSERT_TRUE(frame.cameraAt.has_value());
	const auto head = light::HeadPosition(falling);
	EXPECT_NEAR(frame.cameraAt->x, head.x - falling.way.x * 150.0f, 0.01f);
	EXPECT_NEAR(frame.cameraAt->y, head.y - falling.way.y * 150.0f + 30.0f, 0.01f);
	EXPECT_NEAR(frame.cameraAt->z, head.z - falling.way.z * 150.0f, 0.01f);
	EXPECT_EQ(frame.drawn.size(), light::k_Sprites);
	EXPECT_FALSE(frame.throughEverything);
}

TEST(IntroRules, NearItsEndTheLightShowsThroughEverything)
{
	auto falling = FallingLight();
	falling.elapsed = 8237;
	light::Frame frame;
	light::Step(falling, 10, Middle, frame);
	EXPECT_FALSE(frame.throughEverything);
	light::Step(falling, 10, Middle, frame);
	EXPECT_TRUE(frame.throughEverything);
}

TEST(IntroRules, LandedTheLightFlashesHoldsHalfASecondAndFadesForOne)
{
	auto falling = FallingLight();
	falling.elapsed = 9000;
	light::Frame frame;
	light::Step(falling, 100, Middle, frame);
	EXPECT_EQ(falling.state, light::State::Holding);
	// The flash: its front sprite three times, through everything
	light::Step(falling, 100, Middle, frame);
	EXPECT_EQ(frame.drawn.size(), 3u);
	EXPECT_TRUE(frame.throughEverything);
	EXPECT_EQ(frame.drawn.front().cell, 48);
	for (int i = 0; i < 4; ++i)
	{
		light::Step(falling, 100, Middle, frame);
	}
	EXPECT_EQ(falling.state, light::State::Holding);
	light::Step(falling, 100, Middle, frame);
	EXPECT_EQ(falling.state, light::State::Fading);
	// Fully bright at the start of its fade, down to 50 at its end
	EXPECT_EQ(frame.drawn.front().argb >> 24u, 255u);
	for (int i = 0; i < 10; ++i)
	{
		light::Step(falling, 100, Middle, frame);
	}
	EXPECT_EQ(frame.drawn.front().argb >> 24u, 50u);
	EXPECT_EQ(falling.state, light::State::Gone);
	light::Step(falling, 100, Middle, frame);
	EXPECT_TRUE(frame.drawn.empty());
}

TEST(IntroRules, WhileTheGameStandsStillTheTrailKeepsItsTurns)
{
	auto falling = FallingLight();
	light::Frame frame;
	int draws = 0;
	const light::Random counting = [&draws](float a, float b) {
		++draws;
		return Middle(a, b);
	};
	light::Step(falling, 0, counting, frame);
	EXPECT_EQ(draws, 0);
	// 18 new turns and 5 flickers
	light::Step(falling, 16, counting, frame);
	EXPECT_EQ(draws, 23);
}

TEST(IntroRules, TheHandHoldsTheBoyBelowItsGripUntilItLetsGo)
{
	// A bone moved up a metre, the grip point a metre along it, the hand at a place
	const std::array<glm::mat4, 2> bones {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 20.0f));
	const auto held = intro_rules::GripPoint(1817, bones, 1, glm::vec3(1.0f, 0.0f, 0.0f), model);
	ASSERT_TRUE(held.has_value());
	EXPECT_NEAR(held->x, 11.0f, 1e-5f);
	EXPECT_NEAR(held->y, 1.0f - 0.65f, 1e-5f);
	EXPECT_NEAR(held->z, 20.0f, 1e-5f);
	EXPECT_FALSE(intro_rules::GripPoint(1818, bones, 1, glm::vec3(0.0f), model).has_value());
	EXPECT_FALSE(intro_rules::GripPoint(0, bones, 2, glm::vec3(0.0f), model).has_value());
}
