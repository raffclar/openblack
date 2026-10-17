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
	ASSERT_EQ(agenda.size(), 3);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Action);
		EXPECT_EQ(step.animation, 63);
	}
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 72, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::Wait);
}

// "Guide play 47 loop 1": numbers up to 51 are static actions, the start before the number, its loop and the end after
// it, held ten seconds from the start, and played left to right half the time
TEST(CreatureScriptPlay, AStaticActionStartsLoopsTenSecondsAndEnds)
{
	const auto agenda = Agenda({.animation = 47, .plays = 2});
	ASSERT_EQ(agenda.size(), 2);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Static);
		EXPECT_EQ(step.sequence, (std::array<size_t, 3> {46, 47, 48}));
		EXPECT_FLOAT_EQ(step.seconds, 10.0f);
		EXPECT_TRUE(step.mirrorAtRandom);
		EXPECT_FALSE(step.untilRested);
	}
	// The last static action is 51; the next is played once
	EXPECT_EQ(Agenda({.animation = k_LastStaticAction, .plays = 1}).front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(Agenda({.animation = k_LastStaticAction + 1, .plays = 1}).front().kind, creature_mind::Step::Kind::Action);
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 47, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::Wait);
	EXPECT_FLOAT_EQ(none.front().seconds, 0.0f);
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
	EXPECT_FLOAT_EQ(none.front().seconds, 0.0f);
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
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::Wait);
	EXPECT_FLOAT_EQ(none.front().seconds, 0.0f);
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
// hundredth turn after starting (ten seconds of tenth-second turns), and has played once the end is over
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
		EXPECT_FALSE(Played(mind));
		// The end plays out, then it is done
		senses.bodyLooping = false;
		senses.bodyBusy = false;
		static_cast<void>(creature_mind::Think(mind, senses, random));
		EXPECT_TRUE(Played(mind));
	}
}

// A creature has played once its agenda is over, or while it only idles; it hasn't while any agenda goes on
TEST(CreatureScriptPlay, PlayedOnceTheAgendaIsOver)
{
	creature_mind::IdleMind mind;
	EXPECT_TRUE(Played(mind));
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = 1;
	EXPECT_FALSE(Played(mind));
	mind.step = 2;
	EXPECT_TRUE(Played(mind));
	// Its own agenda counts as well
	mind.activity = creature_mind::Activity::HangAround;
	mind.step = 0;
	EXPECT_FALSE(Played(mind));
	mind.activity = creature_mind::Activity::BeIdle;
	EXPECT_TRUE(Played(mind));
}

// A yes to the script's question stops the steps that remain, and those steps do not count as done
TEST(CreatureScriptPlay, AYesStopsTheStepsThatRemain)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::BeIdle;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = 1;
	mind.stepStarted = true;
	ASSERT_TRUE(Played(mind));
	ClearAfterPlayed(mind);
	EXPECT_EQ(mind.step, mind.agenda.size());
	EXPECT_FALSE(mind.stepStarted);
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_TRUE(mind.stoppedByScript);
	EXPECT_TRUE(Played(mind));
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
