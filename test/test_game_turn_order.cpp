/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "GameTurnOrder.h"

using namespace openblack::game_turn;

namespace
{

/// Records the steps that the turn does, in the order that it does them
class TurnRecorder
{
public:
	void operator()(Step step) { _steps.push_back(step); }

	[[nodiscard]] const std::vector<Step>& Steps() const { return _steps; }

	/// Where the step is in the record
	[[nodiscard]] std::ptrdiff_t IndexOf(Step step) const
	{
		return std::distance(_steps.begin(), std::ranges::find(_steps, step));
	}

	[[nodiscard]] bool Before(Step first, Step second) const { return IndexOf(first) < IndexOf(second); }

private:
	std::vector<Step> _steps;
};

TurnRecorder PlayOneTurn()
{
	TurnRecorder recorder;
	PlayTurn([&recorder](Step step) { recorder(step); });
	return recorder;
}

} // namespace

TEST(GameTurnOrder, EachStepHappensOnceInATurn)
{
	const auto recorder = PlayOneTurn();
	ASSERT_EQ(recorder.Steps().size(), static_cast<size_t>(Step::_Count));
	for (size_t i = 0; i < static_cast<size_t>(Step::_Count); ++i)
	{
		const auto step = static_cast<Step>(i);
		EXPECT_EQ(std::ranges::count(recorder.Steps(), step), 1) << "step " << i;
	}
}

TEST(GameTurnOrder, TheTurnDoesItsStepsInTheOrderOfTheList)
{
	const auto recorder = PlayOneTurn();
	ASSERT_EQ(recorder.Steps().size(), k_Turn.size());
	for (size_t i = 0; i < k_Turn.size(); ++i)
	{
		EXPECT_EQ(recorder.Steps().at(i), k_Turn.at(i).step) << "place " << i;
	}
}

TEST(GameTurnOrder, TheStepsFollowTheStagesOfTheGame)
{
	EXPECT_TRUE(InGameOrder(k_Turn));
}

TEST(GameTurnOrder, AStepOfAnEarlierStageAfterALaterOneIsOutOfOrder)
{
	constexpr std::array k_Wrong = std::to_array<Place>({
	    {.step = Step::Scripts, .stage = Stage::Scripts},
	    {.step = Step::Physics, .stage = Stage::Physics},
	});
	EXPECT_FALSE(InGameOrder(k_Wrong));
	// A step with no stage keeps its place and does not count
	constexpr std::array k_Unknown = std::to_array<Place>({
	    {.step = Step::Physics, .stage = Stage::Physics},
	    {.step = Step::Teleports, .stage = std::nullopt},
	    {.step = Step::Scripts, .stage = Stage::Scripts},
	});
	EXPECT_TRUE(InGameOrder(k_Unknown));
}

TEST(GameTurnOrder, TheHandInputComesBeforeTheTurnCountMovesOn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_EQ(recorder.IndexOf(Step::HandStillness), 0);
	EXPECT_TRUE(recorder.Before(Step::HandStillness, Step::Clock));
}

TEST(GameTurnOrder, TheStormsMoveBeforeThePlayersAndTheClimatesAfterTheTimeOfDay)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Storms, Step::Snow));
	EXPECT_TRUE(recorder.Before(Step::Snow, Step::Players));
	EXPECT_TRUE(recorder.Before(Step::TimeOfDay, Step::WeatherThings));
	EXPECT_TRUE(recorder.Before(Step::WeatherThings, Step::CameraBookmarks));
	EXPECT_TRUE(recorder.Before(Step::CameraBookmarks, Step::ScriptHighlights));
	EXPECT_TRUE(recorder.Before(Step::ScriptHighlights, Step::Climates));
}

TEST(GameTurnOrder, TheAlignmentsMoveAtTheEndOfThePlayersTurns)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Players, Step::TempleLooks));
	EXPECT_TRUE(recorder.Before(Step::TempleLooks, Step::PlayerAlignment));
	EXPECT_TRUE(recorder.Before(Step::WorshipSiteLooks, Step::PlayerAlignment));
	EXPECT_TRUE(recorder.Before(Step::PlayerAlignment, Step::Dances));
}

TEST(GameTurnOrder, TheFieldsAndFishFarmsComeBeforeTheTracksAndTheFootpaths)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Dances, Step::Fields));
	EXPECT_TRUE(recorder.Before(Step::Fields, Step::FishFarms));
	EXPECT_TRUE(recorder.Before(Step::FishFarms, Step::WalkPaths));
	EXPECT_TRUE(recorder.Before(Step::WalkPaths, Step::Footpaths));
	EXPECT_TRUE(recorder.Before(Step::Footpaths, Step::ForestGrowth));
}

TEST(GameTurnOrder, TheAnimalsTakeTheirTurnWithTheLivingBeforeTheFires)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Villagers, Step::Animals));
	EXPECT_TRUE(recorder.Before(Step::Animals, Step::Fires));
	EXPECT_TRUE(recorder.Before(Step::Fires, Step::Reactions));
	EXPECT_TRUE(recorder.Before(Step::Reactions, Step::Miracles));
}

TEST(GameTurnOrder, TheScriptsSeeTheWorldAfterTheMiraclesParticlesAndPhysics)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Miracles, Step::Particles));
	EXPECT_TRUE(recorder.Before(Step::Particles, Step::Fireflies));
	EXPECT_TRUE(recorder.Before(Step::Fireflies, Step::Physics));
	EXPECT_TRUE(recorder.Before(Step::Physics, Step::Vortices));
	EXPECT_TRUE(recorder.Before(Step::Vortices, Step::Scripts));
	EXPECT_TRUE(recorder.Before(Step::Scripts, Step::ScriptFade));
}

TEST(GameTurnOrder, TheHelpComesAfterTheScriptsAndBeforeTheTimeOfDay)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::ScriptFade, Step::Advisors));
	EXPECT_TRUE(recorder.Before(Step::Advisors, Step::HandToolTips));
	EXPECT_TRUE(recorder.Before(Step::HandToolTips, Step::Banter));
	EXPECT_TRUE(recorder.Before(Step::Banter, Step::HelpProfile));
	EXPECT_TRUE(recorder.Before(Step::HelpProfile, Step::TimeOfDay));
}

TEST(GameTurnOrder, TheLateStepsOfTheTurnComeInTheGamesOrder)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Climates, Step::HeldThing));
	EXPECT_TRUE(recorder.Before(Step::HeldThing, Step::Rewards));
	EXPECT_TRUE(recorder.Before(Step::Rewards, Step::Guidance));
	EXPECT_TRUE(recorder.Before(Step::Guidance, Step::CameraChecks));
	EXPECT_TRUE(recorder.Before(Step::CameraChecks, Step::BuildingPieces));
	EXPECT_TRUE(recorder.Before(Step::BuildingPieces, Step::MusicMoods));
}

TEST(GameTurnOrder, TheEndOfTheTurnSoundsThenSavesThenEndsTheGame)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::MusicMoods, Step::SoundTags));
	EXPECT_TRUE(recorder.Before(Step::SoundTags, Step::Ambience));
	EXPECT_TRUE(recorder.Before(Step::Ambience, Step::Autosave));
	EXPECT_TRUE(recorder.Before(Step::Autosave, Step::GameOver));
	EXPECT_TRUE(recorder.Before(Step::GameOver, Step::WalkerLookahead));
	EXPECT_EQ(recorder.IndexOf(Step::InfluenceTurnEnd), static_cast<std::ptrdiff_t>(k_Turn.size()) - 1);
}
