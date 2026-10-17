/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <set>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"

using namespace openblack;
using creature_mind::Step;

namespace
{
/// A species that has every clip but those in the set
std::function<bool(size_t)> AllBut(std::set<size_t> missing)
{
	return [missing = std::move(missing)](size_t clip) { return !missing.contains(clip); };
}

/// A count of the draws of two from the game's random numbers
struct GameDraws
{
	creature_mind::Random Random()
	{
		return [this](uint32_t range) {
			if (range == 2)
			{
				++count;
			}
			return 0u;
		};
	}
	int count {0};
};

const creature_mind::Random k_MindRandom = [](uint32_t) { return 0u; };
} // namespace

// An action, a swap of minds and a communication test their one clip. A static action and a cast test the start, the
// loop and the end.
TEST(CreatureMissingClip, TheStepsTestTheClipsThatTheyStartWith)
{
	const Step action {.kind = Step::Kind::Action, .animation = 60};
	EXPECT_TRUE(creature_mind::LacksClipToStart(action, std::nullopt, AllBut({60})));
	EXPECT_FALSE(creature_mind::LacksClipToStart(action, std::nullopt, AllBut({61})));
	const Step swap {.kind = Step::Kind::SwapMind, .animation = 109};
	EXPECT_TRUE(creature_mind::LacksClipToStart(swap, std::nullopt, AllBut({109})));
	const Step communicate {.kind = Step::Kind::Communicate};
	EXPECT_TRUE(creature_mind::LacksClipToStart(communicate, 203u, AllBut({203})));
	EXPECT_FALSE(creature_mind::LacksClipToStart(communicate, std::nullopt, AllBut({203})));
	for (const auto kind : {Step::Kind::Static, Step::Kind::Cast})
	{
		const Step sequence {.kind = kind, .sequence = {37, 38, 39}};
		for (const size_t missing : {37u, 38u, 39u})
		{
			EXPECT_TRUE(creature_mind::LacksClipToStart(sequence, std::nullopt, AllBut({missing})));
		}
		EXPECT_FALSE(creature_mind::LacksClipToStart(sequence, std::nullopt, AllBut({40})));
	}
	// A faint and a move have no test
	const Step faint {.kind = Step::Kind::Static, .sequence = {12, 12, 13}, .holdLoop = true};
	EXPECT_FALSE(creature_mind::LacksClipToStart(faint, std::nullopt, AllBut({12, 13})));
	const Step move {.kind = Step::Kind::Move, .animation = 60};
	EXPECT_FALSE(creature_mind::LacksClipToStart(move, std::nullopt, AllBut({60})));
}

// A step whose clip the body does not have never starts. Each turn the creature tries again and picks a side from the
// game's random numbers first. When the clip is there, the step starts.
TEST(CreatureMissingClip, EachTurnTheCreatureTriesAgainAndPicksASide)
{
	for (const auto kind : {Step::Kind::Action, Step::Kind::Static, Step::Kind::Cast})
	{
		creature_mind::IdleMind mind;
		mind.activity = creature_mind::Activity::Told;
		mind.agenda = {{.kind = kind, .seconds = 1.0f, .animation = 60, .sequence = {59, 60, 61}}};
		GameDraws draws;
		creature_mind::Senses senses {.seconds = 0.1f, .clipMissing = true};
		for (int turn = 0; turn < 50; ++turn)
		{
			const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
			EXPECT_FALSE(commands.playOnce.has_value());
			EXPECT_FALSE(commands.startSequence.has_value());
		}
		EXPECT_EQ(mind.step, 0u);
		EXPECT_FALSE(mind.stepStarted);
		EXPECT_EQ(draws.count, 50);
		senses.clipMissing = false;
		const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
		EXPECT_TRUE(mind.stepStarted);
		EXPECT_TRUE(commands.playOnce.has_value() || commands.startSequence.has_value());
	}
}

// While the body is busy, the body refuses the try before it tests the clip. The try picks a side one time only.
TEST(CreatureMissingClip, ABusyBodyRefusesTheTryBeforeTheClipTest)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = {{.kind = Step::Kind::Action, .animation = 60}};
	GameDraws draws;
	const creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true, .clipMissing = true};
	static_cast<void>(creature_mind::Think(mind, senses, k_MindRandom, draws.Random()));
	EXPECT_EQ(draws.count, 1);
	EXPECT_FALSE(mind.stepStarted);
}

// Each try to communicate stops the desire to show its state for a time, as the start of the communication does
TEST(CreatureMissingClip, EachTryToCommunicateStopsTheDesireToShowItsState)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = {{.kind = Step::Kind::Communicate}};
	GameDraws draws;
	const creature_mind::Senses senses {
	    .seconds = 0.1f,
	    .clipMissing = true,
	    .communication = {.animation = 203u, .desire = creature_desires::Desire::Water},
	};
	for (int turn = 0; turn < 3; ++turn)
	{
		const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
		ASSERT_TRUE(commands.communicated.has_value());
		EXPECT_EQ(commands.communicated->desire, creature_desires::Desire::Water);
		EXPECT_FALSE(commands.playOnce.has_value());
	}
	EXPECT_EQ(draws.count, 3);
	EXPECT_FALSE(mind.stepStarted);
}

// A new agenda ends the tries
TEST(CreatureMissingClip, ANewAgendaEndsTheTries)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = {{.kind = Step::Kind::Action, .animation = 60}};
	GameDraws draws;
	creature_mind::Senses senses {.seconds = 0.1f, .clipMissing = true};
	static_cast<void>(creature_mind::Think(mind, senses, k_MindRandom, draws.Random()));
	mind.agenda = {{.kind = Step::Kind::Action, .animation = 61}};
	mind.step = 0;
	senses.clipMissing = false;
	const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
	EXPECT_EQ(commands.playOnce, std::optional<size_t>(61));
}
