/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <random>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "Debug/FightAutopilotRules.h"

using namespace openblack;
using namespace openblack::fight_autopilot;
namespace fight = openblack::creature_fight;

namespace
{
constexpr uint32_t k_Own = 4;
constexpr uint32_t k_Guide = 5;

Observation Seeing(fight::FightAction own, fight::FightAction opponent, uint32_t queuedBlows = 0)
{
	return {.own = own, .opponent = opponent, .queuedBlows = queuedBlows, .queued = queuedBlows};
}

/// A fight transcript with records of the kinds that the lesson check reads
class FakeFight
{
public:
	FakeFight() { _fight = _recorder.Begin(k_Own, k_Guide, {}, {}); }
	void Action(uint32_t creature, std::string_view action)
	{
		_recorder.Add(creature, "action", {}, {{"creature", int64_t {creature}}, {"fight_action", action}});
	}
	void Queued(uint32_t blows)
	{
		_recorder.Add(k_Own, "move_queued", {}, {{"creature", int64_t {k_Own}}, {"blows_queued", blows}});
	}
	[[nodiscard]] LessonCheck Check() const { return CheckLesson(*_recorder.Find(_fight), k_Own); }

private:
	fight_transcript::Recorder _recorder;
	uint32_t _fight {0};
};
} // namespace

TEST(FightAutopilotRules, PolicyAndStageNamesReadBack)
{
	for (const auto policy : {Policy::Lesson, Policy::Aggressive, Policy::Random, Policy::Sequence})
	{
		EXPECT_EQ(PolicyFromName(Name(policy)), policy);
	}
	for (const auto stage : {LessonStage::Hit, LessonStage::Hits, LessonStage::Block, LessonStage::Step, LessonStage::Bout})
	{
		EXPECT_EQ(LessonStageFromName(Name(stage)), stage);
	}
	EXPECT_FALSE(PolicyFromName("timid").has_value());
}

TEST(FightAutopilotRules, TheLessonMovesOnAsEachGoalShows)
{
	using Action = fight::FightAction;
	// The guide must reel from a blow
	EXPECT_EQ(Advance(LessonStage::Hit, Seeing(Action::Blow, Action::Stance)), LessonStage::Hit);
	EXPECT_EQ(Advance(LessonStage::Hit, Seeing(Action::Blow, Action::ReelingMiddle)), LessonStage::Hits);
	EXPECT_EQ(Advance(LessonStage::Hit, Seeing(Action::Stance, Action::ReelingLow)), LessonStage::Hits);
	// More than two blows must wait in the queue
	EXPECT_EQ(Advance(LessonStage::Hits, Seeing(Action::Blow, Action::Stance, 2)), LessonStage::Hits);
	EXPECT_EQ(Advance(LessonStage::Hits, Seeing(Action::Blow, Action::Stance, 3)), LessonStage::Block);
	EXPECT_EQ(Advance(LessonStage::Block, Seeing(Action::Block, Action::Stance)), LessonStage::Step);
	EXPECT_EQ(Advance(LessonStage::Step, Seeing(Action::Block, Action::Stance)), LessonStage::Step);
	EXPECT_EQ(Advance(LessonStage::Step, Seeing(Action::Step, Action::Stance)), LessonStage::Bout);
	EXPECT_EQ(Advance(LessonStage::Bout, Seeing(Action::Step, Action::Stance)), LessonStage::Bout);
}

TEST(FightAutopilotRules, TheLessonPolicyDoesWhatEachStageAsks)
{
	std::mt19937 random(1);
	const auto seen = Seeing(fight::FightAction::Stance, fight::FightAction::Stance);
	const auto hit = Choose(Policy::Lesson, LessonStage::Hit, seen, random);
	EXPECT_EQ(hit.intent.kind, Intent::Kind::Hit);
	// The quick blows come faster than one blow plays, so that they wait in the queue
	const auto hits = Choose(Policy::Lesson, LessonStage::Hits, seen, random);
	EXPECT_EQ(hits.intent.kind, Intent::Kind::Hit);
	EXPECT_LT(hits.waitMs, hit.waitMs);
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Block, seen, random).intent.kind, Intent::Kind::Block);
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Step, seen, random).intent.kind, Intent::Kind::Step);
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Bout, seen, random).intent.kind, Intent::Kind::Hit);
}

TEST(FightAutopilotRules, TheBlockAndTheStepWaitForAnEmptyQueue)
{
	std::mt19937 random(1);
	const auto busy = Seeing(fight::FightAction::Blow, fight::FightAction::Stance, 2);
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Block, busy, random).intent.kind, Intent::Kind::None);
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Step, busy, random).intent.kind, Intent::Kind::None);
	// The quick blows do not wait: they must pile up in the queue
	EXPECT_EQ(Choose(Policy::Lesson, LessonStage::Hits, busy, random).intent.kind, Intent::Kind::Hit);
}

TEST(FightAutopilotRules, TheAggressivePolicyOnlyHits)
{
	std::mt19937 random(7);
	std::set<fight::Band> bands;
	for (int i = 0; i < 60; ++i)
	{
		const auto choice = Choose(Policy::Aggressive, LessonStage::Hit, {}, random);
		ASSERT_EQ(choice.intent.kind, Intent::Kind::Hit);
		bands.insert(choice.intent.band);
	}
	EXPECT_EQ(bands.size(), 3u);
}

TEST(FightAutopilotRules, TheRandomPolicyRepeatsForTheSameSeed)
{
	std::mt19937 first(42);
	std::mt19937 second(42);
	std::set<Intent::Kind> kinds;
	for (int i = 0; i < 100; ++i)
	{
		const auto a = Choose(Policy::Random, LessonStage::Hit, {}, first);
		const auto b = Choose(Policy::Random, LessonStage::Hit, {}, second);
		ASSERT_EQ(a.intent, b.intent);
		ASSERT_EQ(a.waitMs, b.waitMs);
		kinds.insert(a.intent.kind);
	}
	EXPECT_EQ(kinds.size(), 3u);
}

TEST(FightAutopilotRules, ASequenceReadsAndComesDueInTimeOrder)
{
	const auto sequence = ParseSequence("2.5:block,1:high,4:step_back");
	ASSERT_TRUE(sequence.has_value());
	ASSERT_EQ(sequence->size(), 3u);
	EXPECT_EQ(sequence->at(0).intent.kind, Intent::Kind::Hit);
	EXPECT_EQ(sequence->at(0).intent.band, fight::Band::High);
	EXPECT_FLOAT_EQ(sequence->at(0).atMs, 1000.0f);
	EXPECT_EQ(sequence->at(1).intent.kind, Intent::Kind::Block);
	EXPECT_EQ(sequence->at(2).intent.step, fight::Step::Back);

	EXPECT_FALSE(DueMove(*sequence, 0, 999.0f).has_value());
	EXPECT_EQ(DueMove(*sequence, 0, 1000.0f), 0u);
	EXPECT_EQ(DueMove(*sequence, 1, 3000.0f), 1u);
	EXPECT_FALSE(DueMove(*sequence, 3, 9000.0f).has_value());

	EXPECT_FALSE(ParseSequence("1:kick").has_value());
	EXPECT_FALSE(ParseSequence("soon:high").has_value());
	EXPECT_FALSE(ParseSequence("-1:high").has_value());
	EXPECT_FALSE(ParseSequence("high").has_value());
	EXPECT_TRUE(ParseSequence("")->empty());
}

TEST(FightAutopilotRules, TheLessonCheckFindsTheStagesInOrder)
{
	FakeFight fight;
	fight.Action(k_Own, "blow");
	fight.Action(k_Guide, "reeling_middle");
	fight.Queued(1);
	fight.Queued(3);
	fight.Action(k_Own, "block");
	fight.Action(k_Own, "step");
	const auto check = fight.Check();
	EXPECT_TRUE(check.InOrder());
	EXPECT_EQ(check.hit, 2u);
	EXPECT_EQ(check.hits, 4u);
	EXPECT_EQ(check.block, 5u);
	EXPECT_EQ(check.step, 6u);
}

TEST(FightAutopilotRules, TheLessonCheckFailsWhenAStageIsMissingOrEarly)
{
	{
		// The step comes before the block: the block is found, the step after it is not
		FakeFight fight;
		fight.Action(k_Guide, "reeling_high");
		fight.Queued(3);
		fight.Action(k_Own, "step");
		fight.Action(k_Own, "block");
		const auto check = fight.Check();
		EXPECT_FALSE(check.InOrder());
		EXPECT_TRUE(check.block.has_value());
		EXPECT_FALSE(check.step.has_value());
	}
	{
		// The player's own creature reeling is not the guide reeling
		FakeFight fight;
		fight.Action(k_Own, "reeling_high");
		fight.Queued(3);
		fight.Action(k_Own, "block");
		fight.Action(k_Own, "step");
		EXPECT_FALSE(fight.Check().hit.has_value());
		EXPECT_FALSE(fight.Check().InOrder());
	}
}
