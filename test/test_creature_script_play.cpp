/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureScriptPlay.h"

using namespace openblack;
using namespace openblack::creature_script_play;

// "CreatureCow play 63 loop 1" and the like: the individual action as many times as asked, one after another
TEST(CreatureScriptPlay, AnIndividualActionIsPlayedAsManyTimesAsAsked)
{
	const auto agenda = Agenda({.animation = 63, .plays = 3});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3);
	for (const auto& step : *agenda)
	{
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Action);
		EXPECT_EQ(step.animation, 63);
	}
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 72, .plays = 0});
	ASSERT_TRUE(none.has_value());
	ASSERT_EQ(none->size(), 1);
	EXPECT_EQ(none->front().kind, creature_mind::Step::Kind::Wait);
}

// Numbers up to 51 are static actions, a start, a loop and an end, which aren't played as individual actions
TEST(CreatureScriptPlay, StaticActionsAreNotIndividualActions)
{
	EXPECT_FALSE(Agenda({.animation = k_LastStaticAction, .plays = 1}).has_value());
	EXPECT_TRUE(Agenda({.animation = k_LastStaticAction + 1, .plays = 1}).has_value());
}

// A creature has played once its agenda is over, or while it only idles; it hasn't while any agenda goes on
TEST(CreatureScriptPlay, PlayedOnceTheAgendaIsOver)
{
	creature_mind::IdleMind mind;
	EXPECT_TRUE(Played(mind));
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = *Agenda({.animation = 63, .plays = 2});
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
