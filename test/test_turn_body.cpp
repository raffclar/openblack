/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "GameTurnOrder.h"
#include "Simulation/TurnBody.h"

using openblack::game_turn::Step;
using openblack::simulation::IsViewStep;
using openblack::simulation::NoTurnView;

namespace
{

/// The steps of one game turn, in the order of the game
std::vector<Step> TurnSteps()
{
	std::vector<Step> steps;
	openblack::game_turn::PlayTurn([&steps](Step step) { steps.push_back(step); });
	return steps;
}

} // namespace

TEST(TurnBody, TheViewDoesTheSoundsTheMusicTheToolTipsAndTheHelpOfTheScreen)
{
	std::vector<Step> viewSteps;
	std::ranges::copy_if(TurnSteps(), std::back_inserter(viewSteps), IsViewStep);
	const std::vector<Step> expected {Step::TurnCheering, Step::HandToolTips, Step::HelpProfile, Step::Ambience, Step::Music};
	std::ranges::sort(viewSteps);
	auto sorted = expected;
	std::ranges::sort(sorted);
	EXPECT_EQ(viewSteps, sorted);
}

TEST(TurnBody, TheWorldDoesTheCommandsTheScriptsAndTheChecksum)
{
	EXPECT_FALSE(IsViewStep(Step::PlayerCommands));
	EXPECT_FALSE(IsViewStep(Step::Clock));
	EXPECT_FALSE(IsViewStep(Step::Living));
	EXPECT_FALSE(IsViewStep(Step::Scripts));
	EXPECT_FALSE(IsViewStep(Step::GlobalStatistics));
	EXPECT_FALSE(IsViewStep(Step::SyncChecksum));
	EXPECT_FALSE(IsViewStep(Step::RunRecord));
}

TEST(TurnBody, AWorldWithNoViewGivesTheStatisticsItsFixedFrameRate)
{
	NoTurnView view(10);
	EXPECT_EQ(view.FrameRate(), 10);
	// It does nothing in a step of the view
	view.PlayViewStep(Step::Music, {});
	EXPECT_EQ(view.FrameRate(), 10);
}
