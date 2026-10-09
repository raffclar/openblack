/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/VillagerScriptRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace rules = openblack::ecs::villager_script_rules;

TEST(VillagerScriptRules, DistanceIsAlongTheGround)
{
	// The game's own hypotenuse, a close approximation
	EXPECT_NEAR(rules::ScriptDistance({0.0f, 0.0f, 0.0f}, {3.0f, 50.0f, 4.0f}), 5.0f, 0.01f);
}

TEST(VillagerScriptRules, UnderHalfAMetreIsNoDistance)
{
	EXPECT_EQ(rules::ScriptDistance({10.0f, 2.0f, 10.0f}, {10.4f, 0.0f, 10.0f}), 0.0f);
	EXPECT_EQ(rules::ScriptDistance({10.0f, 2.0f, 10.0f}, {10.0f, 0.0f, 10.0f}), 0.0f);
	EXPECT_NEAR(rules::ScriptDistance({10.0f, 0.0f, 10.0f}, {10.5f, 0.0f, 10.0f}), 0.5f, 0.01f);
}

TEST(VillagerScriptRules, ScriptSpeedIsKeptInWholeMapUnits)
{
	// A fifth of a metre a turn is 1310 map units a turn, two metres a second
	EXPECT_NEAR(rules::ScriptSpeedToWalkSpeed(0.2f), 1310.0f / 655.36f, 1e-4f);
	EXPECT_EQ(rules::ScriptSpeedToWalkSpeed(-1.0f), 0.0f);
	// No faster than the walk can hold
	EXPECT_NEAR(rules::ScriptSpeedToWalkSpeed(100.0f), 65535.0f / 655.36f, 1e-3f);
	EXPECT_NEAR(rules::WalkSpeedToScriptSpeed(rules::ScriptSpeedToWalkSpeed(0.5f)), 0.5f, 1e-3f);
}

TEST(VillagerScriptRules, ClipPlaysCountDownThenWait)
{
	auto step = rules::StepScriptClip(2);
	EXPECT_EQ(step.playsLeft, 1u);
	EXPECT_EQ(step.then, VillagerStates::ScriptPlayAnim);
	step = rules::StepScriptClip(step.playsLeft);
	EXPECT_EQ(step.playsLeft, 0u);
	EXPECT_EQ(step.then, VillagerStates::InScript);
	step = rules::StepScriptClip(0);
	EXPECT_EQ(step.playsLeft, 0u);
	EXPECT_FALSE(step.then.has_value());
}

TEST(VillagerScriptRules, EndlessPlaysNeverRunOut)
{
	const auto step = rules::StepScriptClip(0xffffffffu);
	EXPECT_EQ(step.then, VillagerStates::ScriptPlayAnim);
	EXPECT_EQ(step.playsLeft, 0xfffffffeu);
}

TEST(VillagerScriptRules, PlayedOnceTheLastPlayEnds)
{
	EXPECT_FALSE(rules::HasPlayedScriptClip(VillagerStates::WaitForAnimation, 0));
	EXPECT_FALSE(rules::HasPlayedScriptClip(VillagerStates::ScriptPlayAnim, 1));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::ScriptPlayAnim, 0));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::InScript, 3));
	EXPECT_TRUE(rules::HasPlayedScriptClip(VillagerStates::MoveToPos, 3));
}

TEST(VillagerScriptRules, ScriptLetsGoOnlyForItsOwnStates)
{
	constexpr rules::StateRules k_Plain {.isScriptState = false, .isScriptInterruptable = false};
	EXPECT_FALSE(rules::ScriptLetsGo(VillagerStates::InScript, VillagerStates::DecideWhatToDo, k_Plain));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::InScript, VillagerStates::InHand, k_Plain));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::InScript, VillagerStates::DecideWhatToDo,
	                                {.isScriptState = true, .isScriptInterruptable = false}));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::InScript, VillagerStates::DecideWhatToDo,
	                                {.isScriptState = false, .isScriptInterruptable = true}));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::InScript, VillagerStates::MoveAlongPath, k_Plain));
	EXPECT_TRUE(rules::ScriptLetsGo(VillagerStates::ScriptPlayAnim, VillagerStates::ScriptPlayAnim, k_Plain));
	EXPECT_FALSE(rules::ScriptLetsGo(VillagerStates::ScriptPlayAnim, VillagerStates::InScript, k_Plain));
}
