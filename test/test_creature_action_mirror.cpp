/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"

using namespace openblack;
using creature_mind::Step;

namespace
{
/// The game's random numbers: it keeps the range of each draw and gives the same number each time
struct GameDraws
{
	creature_mind::Random Random()
	{
		return [this](uint32_t range) {
			ranges.push_back(range);
			return range > value ? value : 0u;
		};
	}
	[[nodiscard]] int Twos() const { return static_cast<int>(std::ranges::count(ranges, 2u)); }
	uint32_t value {0};
	std::vector<uint32_t> ranges;
};

/// The mind's own random numbers. They give 1, so a side that they chose is the left side.
const creature_mind::Random k_MindRandom = [](uint32_t range) { return range > 1 ? 1u : 0u; };

creature_mind::IdleMind Told(Step step)
{
	creature_mind::IdleMind mind;
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = {step};
	return mind;
}

const std::vector<Step> k_StepsThatTryToStart {
    {.kind = Step::Kind::Action, .animation = 60},
    {.kind = Step::Kind::Static, .sequence = {28, 29, 30}, .untilRested = true, .closedEyes = true},
    {.kind = Step::Kind::Static, .seconds = 5.0f, .sequence = {37, 38, 39}},
    {.kind = Step::Kind::Cast, .seconds = 1.0f, .sequence = {110, 111, 112}},
    {.kind = Step::Kind::SwapMind, .animation = 109},
};

const creature_desires::Communication k_Communication {.animation = 203u, .desire = creature_desires::Desire::Water};
} // namespace

// Each turn that the busy body refuses the start, the creature picks a side from the game's random numbers
TEST(CreatureActionMirror, ABusyBodyRefusesTheStartAndASideIsPickedEachTurn)
{
	for (const auto& step : k_StepsThatTryToStart)
	{
		auto mind = Told(step);
		GameDraws draws;
		creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true};
		senses.swap.spellsGone = true;
		for (int turn = 0; turn < 20; ++turn)
		{
			const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
			EXPECT_FALSE(commands.playOnce.has_value());
			EXPECT_FALSE(commands.startSequence.has_value());
		}
		EXPECT_FALSE(mind.stepStarted);
		EXPECT_EQ(draws.Twos(), 20) << static_cast<int>(step.kind);
	}
}

// A busy body also refuses a communication. Each try picks a side and stops the desire to show its state for a time.
TEST(CreatureActionMirror, ABusyBodyRefusesACommunication)
{
	auto mind = Told({.kind = Step::Kind::Communicate});
	GameDraws draws;
	const creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true, .communication = k_Communication};
	for (int turn = 0; turn < 3; ++turn)
	{
		const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
		ASSERT_TRUE(commands.communicated.has_value());
		EXPECT_FALSE(commands.playOnce.has_value());
	}
	EXPECT_EQ(draws.Twos(), 3);
}

// A faint, a communication with no clip and a walk start no clip with a try, so a busy body costs them no draw
TEST(CreatureActionMirror, StepsWithNoTryPickNoSideWhileTheBodyIsBusy)
{
	const std::vector<Step> steps {
	    {.kind = Step::Kind::Static, .seconds = 5.0f, .sequence = {106, 106, 107}, .holdLoop = true},
	    {.kind = Step::Kind::Communicate},
	    {.kind = Step::Kind::Move, .movement = {.kind = creature_mind::Movement::Kind::ToPoint}},
	};
	for (const auto& step : steps)
	{
		auto mind = Told(step);
		GameDraws draws;
		const creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true, .moving = true};
		for (int turn = 0; turn < 5; ++turn)
		{
			static_cast<void>(creature_mind::Think(mind, senses, k_MindRandom, draws.Random()));
		}
		EXPECT_EQ(draws.Twos(), 0) << static_cast<int>(step.kind);
	}
}

// The game's random numbers, not the mind's, choose the side of each step as it starts. Every static step, the sleep
// and the faint too, and every cast play on the left side one time in two.
TEST(CreatureActionMirror, TheGameChoosesTheSideOfEachStart)
{
	auto steps = k_StepsThatTryToStart;
	steps.push_back({.kind = Step::Kind::Static, .seconds = 5.0f, .sequence = {106, 106, 107}, .holdLoop = true});
	steps.push_back({.kind = Step::Kind::Communicate});
	for (const auto& step : steps)
	{
		for (const uint32_t value : {0u, 1u})
		{
			auto mind = Told(step);
			GameDraws draws {.value = value};
			creature_mind::Senses senses {.seconds = 0.1f, .communication = k_Communication};
			senses.swap.spellsGone = true;
			const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
			ASSERT_TRUE(mind.stepStarted);
			ASSERT_TRUE(commands.playOnce.has_value() || commands.startSequence.has_value());
			EXPECT_EQ(commands.mirrored, value == 1) << static_cast<int>(step.kind);
			EXPECT_EQ(draws.Twos(), 1) << static_cast<int>(step.kind);
			// The side is the last draw of the start
			EXPECT_EQ(draws.ranges.back(), 2u);
		}
	}
}

// In a dance with villagers, each turn of a move that plays an action or a pose picks a side, also while the body
// plays. The action starts once the body is free, on the side of that turn.
TEST(CreatureActionMirror, EachTurnOfADanceMovePicksASide)
{
	for (const uint32_t move : {35u, 36u})
	{
		auto mind = Told({.kind = Step::Kind::Dance, .dance = {.kind = creature_mind::DanceOrder::Kind::Dance}});
		mind.stepStarted = true;
		GameDraws draws {.value = 1};
		creature_mind::Senses senses {.seconds = 0.1f, .bodyPlaying = true};
		senses.dance = creature_mind::Senses::DanceView {.exists = true, .inGroup = true, .action = move, .first = 72};
		for (int turn = 0; turn < 4; ++turn)
		{
			const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
			EXPECT_FALSE(commands.playOnce.has_value());
			EXPECT_FALSE(commands.startSequence.has_value());
		}
		EXPECT_EQ(draws.Twos(), 4);
		senses.bodyPlaying = false;
		const auto commands = creature_mind::Think(mind, senses, k_MindRandom, draws.Random());
		EXPECT_TRUE(commands.playOnce.has_value() || commands.startSequence.has_value());
		EXPECT_TRUE(commands.mirrored);
		EXPECT_EQ(draws.Twos(), 5);
	}
}
