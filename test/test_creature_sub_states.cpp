/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureSubStates.h"

using namespace openblack;
using creature_mind::Gaze;
using creature_mind::Step;
using creature_sub_states::SubState;

namespace
{
constexpr float k_Turn = 0.1f;

creature_mind::Random Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}

creature_mind::Senses IdleSenses()
{
	return {.seconds = k_Turn};
}

/// One turn of the creature: the head takes its look first, then the mind thinks
std::optional<Gaze> Turn(creature_mind::IdleMind& mind)
{
	const auto look = creature_mind::TurnLook(mind);
	static_cast<void>(creature_mind::Think(mind, IdleSenses(), Always(0)));
	return look;
}
} // namespace

TEST(CreatureSubStates, TheTableHoldsTheStagesOfEachKindOfStep)
{
	using creature_sub_states::StagesOf;
	EXPECT_EQ(creature_sub_states::k_SubStateStages.size(), 143u);
	// A wait has only the stage of each turn
	EXPECT_FALSE(StagesOf(SubState::Wait).start);
	EXPECT_TRUE(StagesOf(SubState::Wait).turn);
	EXPECT_FALSE(StagesOf(SubState::Wait).end);
	// A walk to a point has a start and an end, but no stage of each turn
	EXPECT_TRUE(StagesOf(SubState::MoveToPos).start);
	EXPECT_FALSE(StagesOf(SubState::MoveToPos).turn);
	EXPECT_TRUE(StagesOf(SubState::MoveToPos).end);
	// A sleep has no end stage, and a rest to get better has one
	EXPECT_FALSE(StagesOf(SubState::Sleep).end);
	EXPECT_TRUE(StagesOf(SubState::RestToGetBetter).end);
	// To make a thing still has only the end stage
	EXPECT_FALSE(StagesOf(SubState::RenderImmobile).start);
	EXPECT_FALSE(StagesOf(SubState::RenderImmobile).turn);
	EXPECT_TRUE(StagesOf(SubState::RenderImmobile).end);
	// The last kind of step has the stage of each turn only
	EXPECT_TRUE(StagesOf(SubState::EatNearestFoodPile).turn);
	EXPECT_FALSE(StagesOf(SubState::EatNearestFoodPile).end);
}

TEST(CreatureSubStates, ALookHoldsOnlyAfterAKindWithNoEndStage)
{
	using creature_sub_states::LookHoldsAfterEnd;
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::OrderCreature));
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::RequestPartner));
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::DemobiliseObject));
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::Wait));
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::GoNearObject));
	EXPECT_TRUE(LookHoldsAfterEnd(SubState::Faint));
	EXPECT_FALSE(LookHoldsAfterEnd(SubState::TurnToFaceObject));
	EXPECT_FALSE(LookHoldsAfterEnd(SubState::MoveToPos));
	EXPECT_FALSE(LookHoldsAfterEnd(SubState::StaticAction));
	EXPECT_FALSE(LookHoldsAfterEnd(SubState::Fight));
	// A kind with no stages takes no turn, so its look never shows
	EXPECT_FALSE(LookHoldsAfterEnd(SubState::WalkAlongCoast));
}

TEST(CreatureSubStates, EachStepGivesTheKindOfTheGameStepThatItStandsFor)
{
	using creature_mind::SubStateOf;
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Order}), SubState::OrderCreature);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Wait}), SubState::Wait);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Static}), SubState::StaticAction);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Static, .untilRested = true}), SubState::Sleep);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Static, .untilBetter = true}), SubState::RestToGetBetter);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Static, .faint = true}), SubState::Faint);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Static, .effect = creature_mind::Effect::Poo}), SubState::Poo);

	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = creature_mind::Movement::Kind::TurnToFaceObject, .object = 3u};
	EXPECT_EQ(SubStateOf(turn), SubState::TurnToFaceObject);
	turn.movement = {.kind = creature_mind::Movement::Kind::TurnToFace};
	EXPECT_EQ(SubStateOf(turn), SubState::TurnToFacePos);

	Step take {.kind = Step::Kind::Object};
	take.order = {.kind = creature_mind::ObjectOrder::Kind::TakeFromPile};
	EXPECT_EQ(SubStateOf(take), SubState::CreatePickUpThenRemove);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Kiss}), SubState::SingleKiss);
	EXPECT_EQ(SubStateOf({.kind = Step::Kind::Kiss, .object = 2u}), SubState::Kiss);
}

TEST(CreatureSubStates, TheHeadTakesTheLookOfTheStepThatTheLastTurnLeft)
{
	creature_mind::IdleMind mind;
	// No agenda: the head looks about
	EXPECT_FALSE(creature_mind::TurnLook(mind).has_value());

	// An order step has no end stage, so its look holds one more turn. The same is true of the demobilise step, which
	// has no look: on that turn the head looks about.
	creature_mind::Plan(mind, creature_mind::Activity::None,
	                    {
	                        {.kind = Step::Kind::Order, .object = 2u, .gaze = Gaze {.object = 1u}},
	                        {.kind = Step::Kind::Demobilise, .object = 2u},
	                        {.kind = Step::Kind::Wait, .seconds = 1.0f, .gaze = Gaze {.object = 7u}},
	                    });
	const auto first = Turn(mind);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->object, 1u);
	EXPECT_EQ(mind.step, 1u);
	// The order ended, but its look holds while the demobilise step runs
	const auto second = Turn(mind);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->object, 1u);
	EXPECT_EQ(mind.step, 2u);
	// The demobilise step ended with no look: the head looks about while the wait starts
	EXPECT_FALSE(Turn(mind).has_value());
	const auto fourth = Turn(mind);
	ASSERT_TRUE(fourth.has_value());
	EXPECT_EQ(fourth->object, 7u);
}

TEST(CreatureSubStates, TheLookOfAStepWithAnEndStageEndsWithTheStep)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None,
	                    {
	                        {.kind = Step::Kind::RenderImmobile, .gaze = Gaze {.object = 4u}},
	                        {.kind = Step::Kind::Wait, .seconds = 1.0f, .gaze = Gaze {.object = 7u}},
	                    });
	const auto first = Turn(mind);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->object, 4u);
	EXPECT_EQ(mind.step, 1u);
	const auto second = Turn(mind);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->object, 7u);
}

TEST(CreatureSubStates, TheLookOfTheLastStepHoldsAfterTheAgendaButNotIntoANewAgenda)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::None,
	                    {{.kind = Step::Kind::Order, .object = 2u, .gaze = Gaze {.object = 1u}}});
	static_cast<void>(Turn(mind));
	EXPECT_EQ(mind.step, mind.agenda.size());
	const auto held = creature_mind::TurnLook(mind);
	ASSERT_TRUE(held.has_value());
	EXPECT_EQ(held->object, 1u);

	// A new agenda replaces the held look with the look of its first step
	creature_mind::Plan(mind, creature_mind::Activity::None,
	                    {{.kind = Step::Kind::Wait, .seconds = 1.0f, .gaze = Gaze {.object = 9u}}});
	const auto next = creature_mind::TurnLook(mind);
	ASSERT_TRUE(next.has_value());
	EXPECT_EQ(next->object, 9u);
}
