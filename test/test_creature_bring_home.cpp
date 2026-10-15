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
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace bring = openblack::creature_bring_home;

namespace
{
uint32_t First(uint32_t /*range*/)
{
	return 0;
}

const plan_actions::Executor& ActionNamed(std::string_view name)
{
	const auto* action = plan_actions::For(name);
	EXPECT_NE(action, nullptr) << name;
	static const plan_actions::Executor k_None {};
	return action != nullptr ? *action : k_None;
}

void ExpectWalkHome(const Step& step, glm::vec2 home, float within)
{
	EXPECT_EQ(step.kind, Step::Kind::Move);
	EXPECT_EQ(step.movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(step.movement.point, home);
	EXPECT_FLOAT_EQ(step.movement.maxDistance, within);
	EXPECT_TRUE(step.gaze->whileGoing);
}

void ExpectPutDown(const Step& step)
{
	EXPECT_EQ(step.kind, Step::Kind::Object);
	EXPECT_EQ(step.order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(step.order.animation, creature_throw::k_PutDown);
}

void ExpectPickUp(const Step& step, uint32_t thing)
{
	EXPECT_EQ(step.kind, Step::Kind::Object);
	EXPECT_EQ(step.order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(step.order.object, thing);
	ASSERT_TRUE(step.gaze.has_value());
	EXPECT_EQ(step.gaze->object, thing);
	EXPECT_TRUE(step.gaze->whileGoing);
}

constexpr glm::vec2 k_Home {100.0f, 200.0f};
} // namespace

// The three actions of the game's table that bring things home act on the things that the table names
TEST(CreatureBringHome, TheActionsActOnTheirThings)
{
	using plan_actions::Build;
	using plan_actions::Target;
	EXPECT_EQ(ActionNamed("BringHome").target, Target::BringableHome);
	EXPECT_EQ(ActionNamed("BringHome").build, Build::BringHome);
	EXPECT_EQ(ActionNamed("BringFoodHome").target, Target::Food);
	EXPECT_EQ(ActionNamed("BringFoodHome").build, Build::BringFoodHome);
	EXPECT_EQ(ActionNamed("TakeFoodFromFieldToHome").target, Target::FieldWithFood);
	EXPECT_EQ(ActionNamed("TakeFoodFromFieldToHome").build, Build::TakeFieldFoodHome);
}

// Only a thing more than 50 m from home, not alive, that the creature can pick up
TEST(CreatureBringHome, OnlyFarLifelessThingsThatItCanPickUp)
{
	EXPECT_TRUE(bring::CanBeBroughtHome(50.5f, false, true));
	EXPECT_FALSE(bring::CanBeBroughtHome(50.0f, false, true));
	EXPECT_FALSE(bring::CanBeBroughtHome(80.0f, true, true));
	EXPECT_FALSE(bring::CanBeBroughtHome(80.0f, false, false));
}

// It stops within its height of home, but never further than 5 m
TEST(CreatureBringHome, StopsWithinFiveMetresOrItsHeight)
{
	EXPECT_FLOAT_EQ(bring::BringHomeWithin(3.0f), 3.0f);
	EXPECT_FLOAT_EQ(bring::BringHomeWithin(12.0f), 5.0f);
}

TEST(CreatureBringHome, BringHomePicksUpWalksHomeAndPutsDown)
{
	const auto agenda = bring::BringHome(7u, 7u, false, k_Home, 12.0f);
	ASSERT_EQ(agenda.size(), 3u);
	ExpectPickUp(agenda[0], 7u);
	ExpectWalkHome(agenda[1], k_Home, 5.0f);
	ExpectPutDown(agenda[2]);

	// With a full hand it brings home what it holds
	const auto holding = bring::BringHome(std::nullopt, 7u, false, k_Home, 12.0f);
	ASSERT_EQ(holding.size(), 2u);
	ExpectWalkHome(holding[0], k_Home, 5.0f);
}

TEST(CreatureBringHome, BringFoodHomeWalksWithinItsHeight)
{
	const auto agenda = bring::BringFoodHome(9u, false, k_Home, 12.0f);
	ASSERT_EQ(agenda.size(), 3u);
	ExpectPickUp(agenda[0], 9u);
	ExpectWalkHome(agenda[1], k_Home, 12.0f);
	ExpectPutDown(agenda[2]);
	EXPECT_EQ(bring::BringFoodHome(std::nullopt, false, k_Home, 12.0f).size(), 2u);
}

TEST(CreatureBringHome, ALivingThingIsMadeStillAfterThePutDown)
{
	const auto agenda = bring::BringHome(7u, 7u, true, k_Home, 12.0f);
	ASSERT_EQ(agenda.size(), 4u);
	ExpectPutDown(agenda[2]);
	EXPECT_EQ(agenda[3].kind, Step::Kind::RenderImmobile);
	EXPECT_EQ(agenda[3].object, std::optional<uint32_t>(7u));
	// With the thing in its hand already, the step still acts on the thing of the plan
	const auto holding = bring::BringHome(std::nullopt, 7u, true, k_Home, 12.0f);
	ASSERT_EQ(holding.size(), 3u);
	EXPECT_EQ(holding[2].kind, Step::Kind::RenderImmobile);
}

TEST(CreatureBringHome, LivingFoodIsMadeStillAfterThePickUp)
{
	const auto agenda = bring::BringFoodHome(9u, true, k_Home, 12.0f);
	ASSERT_EQ(agenda.size(), 4u);
	ExpectPickUp(agenda[0], 9u);
	EXPECT_EQ(agenda[1].kind, Step::Kind::RenderImmobile);
	EXPECT_EQ(agenda[1].object, std::optional<uint32_t>(9u));
	ExpectWalkHome(agenda[2], k_Home, 12.0f);
	ExpectPutDown(agenda[3]);
	// Food in the hand already is not picked up, and nothing is made still
	EXPECT_EQ(bring::BringFoodHome(std::nullopt, true, k_Home, 12.0f).size(), 2u);
}

TEST(CreatureBringHome, TakeFoodFromFieldHomeTakesWheat)
{
	const auto agenda = bring::TakeFoodFromFieldHome(bring::Field {.field = 4u, .point = {10.0f, 20.0f}}, k_Home, 12.0f);
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 12.0f);
	EXPECT_FALSE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[1].movement.object, 4u);
	EXPECT_FLOAT_EQ(agenda[1].seconds, bring::k_FaceFieldSeconds);
	EXPECT_EQ(agenda[2].kind, Step::Kind::ActOnItself);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ(agenda[3].order.handful, ObjectOrder::Handful::Wheat);
	EXPECT_EQ(agenda[3].order.object, 4u);
	EXPECT_EQ(agenda[3].effect, Effect::TookFromPile);
	ExpectWalkHome(agenda[4], k_Home, 12.0f);
	ExpectPutDown(agenda[5]);

	// Holding food, it takes no wheat and walks home at once
	EXPECT_EQ(bring::TakeFoodFromFieldHome(std::nullopt, k_Home, 12.0f).size(), 2u);
}

// The planner makes no agenda without a home, and passes the field and the hand to the agendas
TEST(CreatureBringHome, PlanActionsNeedAHome)
{
	plan_actions::Situation situation;
	situation.height = 12.0f;
	const auto& takeWheat = ActionNamed("TakeFoodFromFieldToHome");
	EXPECT_FALSE(plan_actions::Agenda(takeWheat, 4u, {10.0f, 20.0f}, situation, First).has_value());
	situation.home = k_Home;
	const auto agenda = plan_actions::Agenda(takeWheat, 4u, {10.0f, 20.0f}, situation, First);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->size(), 6u);
	situation.holdingEatable = true;
	EXPECT_EQ(plan_actions::Agenda(takeWheat, 4u, {10.0f, 20.0f}, situation, First)->size(), 2u);

	const auto& bringHome = ActionNamed("BringHome");
	situation.handFull = true;
	EXPECT_EQ(plan_actions::Agenda(bringHome, 7u, {}, situation, First)->size(), 2u);
	situation.handFull = false;
	EXPECT_EQ(plan_actions::Agenda(bringHome, 7u, {}, situation, First)->size(), 3u);
}

TEST(CreatureBringHome, TheStillStepWaitsWhileTheThingFlies)
{
	const auto never = [](uint32_t /*range*/) { return 1u; };
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::RenderImmobile, .object = 5}});
	// While the thing flies, nothing happens and the step goes on
	auto commands = Think(mind, {.seconds = 0.1f, .objectFlying = true}, never);
	EXPECT_FALSE(commands.renderImmobile.has_value());
	EXPECT_EQ(mind.step, 0u);
	commands = Think(mind, {.seconds = 0.1f, .objectFlying = true}, never);
	EXPECT_FALSE(commands.renderImmobile.has_value());
	EXPECT_EQ(mind.step, 0u);
	// Down again, the thing starts to die and the step ends
	commands = Think(mind, {.seconds = 0.1f}, never);
	EXPECT_EQ(commands.renderImmobile, std::optional<uint32_t>(5u));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureBringHome, TheStillStepNeedsNoHand)
{
	EXPECT_EQ(HandsNeededFor(Step {.kind = Step::Kind::RenderImmobile}), HandsNeed::Any);
}
