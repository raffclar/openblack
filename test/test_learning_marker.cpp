/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/LearningMarker.h"
#include "ECS/LearningMarker.h"
#include "ECS/Registry.h"

using namespace openblack;
namespace marker = openblack::ecs::learning_marker;

TEST(LearningMarker, TheShareIsShownInWholeHundredths)
{
	EXPECT_EQ(marker::Text(0.5f), "50%");
	EXPECT_EQ(marker::Text(1.0f), "100%");
	EXPECT_EQ(marker::Text(1.0f / 3.0f), "33%");
	// A half is rounded up
	EXPECT_EQ(marker::Text(0.125f), "13%");
}

TEST(LearningMarker, ItStartsOverTheHeadSizedByTheCreature)
{
	const auto mark = marker::Start({10.0f, 20.0f, 30.0f}, 30.0f, 0.5f);
	EXPECT_EQ(mark.position, glm::vec3(10.0f, 21.0f, 30.0f));
	EXPECT_FLOAT_EQ(mark.size, 3.0f);
	EXPECT_EQ(mark.text, "50%");
}

TEST(LearningMarker, ItGrowsByTheGamesFloatSteps)
{
	// The game multiplies by the float reciprocal of the life, and does not divide
	auto mark = marker::Start({0.0f, 0.0f, 0.0f}, 10.0f, 1.0f);
	ASSERT_EQ(mark.size, 1.0f);
	EXPECT_TRUE(marker::Step(mark, 291.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(marker::Grown(mark)), 0x3F86353Fu);
	EXPECT_TRUE(marker::Step(mark, 109.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(marker::Grown(mark)), 0x3F888888u);
}

TEST(LearningMarker, ItRisesGrowsAndGoesAfterSixSeconds)
{
	auto mark = marker::Start({0.0f, 0.0f, 0.0f}, 20.0f, 1.0f);
	EXPECT_FLOAT_EQ(marker::Grown(mark), 2.0f);
	EXPECT_TRUE(marker::Step(mark, 1000.0f));
	// Three thousandths of its size a millisecond
	EXPECT_FLOAT_EQ(mark.position.y, 1.0f + 6.0f);
	EXPECT_TRUE(marker::Step(mark, 2000.0f));
	// Half way through its life it is half as big again
	EXPECT_FLOAT_EQ(marker::Grown(mark), 3.0f);
	EXPECT_TRUE(marker::Step(mark, 2999.0f));
	// It goes once it is six seconds old, before it moves again
	const float before = mark.position.y;
	EXPECT_FALSE(marker::Step(mark, 1.0f));
	EXPECT_FLOAT_EQ(mark.position.y, before);
}

TEST(LearningMarker, TheTextFadesAfterTwoAndAHalfSeconds)
{
	EXPECT_EQ(marker::TextAlpha(0.0f), 255);
	EXPECT_EQ(marker::TextAlpha(2499.0f), 255);
	EXPECT_EQ(marker::TextAlpha(2500.0f), 255);
	EXPECT_EQ(marker::TextAlpha(4250.0f), 127);
	EXPECT_EQ(marker::TextAlpha(5999.0f), 0);
}

TEST(LearningMarker, TheBulbBlinksAgainstTheGlow)
{
	// White, with no glow, each half second
	EXPECT_EQ(marker::BulbWhite(0.0f), 255);
	EXPECT_EQ(marker::GlowAlpha(0.0f), 0);
	EXPECT_EQ(marker::BulbWhite(500.0f), 255);
	// Yellow between, with the glow at half
	EXPECT_EQ(marker::BulbWhite(250.0f), 0);
	EXPECT_EQ(marker::GlowAlpha(250.0f), 127);
}

TEST(LearningMarker, TheBlinkUsesTheGameFloatAndRoundsLikeTheGame)
{
	// The float that the game holds for one turn a second
	EXPECT_EQ(std::bit_cast<uint32_t>(marker::k_BlinkPerMillisecond), 0x3BCDE32Eu);
	// At one second the angle is the float nearest to two pi. The cosine is a little under 1, and the multiply by 255
	// rounds it to exactly 255
	EXPECT_EQ(marker::BulbWhite(1000.0f), 255);
	EXPECT_EQ(marker::BulbWhite(100.0f), 206);
	EXPECT_EQ(marker::BulbWhite(333.0f), 127);
	EXPECT_EQ(marker::BulbWhite(1234.0f), 25);
}

TEST(LearningMarker, ItsLookPlacesTheBulbUnderTheGlowAndTheTextOverIt)
{
	auto mark = marker::Start({0.0f, 99.0f, 0.0f}, 30.0f, 0.5f);
	const auto look = marker::LookOf(mark);
	EXPECT_FLOAT_EQ(look.glowHalfSize, 3.0f);
	EXPECT_FLOAT_EQ(look.bulb.y, 100.0f - (3.0f * marker::k_BulbUnder));
	EXPECT_FLOAT_EQ(look.bulbScale, 1.0f);
	EXPECT_FLOAT_EQ(look.textPoint.y, 104.5f);
	EXPECT_EQ(look.glowColour, glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));
	EXPECT_EQ(look.bulbColour, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	EXPECT_FLOAT_EQ(look.textAlpha, 1.0f);
}

TEST(LearningMarker, MarksWhoseTimeIsUpGoAtTheTurn)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	registry.Assign<ecs::components::LearningMarker>(first, marker::Start({}, 15.0f, 0.5f, 40));
	const auto second = registry.Create();
	auto old = marker::Start({}, 15.0f, 0.5f, 0);
	old.milliseconds = 5990.0f;
	registry.Assign<ecs::components::LearningMarker>(second, old);
	// The frames step them; one whose time is up stands still and stays until the turn
	marker::StepAll(registry, 16.0f);
	EXPECT_TRUE(registry.Valid(second));
	EXPECT_FLOAT_EQ(registry.Get<const ecs::components::LearningMarker>(second).milliseconds, 6006.0f);
	marker::StepAll(registry, 16.0f);
	EXPECT_FLOAT_EQ(registry.Get<const ecs::components::LearningMarker>(second).milliseconds, 6006.0f);
	// 60 turns of 100 ms after it was made, a mark goes
	marker::RemoveDone(registry, 59);
	EXPECT_TRUE(registry.Valid(second));
	marker::RemoveDone(registry, 60);
	EXPECT_FALSE(registry.Valid(second));
	EXPECT_TRUE(registry.Valid(first));
	marker::RemoveDone(registry, 100);
	EXPECT_FALSE(registry.Valid(first));
}
