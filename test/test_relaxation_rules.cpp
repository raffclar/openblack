/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/RelaxationRules.h"

namespace rules = openblack::ecs::relaxation_rules;

namespace
{
/// A random number that is half the range asked for, recording the ranges
struct HalfRandom
{
	std::vector<float>* asked;
	float operator()(float range) const
	{
		asked->push_back(range);
		return range * 0.5f;
	}
};

TEST(RelaxationRules, APitchNeverPlayedOnDrawsEveryAdultFully)
{
	std::vector<float> asked;
	const HalfRandom random {&asked};
	EXPECT_EQ(rules::FootballDesire(15, {}, 5000, random), 1.0f);
	EXPECT_EQ(rules::FootballDesire(40, {}, 5000, random), 1.0f);
	// Children don't play
	EXPECT_EQ(rules::FootballDesire(14, {}, 5000, random), 0.0f);
	EXPECT_TRUE(asked.empty());
}

TEST(RelaxationRules, NoOneJoinsAGameOnWithFullTeams)
{
	std::vector<float> asked;
	const rules::FootballGame full {.firstTeam = 5, .secondTeam = 5, .playing = true, .lastGameTurn = 0};
	EXPECT_EQ(rules::FootballDesire(30, full, 5000, HalfRandom {&asked}), 0.0f);
	// Four a side isn't full
	const rules::FootballGame four {.firstTeam = 4, .secondTeam = 5, .playing = true, .lastGameTurn = 0};
	EXPECT_EQ(rules::FootballDesire(30, four, 5000, HalfRandom {&asked}), 1.0f);
}

TEST(RelaxationRules, AfterAGameThePitchDrawsByChanceOnlyOnceItsRestIsOver)
{
	std::vector<float> asked;
	const rules::FootballGame played {.lastGameTurn = 1000};
	// Under a whole rest period the share is drawn from nothing
	EXPECT_EQ(rules::FootballDesire(30, played, 1000 + 1799, HalfRandom {&asked}), 0.0f);
	EXPECT_EQ(rules::FootballDesire(30, played, 1000 + 1800, HalfRandom {&asked}), 0.5f);
	EXPECT_EQ(rules::FootballDesire(30, played, 1000 + 90000, HalfRandom {&asked}), 0.5f);
	EXPECT_EQ(asked, (std::vector<float> {0.0f, 1.0f, 1.0f}));
}

TEST(RelaxationRules, ThePitchIsWeighedFirstAndAnArtefactMustBeWantedStrictlyMore)
{
	const std::vector<float> artefacts {0.4f, 0.9f, 0.9f};
	const auto withPitch = rules::Choose(1.0f, artefacts);
	EXPECT_EQ(withPitch.choice, rules::Choice::Football);
	const auto weakPitch = rules::Choose(0.5f, artefacts);
	EXPECT_EQ(weakPitch.choice, rules::Choice::Artefact);
	// The first of two wanted as much
	EXPECT_EQ(weakPitch.index, 1u);
	const auto noPitch = rules::Choose(std::nullopt, artefacts);
	EXPECT_EQ(noPitch.choice, rules::Choice::Artefact);
	EXPECT_EQ(noPitch.index, 1u);
	// A pitch wanted not at all leaves the artefacts to be chosen; nothing wanted, nothing chosen
	EXPECT_EQ(rules::Choose(0.0f, artefacts).choice, rules::Choice::Artefact);
	EXPECT_EQ(rules::Choose(0.0f, std::vector<float> {0.0f}).choice, rules::Choice::None);
	EXPECT_EQ(rules::Choose(std::nullopt, {}).choice, rules::Choice::None);
}
} // namespace
