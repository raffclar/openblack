/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "Creature/CreatureHelp.h"

using namespace openblack::creature_help;

namespace
{
constexpr uint32_t k_Tps = 10;

Message Of(Kind kind, uint32_t parameter = 0, bool forced = false)
{
	return {.kind = kind, .parameter = parameter, .forced = forced};
}
} // namespace

TEST(CreatureHelp, TextsAreTheGamesOwnNumbers)
{
	EXPECT_EQ(Text(Kind::PhysicalProblem, 0), 868u);
	EXPECT_EQ(Text(Kind::PhysicalProblem, 7), 875u);
	EXPECT_EQ(Text(Kind::LessonIncreaseSource, 0), 462u);
	EXPECT_EQ(Text(Kind::CurrentDesire, 1), 625u);
	EXPECT_EQ(Text(Kind::Tutorial, 11), 1095u);
	EXPECT_EQ(Text(Kind::FailureExplanation, k_PulledAway), 1103u);
	EXPECT_EQ(Text(Kind::FailureExplanation, k_Inaccessible), 1099u);
	EXPECT_EQ(Text(Kind::MiscellaneousStacked, 3), 1108u);
	// The thing too big to handle has a text of its own
	EXPECT_EQ(Text(Kind::MiscellaneousStacked, k_TooBigToHandle), k_TooBigToHandleText);
	// Skill 0 isn't told of, and a miracle has its own texts
	EXPECT_FALSE(Text(Kind::LessonNormalAction, 0).has_value());
	EXPECT_EQ(Text(Kind::LessonNormalAction, 4), 880u);
	EXPECT_FALSE(Text(Kind::LessonMagicAction, 3).has_value());
	EXPECT_EQ(Text(Kind::LessonMagicAction, 3, 905u), 905u);
	EXPECT_FALSE(Text(Kind::StageLearnToEat, 0).has_value());
}

TEST(CreatureHelp, StageMessagesStartTheirLessons)
{
	EXPECT_FALSE(StageScript(Kind::StageInitial, 1).has_value());
	EXPECT_EQ(StageScript(Kind::StageLearnToEat, 1), "CreatureDevLearnToEat");
	EXPECT_EQ(StageScript(Kind::StageLeashGoodAndEvil, 2), "CreatureDevLeashGoodAndEvil");
	EXPECT_EQ(StageScript(Kind::StageFullyMature, 1), "TheStorm");
	EXPECT_FALSE(StageScript(Kind::StageFullyMature, 2).has_value());
	EXPECT_FALSE(StageScript(Kind::CurrentDesire, 1).has_value());
}

TEST(CreatureHelp, OnlyAGrownUpCreatureOfThePlayersGivesHelp)
{
	Situation situation;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Tell);
	situation.stage = 12;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Drop);
	// The stage messages don't wait for it to grow up
	EXPECT_EQ(Decide(Of(Kind::StageLearnToEat), situation, false), Outcome::Tell);
	situation.stage = k_HelpStage;
	situation.playersCreature = false;
	EXPECT_EQ(Decide(Of(Kind::StageLearnToEat), situation, false), Outcome::Drop);
	situation.playersCreature = true;
	situation.insideTemple = true;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Drop);
}

TEST(CreatureHelp, TheOptionTurnsAllButTheStagesOff)
{
	Situation situation;
	situation.helpOn = false;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Drop);
	EXPECT_EQ(Decide(Of(Kind::StagePunishment), situation, false), Outcome::Tell);
}

TEST(CreatureHelp, HeldBackMessagesWaitIfTheirKindStacks)
{
	Situation situation;
	situation.dialogueTaken = true;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Stack);
	EXPECT_EQ(Decide(Of(Kind::CurrentDesire), situation, false), Outcome::Drop);
	situation.dialogueTaken = false;
	situation.inDevelopmentScript = true;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Stack);
	EXPECT_EQ(Decide(Of(Kind::StageLearnToEat), situation, false), Outcome::Tell);
	situation.inDevelopmentScript = false;
	situation.raisedThisTurn = true;
	EXPECT_EQ(Decide(Of(Kind::MessedUpAction), situation, false), Outcome::Stack);
	EXPECT_EQ(Decide(Of(Kind::StageLearnToEat), situation, false), Outcome::Tell);
	situation.raisedThisTurn = false;
	situation.handOnCreature = true;
	EXPECT_EQ(Decide(Of(Kind::Tutorial), situation, false), Outcome::Stack);
	// Forced, the hand on it doesn't hold it back
	EXPECT_EQ(Decide(Of(Kind::CurrentDesire, 0, true), situation, false), Outcome::Tell);
}

TEST(CreatureHelp, AsleepOrDyingNothingButForcedHelp)
{
	Situation situation;
	situation.outOrAsleep = true;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Drop);
	EXPECT_EQ(Decide(Of(Kind::CurrentDesire, 0, true), situation, false), Outcome::Tell);
}

TEST(CreatureHelp, SomeKindsOnlyWithTheCreatureInView)
{
	Situation situation;
	situation.inView = false;
	EXPECT_EQ(Decide(Of(Kind::FailureExplanation, k_Inaccessible), situation, false), Outcome::Drop);
	EXPECT_EQ(Decide(Of(Kind::Tutorial), situation, false), Outcome::Drop);
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, false), Outcome::Tell);
}

TEST(CreatureHelp, ToldLatelyWaitsOrGoes)
{
	const Situation situation;
	EXPECT_EQ(Decide(Of(Kind::PhysicalProblem), situation, true), Outcome::Stack);
	EXPECT_EQ(Decide(Of(Kind::CurrentDesire), situation, true), Outcome::Drop);
	EXPECT_EQ(Decide(Of(Kind::CurrentDesire, 0, true), situation, true), Outcome::Tell);
}

TEST(CreatureHelp, AMessageIsToldAgainAfterAMinute)
{
	Memory memory;
	EXPECT_FALSE(ToldLately(memory, Kind::PhysicalProblem, 2, 1000, k_Tps));
	MarkTold(memory, Kind::PhysicalProblem, 2, 1000);
	EXPECT_TRUE(ToldLately(memory, Kind::PhysicalProblem, 2, 1599, k_Tps));
	EXPECT_FALSE(ToldLately(memory, Kind::PhysicalProblem, 2, 1600, k_Tps));
	// Another parameter is another message
	EXPECT_FALSE(ToldLately(memory, Kind::PhysicalProblem, 4, 1001, k_Tps));
}

TEST(CreatureHelp, WhatItWantsIsToldAgainAfterFiveSeconds)
{
	Memory memory;
	MarkTold(memory, Kind::CurrentDesire, 4, 500);
	EXPECT_TRUE(ToldLately(memory, Kind::CurrentDesire, 4, 549, k_Tps));
	EXPECT_FALSE(ToldLately(memory, Kind::CurrentDesire, 4, 550, k_Tps));
}

TEST(CreatureHelp, TheStackTellsInKindOrderAndForgetsWhatItRaised)
{
	Memory memory;
	Stack(memory, Of(Kind::MessedUpAction, 1), 100, k_Tps);
	Stack(memory, Of(Kind::PhysicalProblem, 4), 100, k_Tps);
	Stack(memory, Of(Kind::PhysicalProblem, 6), 100, k_Tps);
	auto first = FirstWaiting(memory);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->kind, Kind::PhysicalProblem);
	EXPECT_EQ(memory.entries.at(static_cast<size_t>(first->kind)).at(first->index).parameter, 4u);
	Settle(memory, *first, 101);
	first = FirstWaiting(memory);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(memory.entries.at(static_cast<size_t>(first->kind)).at(first->index).parameter, 6u);
	Settle(memory, *first, 102);
	first = FirstWaiting(memory);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->kind, Kind::MessedUpAction);
	Settle(memory, *first, 103);
	EXPECT_FALSE(FirstWaiting(memory).has_value());
	// A settled message counts as told
	EXPECT_TRUE(ToldLately(memory, Kind::PhysicalProblem, 4, 200, k_Tps));
}

TEST(CreatureHelp, ARecentMessageDoesNotStackAgain)
{
	Memory memory;
	MarkTold(memory, Kind::PhysicalProblem, 4, 1000);
	Stack(memory, Of(Kind::PhysicalProblem, 4), 1600, k_Tps);
	EXPECT_FALSE(FirstWaiting(memory).has_value());
	Stack(memory, Of(Kind::PhysicalProblem, 4), 1610, k_Tps);
	EXPECT_TRUE(FirstWaiting(memory).has_value());
}

TEST(CreatureHelp, BodyProblemsEveryTwoMinutesInTheirOrder)
{
	const BodyThresholds thresholds;
	EXPECT_FALSE(BodyProblemNow({.energy = 0.1f}, thresholds, 120).has_value());
	EXPECT_EQ(BodyProblemNow({.energy = 0.1f}, thresholds, 121), BodyProblem::LowEnergy);
	EXPECT_EQ(BodyProblemNow({.life = 0.4f}, thresholds, 121), BodyProblem::Hurt);
	EXPECT_EQ(BodyProblemNow({.exhaustion = 0.9f}, thresholds, 121), BodyProblem::Tired);
	EXPECT_EQ(BodyProblemNow({.dehydration = 0.6f}, thresholds, 121), BodyProblem::Thirsty);
	EXPECT_EQ(BodyProblemNow({.energy = 0.4f, .life = 0.1f}, thresholds, 121), BodyProblem::LowEnergy);
	EXPECT_FALSE(BodyProblemNow({}, thresholds, 121).has_value());
}

TEST(CreatureHelp, FaintingSaysWhy)
{
	EXPECT_EQ(FaintingProblem({.life = 0.0f}), BodyProblem::BadlyHurt);
	EXPECT_EQ(FaintingProblem({.energy = 0.0f}), BodyProblem::VeryLowEnergy);
	EXPECT_EQ(FaintingProblem({.exhaustion = 1.0f}), BodyProblem::VeryTired);
	EXPECT_FALSE(FaintingProblem({}).has_value());
}

TEST(CreatureHelp, TheHandOverItBuildsUpSlowly)
{
	float handOver = 0.0f;
	int turns = 0;
	while (handOver < k_HandOverToTell)
	{
		handOver = NextHandOver(handOver, true);
		++turns;
	}
	// 1 - 0.9^n reaches a quarter on the third turn
	EXPECT_EQ(turns, 3);
	EXPECT_LT(NextHandOver(handOver, false), handOver);
}

TEST(CreatureHelp, PromptsTakeTurnsOldestFirst)
{
	Memory memory;
	const auto applies = TutorialApplies({.stage = 13});
	EXPECT_TRUE(applies.at(0));
	EXPECT_FALSE(applies.at(static_cast<size_t>(Prompt::Unleash)));
	EXPECT_FALSE(applies.at(static_cast<size_t>(Prompt::DropAtFeet)));
	// Two minutes must pass since the last prompt
	EXPECT_FALSE(TutorialNow(memory, applies, 1200, k_Tps, 120.0f).has_value());
	EXPECT_EQ(TutorialNow(memory, applies, 1210, k_Tps, 120.0f), 0u);
	memory.tutorialTurn = 1210;
	memory.tutorialToldTurn.at(0) = 1210;
	// Next, the first never told
	EXPECT_EQ(TutorialNow(memory, applies, 2420, k_Tps, 120.0f), 1u);
	memory.tutorialDone.at(1) = true;
	EXPECT_EQ(TutorialNow(memory, applies, 2420, k_Tps, 120.0f), 2u);
}

TEST(CreatureHelp, PromptConditions)
{
	const auto young = TutorialApplies({.stage = 2});
	EXPECT_FALSE(young.at(static_cast<size_t>(Prompt::Run)));
	const auto busy = TutorialApplies({.stage = 13, .wantsToBuildHome = true, .carrying = true});
	EXPECT_TRUE(busy.at(static_cast<size_t>(Prompt::MoreRocks)));
	EXPECT_TRUE(busy.at(static_cast<size_t>(Prompt::DropRocksAtHome)));
	EXPECT_TRUE(busy.at(static_cast<size_t>(Prompt::DropAtFeet)));
	EXPECT_TRUE(busy.at(static_cast<size_t>(Prompt::StrokeToEat)));
	EXPECT_FALSE(busy.at(static_cast<size_t>(Prompt::Drink)));
}

TEST(CreatureHelp, TheCameraMustLookAtTheCreature)
{
	const glm::vec3 origin {0.0f, 0.0f, 0.0f};
	const glm::vec3 focus {0.0f, 0.0f, 100.0f};
	EXPECT_TRUE(LooksAt(origin, focus, {0.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(LooksAt(origin, focus, {0.0f, 0.0f, 400.0f}));
	// About 46.7 degrees off the view either way
	EXPECT_TRUE(LooksAt(origin, focus, {std::tan(0.80f) * 50.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(LooksAt(origin, focus, {std::tan(0.83f) * 50.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(LooksAt(origin, focus, {0.0f, 0.0f, -50.0f}));
}

TEST(CreatureHelp, TheDominantDesireIsTheFirstGreatestActive)
{
	const std::array values {0.2f, 0.7f, 0.7f, 0.9f};
	const std::array active {true, true, true, false};
	EXPECT_EQ(DominantDesire(values, active), 1u);
	const std::array none {0.0f, 0.0f};
	const std::array on {true, true};
	EXPECT_EQ(DominantDesire(none, on), 0u);
}

TEST(CreatureHelp, TheHandsHoldShowsTheCreditedDesireElseThePlans)
{
	using openblack::creature_desires::Desire;
	// What it did lately comes first
	EXPECT_EQ(DesireShownOnHold(Desire::Hunger, Desire::Poo), Desire::Hunger);
	// Else what its plan serves
	EXPECT_EQ(DesireShownOnHold(std::nullopt, Desire::Poo), Desire::Poo);
	// A desire with no way of being shown is neither shown nor told of, even with a plan's that could be
	EXPECT_FALSE(DesireShownOnHold(Desire::BuildHome, Desire::Poo).has_value());
	EXPECT_FALSE(DesireShownOnHold(std::nullopt, std::nullopt).has_value());
}

TEST(CreatureHelp, FeedbackTooYoungToLearnFrom)
{
	EXPECT_EQ(TooYoungToLearn(0.5f).kind, Kind::MiscellaneousStacked);
	EXPECT_EQ(TooYoungToLearn(0.5f).parameter, k_StrokedTooYoung);
	EXPECT_EQ(TooYoungToLearn(-0.5f).parameter, k_SlappedTooYoung);
	EXPECT_EQ(Text(Kind::MiscellaneousStacked, k_StrokedTooYoung), 1109u);
}

TEST(CreatureHelp, FeedbackLessonsTellOfTheOpinionOrTheSource)
{
	using openblack::creature_desires::Desire;
	auto lesson = FeedbackLesson(0.4f, true, Desire::Hunger, 12);
	EXPECT_EQ(lesson.kind, Kind::LessonIncreaseOpinion);
	EXPECT_EQ(lesson.parameter, static_cast<uint32_t>(Desire::Hunger));
	lesson = FeedbackLesson(-0.4f, true, Desire::Hunger, 12);
	EXPECT_EQ(lesson.kind, Kind::LessonDecreaseOpinion);
	lesson = FeedbackLesson(0.4f, false, Desire::Hunger, 12);
	EXPECT_EQ(lesson.kind, Kind::LessonIncreaseSource);
	EXPECT_EQ(lesson.parameter, 12u);
	EXPECT_EQ(FeedbackLesson(-0.4f, false, Desire::Hunger, 12).kind, Kind::LessonDecreaseSource);
	EXPECT_EQ(Text(Kind::LessonDecreaseSource, 12), 535u);
}
