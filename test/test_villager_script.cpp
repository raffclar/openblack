/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/HighDetailRules.h>
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

TEST(VillagerScriptRules, WithinAStepOfTheGoalIsThere)
{
	// Two metres a second is a fifth of a metre a turn
	const auto walkSpeed = rules::ScriptSpeedToWalkSpeed(0.2f);
	EXPECT_TRUE(rules::WithinAStepOfGoal({100.0f, 100.0f}, {100.0f, 100.0f}, walkSpeed));
	EXPECT_TRUE(rules::WithinAStepOfGoal({100.15f, 100.0f}, {100.0f, 100.0f}, walkSpeed));
	EXPECT_TRUE(rules::WithinAStepOfGoal({100.1f, 100.1f}, {100.0f, 100.0f}, walkSpeed));
	// A whole step away is not there yet
	EXPECT_FALSE(rules::WithinAStepOfGoal({100.25f, 100.0f}, {100.0f, 100.0f}, walkSpeed));
	EXPECT_FALSE(rules::WithinAStepOfGoal({100.0f, 99.7f}, {100.0f, 100.0f}, walkSpeed));
	// Standing still, it is only there on the goal itself
	EXPECT_FALSE(rules::WithinAStepOfGoal({100.0f, 100.01f}, {100.0f, 100.0f}, 0.0f));
}

TEST(VillagerScriptRules, ScriptsSeeAWalkerAtItsGoalOnceAStepAway)
{
	const auto walkSpeed = rules::ScriptSpeedToWalkSpeed(0.4f);
	const glm::vec2 goal {1880.0f, 2520.0f};
	// Short of the goal by less than a step, the script sees it on the goal: within half a metre of it
	const auto seen = rules::ScriptSeenPlace({1880.3f, 2520.0f}, goal, walkSpeed);
	EXPECT_EQ(seen, goal);
	EXPECT_EQ(rules::ScriptDistance({seen.x, 0.0f, seen.y}, {goal.x, 0.0f, goal.y}), 0.0f);
	// Further off it is seen where it is
	EXPECT_EQ(rules::ScriptSeenPlace({1881.0f, 2520.0f}, goal, walkSpeed), glm::vec2(1881.0f, 2520.0f));
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

TEST(VillagerScriptRules, OnlyTheWalkingStatesCarryOnWithAWalk)
{
	EXPECT_TRUE(rules::KeepsWalking(VillagerStates::MoveToPos));
	EXPECT_TRUE(rules::KeepsWalking(VillagerStates::MoveToObject));
	EXPECT_TRUE(rules::KeepsWalking(VillagerStates::MoveOnStructure));
	// The opening's boy is set swimming, put in the script's hands and set playing clips: each stops his walk
	EXPECT_FALSE(rules::KeepsWalking(VillagerStates::InScript));
	EXPECT_FALSE(rules::KeepsWalking(VillagerStates::ScriptPlayAnim));
	EXPECT_FALSE(rules::KeepsWalking(VillagerStates::WaitForAnimation));
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

TEST(HighDetail, OnlyTheOpeningsFamilyAndTheTrainerHaveDetailedModels)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const auto man = hd::DetailedModelFor(openblack::MeshId::PersonNorseMaleA1);
	ASSERT_TRUE(man.has_value());
	EXPECT_EQ(man->file, "Intro/nors_man.l3d");
	EXPECT_EQ(man->face, hd::Face::Man);
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonNorseFemaleA1)->face, hd::Face::Woman);
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonBoyWhite1)->file, "Intro/nors_boy.l3d");
	// The trainer wears the woman's face
	EXPECT_EQ(hd::DetailedModelFor(openblack::MeshId::PersonAnimalTrainer)->face, hd::Face::Woman);
	EXPECT_FALSE(hd::DetailedModelFor(openblack::MeshId::PersonNorseMaleA2).has_value());
}

TEST(HighDetail, TheOpeningsOrdersTurnTheBoyAndEaseHisTurning)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const hd::DrawOrders none;
	// Easing switched off, then on again
	auto result = hd::ApplyThingSpecial(hd::ThingSpecial::EaseTurning, false, none, 1.0f);
	EXPECT_TRUE(result.orders.turnAtOnce);
	EXPECT_FALSE(result.yAngle.has_value());
	result = hd::ApplyThingSpecial(hd::ThingSpecial::EaseTurning, true, result.orders, 1.0f);
	EXPECT_FALSE(result.orders.turnAtOnce);
	// Mirrored, he faces the other way round and turns at once
	result = hd::ApplyThingSpecial(hd::ThingSpecial::FaceMirrored, true, none, 1.0f);
	ASSERT_TRUE(result.yAngle.has_value());
	EXPECT_FLOAT_EQ(*result.yAngle, -1.0f);
	EXPECT_TRUE(result.orders.turnAtOnce);
	// Following the hand doesn't look at on
	result = hd::ApplyThingSpecial(hd::ThingSpecial::FollowIntroHand, false, none, 1.0f);
	EXPECT_TRUE(result.orders.followIntroHand);
	result = hd::ApplyThingSpecial(hd::ThingSpecial::TakeQuarterTurn, true, none, 1.0f);
	EXPECT_FLOAT_EQ(*result.yAngle, 1.0f - hd::k_QuarterTurn);
}

TEST(HighDetail, KeptOnlyWhileAScriptHoldsTheBars)
{
	namespace hd = openblack::ecs::high_detail_rules;
	EXPECT_TRUE(hd::KeepsHighDetail(true, 12));
	EXPECT_FALSE(hd::KeepsHighDetail(true, 0));
	EXPECT_FALSE(hd::KeepsHighDetail(false, 12));
}

TEST(HighDetail, DrawnAndItsEyesPlacedInTheHandsGripWhileHeld)
{
	namespace hd = openblack::ecs::high_detail_rules;
	const glm::vec3 standsAt {1494.895f, 0.0f, 2092.996f};
	const glm::vec3 grip {1416.0f, 1.2f, 2053.0f};
	EXPECT_EQ(hd::DrawnAt(std::nullopt, standsAt), standsAt);
	EXPECT_EQ(hd::DrawnAt(grip, standsAt), grip);
}
