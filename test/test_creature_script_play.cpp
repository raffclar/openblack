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

#include <gtest/gtest.h>

#include "Creature/CreatureScriptPlay.h"

using namespace openblack;
using namespace openblack::creature_script_play;

// "CreatureCow play 63 loop 1" and the like: the individual action as many times as asked, one after another
TEST(CreatureScriptPlay, AnIndividualActionIsPlayedAsManyTimesAsAsked)
{
	const auto agenda = Agenda({.animation = 63, .plays = 3});
	ASSERT_EQ(agenda.size(), 4);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(agenda[i].kind, creature_mind::Step::Kind::Action);
		EXPECT_EQ(agenda[i].animation, 63);
	}
	// Then it waits until its body is free
	EXPECT_EQ(agenda.back().kind, creature_mind::Step::Kind::WaitUntilFree);
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 72, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// The agenda has 32 slots: at most 31 plays and the wait after them. A count of -1 does not make an agenda without end.
TEST(CreatureScriptPlay, ThePlaysFillAtMostTheSlotsOfTheAgenda)
{
	EXPECT_EQ(Agenda({.animation = 63, .plays = 31}).size(), 32u);
	EXPECT_EQ(Agenda({.animation = 63, .plays = 32}).size(), 32u);
	const auto most = Agenda({.animation = 47, .plays = 0xFFFFFFFFu});
	ASSERT_EQ(most.size(), k_AgendaSlots);
	EXPECT_EQ(most.front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(most.back().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// "Guide play 47 loop 1": numbers up to 51 are static actions, the start before the number, its loop and the end after
// it, held ten seconds from the start, and played left to right half the time
TEST(CreatureScriptPlay, AStaticActionStartsLoopsTenSecondsAndEnds)
{
	const auto agenda = Agenda({.animation = 47, .plays = 2});
	ASSERT_EQ(agenda.size(), 3);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto& step = agenda[i];
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Static);
		EXPECT_EQ(step.sequence, (std::array<size_t, 3> {46, 47, 48}));
		EXPECT_FLOAT_EQ(step.seconds, 10.0f);
		EXPECT_FALSE(step.untilRested);
	}
	EXPECT_EQ(agenda.back().kind, creature_mind::Step::Kind::WaitUntilFree);
	// The last static action is 51; the next is played once
	EXPECT_EQ(Agenda({.animation = k_LastStaticAction, .plays = 1}).front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(Agenda({.animation = k_LastStaticAction + 1, .plays = 1}).front().kind, creature_mind::Step::Kind::Action);
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 47, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// Static action 0 has no start to play, so it never begins and the creature never gets past it
TEST(CreatureScriptPlay, AStaticActionWithNoStartNeverEnds)
{
	const auto agenda = Agenda({.animation = 0, .plays = 1});
	ASSERT_EQ(agenda.size(), 1);
	EXPECT_EQ(agenda.front().kind, creature_mind::Step::Kind::Wait);
	EXPECT_TRUE(std::isinf(agenda.front().seconds));
	EXPECT_EQ(agenda.front().retriedClip, creature_mind::RetriedClip::Static);
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 0, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
	EXPECT_EQ(none.front().retriedClip, creature_mind::RetriedClip::None);
}

// The game compares the number with no sign, so a number below 0 is an individual action. The body has no such clip,
// so the action never starts and the creature never gets past it. Asked to play it no times, it only waits.
TEST(CreatureScriptPlay, AnIndividualActionBelowZeroNeverEnds)
{
	const auto agenda = Agenda({.animation = -1, .plays = 2});
	ASSERT_EQ(agenda.size(), 1);
	EXPECT_EQ(agenda.front().kind, creature_mind::Step::Kind::Wait);
	EXPECT_TRUE(std::isinf(agenda.front().seconds));
	EXPECT_EQ(agenda.front().retriedClip, creature_mind::RetriedClip::Individual);
	const auto none = Agenda({.animation = -5, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// Each turn the creature tries the action again and picks a side from the game's stream first. An individual action
// picks no side while the creature holds a thing, because the hand picks it.
TEST(CreatureScriptPlay, EachTryOfAMissingClipDrawsASide)
{
	for (const int32_t animation : {0, -1})
	{
		for (const bool holding : {false, true})
		{
			creature_mind::IdleMind mind;
			mind.activity = creature_mind::Activity::Told;
			mind.agenda = Agenda({.animation = animation, .plays = 1});
			int gameDraws = 0;
			const creature_mind::Random random = [](uint32_t) { return 0u; };
			const creature_mind::Random gameRandom = [&gameDraws](uint32_t range) {
				if (range == 2)
				{
					++gameDraws;
				}
				return 0u;
			};
			creature_mind::Senses senses {.seconds = 0.1f};
			if (holding)
			{
				senses.held = 7u;
			}
			for (int turn = 0; turn < 5; ++turn)
			{
				static_cast<void>(creature_mind::Think(mind, senses, random, gameRandom));
			}
			EXPECT_EQ(mind.step, 0u);
			const bool draws = animation == 0 || !holding;
			EXPECT_EQ(gameDraws, draws ? 5 : 0) << animation << " " << holding;
		}
	}
}

// Played by the mind: it starts the sequence, left to right when the draw of two comes up 1, ends the loop on the
// hundredth turn after starting (ten seconds of tenth-second turns). When the end is over, the step that waits until the
// body is free is done on the next turn, and the agenda is over on the turn after that.
TEST(CreatureScriptPlay, TheMindPlaysAStaticActionForTenSeconds)
{
	for (const uint32_t draw : {0u, 1u})
	{
		creature_mind::IdleMind mind;
		mind.activity = creature_mind::Activity::Told;
		mind.agenda = Agenda({.animation = 47, .plays = 1});
		const creature_mind::Random random = [draw](uint32_t range) { return range == 2 ? draw : 0u; };
		creature_mind::Senses senses {.seconds = 0.1f};
		const auto start = creature_mind::Think(mind, senses, random);
		ASSERT_TRUE(start.startSequence.has_value());
		EXPECT_EQ(*start.startSequence, (std::array<size_t, 3> {46, 47, 48}));
		EXPECT_EQ(start.mirrored, draw == 1);
		senses.bodyBusy = true;
		senses.bodyLooping = true;
		int endedOn = 0;
		for (int turn = 1; turn <= 120 && endedOn == 0; ++turn)
		{
			if (creature_mind::Think(mind, senses, random).endSit)
			{
				endedOn = turn;
			}
		}
		EXPECT_EQ(endedOn, 100);
		EXPECT_FALSE(Played(mind, true));
		// The end plays out
		senses.bodyLooping = false;
		senses.bodyBusy = false;
		static_cast<void>(creature_mind::Think(mind, senses, random));
		EXPECT_EQ(mind.step, 1u);
		EXPECT_FALSE(Played(mind, true));
		// The body is free: the last step is done, but its empty last stage holds the agenda for one more turn
		static_cast<void>(creature_mind::Think(mind, senses, random));
		EXPECT_EQ(mind.step, mind.agenda.size());
		EXPECT_FALSE(Played(mind, true));
		static_cast<void>(creature_mind::Think(mind, senses, random));
		EXPECT_TRUE(Played(mind, true));
	}
}

// An individual action: the turn after the body is free, the agenda goes past the empty end of the action and the step
// that waits until the body is free is done. The agenda is over on the turn after that.
TEST(CreatureScriptPlay, TheMindPlaysAnIndividualActionAndThenWaitsUntilTheBodyIsFree)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = Agenda({.animation = 63, .plays = 1});
	const creature_mind::Random random = [](uint32_t) { return 0u; };
	creature_mind::Senses senses {.seconds = 0.1f};
	const auto start = creature_mind::Think(mind, senses, random);
	ASSERT_TRUE(start.playOnce.has_value());
	senses.bodyBusy = true;
	for (int turn = 0; turn < 5; ++turn)
	{
		static_cast<void>(creature_mind::Think(mind, senses, random));
		EXPECT_FALSE(Played(mind, true));
	}
	// The action ends on the first turn that the body is free
	senses.bodyBusy = false;
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(Played(mind, true));
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_EQ(mind.step, mind.agenda.size());
	EXPECT_FALSE(Played(mind, true));
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_TRUE(Played(mind, true));
}

// While the agenda waits to go past the empty last stage of its last step, the creature has not played
TEST(CreatureScriptPlay, AnAgendaThatWaitsToGoPastAnEmptyLastStageHasNotPlayed)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 0}));
	const creature_mind::Random random = [](uint32_t) { return 0u; };
	const creature_mind::Senses senses {.seconds = 0.1f};
	static_cast<void>(creature_mind::Think(mind, senses, random));
	ASSERT_EQ(mind.step, mind.agenda.size());
	ASSERT_EQ(mind.endsNextTurn, mind.serial);
	EXPECT_FALSE(Played(mind, true));
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_TRUE(Played(mind, true));
}

// An idle creature has played at once. A yes in the turn before its agenda goes past an empty last stage stops the
// agenda, and the agenda does not complete.
TEST(CreatureScriptPlay, AYesToAnIdleCreatureStopsAnAgendaThatWaitsToGoPastAnEmptyLastStage)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::BeIdle, {{.kind = creature_mind::Step::Kind::WaitUntilFree}});
	const creature_mind::Random random = [](uint32_t) { return 0u; };
	const creature_mind::Senses senses {.seconds = 0.1f};
	static_cast<void>(creature_mind::Think(mind, senses, random));
	ASSERT_EQ(mind.endsNextTurn, mind.serial);
	ASSERT_TRUE(Played(mind, true));
	ClearAfterPlayed(mind);
	EXPECT_FALSE(mind.endsNextTurn.has_value());
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_TRUE(mind.stoppedByScript);
	EXPECT_TRUE(creature_mind::AgendaOver(mind));
	// The next turn does not end the agenda again, so the eyes do not rest
	EXPECT_FALSE(creature_mind::Think(mind, senses, random).restEyes);
}

// A creature has played once its agenda is over, or while it only idles; it hasn't while any agenda goes on
TEST(CreatureScriptPlay, PlayedOnceTheAgendaIsOver)
{
	creature_mind::IdleMind mind;
	EXPECT_TRUE(Played(mind, true));
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = 2;
	EXPECT_FALSE(Played(mind, true));
	mind.step = 3;
	EXPECT_TRUE(Played(mind, true));
	// Its own agenda counts as well
	mind.activity = creature_mind::Activity::HangAround;
	mind.step = 0;
	EXPECT_FALSE(Played(mind, true));
	mind.activity = creature_mind::Activity::BeIdle;
	EXPECT_TRUE(Played(mind, true));
}

// A creature whose plan is not valid has played, also while steps of its agenda are left. The yes stops those steps.
TEST(CreatureScriptPlay, ACreatureWithNoValidPlanHasPlayedWhileStepsAreLeft)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 2}));
	ASSERT_FALSE(creature_mind::AgendaOver(mind));
	EXPECT_FALSE(Played(mind, true));
	ASSERT_TRUE(Played(mind, false));
	ClearAfterPlayed(mind);
	EXPECT_TRUE(creature_mind::AgendaOver(mind));
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_TRUE(mind.stoppedByScript);
	EXPECT_TRUE(Played(mind, false));
}

// A yes to the script's question stops the steps that remain, and those steps do not count as done
TEST(CreatureScriptPlay, AYesStopsTheStepsThatRemain)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::BeIdle;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = 1;
	mind.stepStarted = true;
	ASSERT_TRUE(Played(mind, true));
	ClearAfterPlayed(mind);
	EXPECT_EQ(mind.step, mind.agenda.size());
	EXPECT_FALSE(mind.stepStarted);
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_TRUE(mind.stoppedByScript);
	EXPECT_TRUE(Played(mind, true));
	// A new agenda is not stopped
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 1}));
	EXPECT_FALSE(mind.stoppedByScript);
}

// An agenda at its end does not change, so that the creature completes it as usual
TEST(CreatureScriptPlay, AYesDoesNotChangeAnAgendaAtItsEnd)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = mind.agenda.size();
	ClearAfterPlayed(mind);
	EXPECT_EQ(mind.step, mind.agenda.size());
	EXPECT_FALSE(mind.gaveUp);
	EXPECT_FALSE(mind.stoppedByScript);
}
