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

/// The steps of the turn alone
TurnRecorder PlayTurnSteps()
{
	TurnRecorder recorder;
	PlayTurn([&recorder](Step step) { recorder(step); });
	return recorder;
}

/// The turn with the creatures' turn in the turn of the living, as the game plays it with a creature
TurnRecorder PlayOneTurn()
{
	TurnRecorder recorder;
	PlayTurn([&recorder](Step step) {
		recorder(step);
		if (step == Step::Living)
		{
			PlayCreaturesTurn([&recorder](Step creatureStep) { recorder(creatureStep); });
		}
	});
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
	const auto recorder = PlayTurnSteps();
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
	// The drawn things go back to the places of the last turn first. The commands of the players come next, then the
	// hand's own work of the turn.
	EXPECT_EQ(recorder.IndexOf(Step::DrawnPlaces), 0);
	EXPECT_EQ(recorder.IndexOf(Step::PlayerCommands), 1);
	EXPECT_EQ(recorder.IndexOf(Step::HandStillness), 2);
	EXPECT_TRUE(recorder.Before(Step::HandStillness, Step::Clock));
}

TEST(GameTurnOrder, TheHandDoesItsOwnWorkAfterTheCommandsOfTheTurn)
{
	const auto recorder = PlayOneTurn();
	// The commands of the turn come first, so a command that the hand sends in its own work waits for a later turn
	EXPECT_TRUE(recorder.Before(Step::PlayerCommands, Step::HandInterface));
	EXPECT_TRUE(recorder.Before(Step::HandStillness, Step::HandInterface));
	EXPECT_TRUE(recorder.Before(Step::HandInterface, Step::Clock));
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
	EXPECT_TRUE(recorder.Before(Step::ForestGrowth, Step::Living));
	EXPECT_TRUE(recorder.Before(Step::Living, Step::MapCellsAfterLiving));
	EXPECT_TRUE(recorder.Before(Step::MapCellsAfterLiving, Step::Fires));
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
	EXPECT_TRUE(recorder.Before(Step::HeldThing, Step::PlayerSparkles));
	EXPECT_TRUE(recorder.Before(Step::PlayerSparkles, Step::Rewards));
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
	EXPECT_EQ(recorder.IndexOf(Step::InfluenceTurnEnd), static_cast<std::ptrdiff_t>(recorder.Steps().size()) - 3);
	// The checksum of the turn comes after all the work of the turn, just before a tool's run record
	EXPECT_EQ(recorder.IndexOf(Step::SyncChecksum), static_cast<std::ptrdiff_t>(recorder.Steps().size()) - 2);
}

TEST(GameTurnOrder, TheRunRecordSeesTheWorldAfterTheLastStepOfTheTurn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_EQ(recorder.IndexOf(Step::RunRecord), static_cast<std::ptrdiff_t>(recorder.Steps().size()) - 1);
}

// The real-world weather follows the turn of the game, before the walker that came up against an edge tries again
TEST(GameTurnOrder, TheRealWeatherFollowsTheTurnOfTheGame)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::GlobalStatistics, Step::RealWeather));
	EXPECT_TRUE(recorder.Before(Step::RealWeather, Step::WalkerLookahead));
}

TEST(GameTurnOrder, TheHandSpeedForTheSmokeIsPartOfTheInput)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::HandStillness, Step::ChimneySmoke));
	EXPECT_TRUE(recorder.Before(Step::ChimneySmoke, Step::Clock));
}

TEST(GameTurnOrder, TheTotemsTakeTheirTurnWithTheirTowns)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Players, Step::VillageTotems));
	EXPECT_TRUE(recorder.Before(Step::VillageTotems, Step::PlayerAlignment));
}

TEST(GameTurnOrder, TheTeleportTravellersAreDroppedAfterTheTownsAndBeforeTheComputerPlayers)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::VillageTotems, Step::Teleports));
	EXPECT_TRUE(recorder.Before(Step::Teleports, Step::ComputerPlayers));
	EXPECT_TRUE(recorder.Before(Step::Teleports, Step::Living));
	EXPECT_TRUE(recorder.Before(Step::Teleports, Step::Reactions));
}

TEST(GameTurnOrder, TheCreaturesHelpPictureAndAlignmentArePartOfTheirOwnTurn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreaturePlans, Step::CreatureCarryOver));
	EXPECT_TRUE(recorder.Before(Step::CreatureCarryOver, Step::CreatureWebPage));
	EXPECT_TRUE(recorder.Before(Step::CreatureWebPage, Step::CreatureAlignment));
	// So the scripts and the miracles see the alignment of this turn
	EXPECT_TRUE(recorder.Before(Step::CreatureAlignment, Step::Fires));
}

TEST(GameTurnOrder, TheLeashPullsACreatureBeforeItsBodyLivesTheTurn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreaturePens, Step::CreatureBreath));
	EXPECT_TRUE(recorder.Before(Step::CreatureBreath, Step::Leashes));
	EXPECT_TRUE(recorder.Before(Step::Leashes, Step::CreaturePhysiology));
	EXPECT_TRUE(recorder.Before(Step::CreaturePhysiology, Step::CreatureMinds));
}

TEST(GameTurnOrder, TheAdvisorTellsOfACreatureBeforeItsDesiresChange)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreatureMinds, Step::CreatureHelp));
	EXPECT_TRUE(recorder.Before(Step::CreatureHelp, Step::CreatureDesires));
}

TEST(GameTurnOrder, ACreatureDecidesBeforeItDoesTheStepOfItsAgenda)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreatureDesires, Step::CreaturePlans));
	EXPECT_TRUE(recorder.Before(Step::CreaturePlans, Step::CreatureAgendas));
	EXPECT_TRUE(recorder.Before(Step::CreatureAgendas, Step::CreatureLocomotion));
}

TEST(GameTurnOrder, TheBodyOfACreatureFollowsItsFatnessAfterItsAction)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreatureFights, Step::CreatureBodies));
	EXPECT_TRUE(recorder.Before(Step::CreatureBodies, Step::CreatureFizz));
	// Then its body lives the turn, and takes its pose at the end
	EXPECT_TRUE(recorder.Before(Step::CreatureFizz, Step::CreatureSkin));
	EXPECT_TRUE(recorder.Before(Step::CreatureSkin, Step::CreatureMotes));
	EXPECT_TRUE(recorder.Before(Step::CreatureMotes, Step::CreatureBlood));
	// The shape that the body is drawn with follows what it became after the marks heal, before the motes, as the game's
	// creature does in its turn
	EXPECT_TRUE(recorder.Before(Step::CreatureSkin, Step::CreatureMorph));
	EXPECT_TRUE(recorder.Before(Step::CreatureMorph, Step::CreatureMotes));
	EXPECT_TRUE(recorder.Before(Step::CreatureBlood, Step::CreatureSway));
	EXPECT_TRUE(recorder.Before(Step::CreatureSway, Step::CreatureFightMoves));
	EXPECT_TRUE(recorder.Before(Step::CreatureFightMoves, Step::CreaturePoses));
}

TEST(GameTurnOrder, AFighterGetsBackStaminaAfterItsWebPage)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreatureWebPage, Step::CreatureFightStamina));
	EXPECT_TRUE(recorder.Before(Step::CreatureFightStamina, Step::CreatureAlignment));
}

TEST(GameTurnOrder, ThePartsOfTheCreaturesTurnsForNoOneCreatureComeBeforeTheLiving)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreaturesTurnStart, Step::Living));
	EXPECT_TRUE(recorder.Before(Step::ForestGrowth, Step::CreaturesTurnStart));
}

TEST(GameTurnOrder, TheTipBubbleClosesBetweenTheToolTipsAndTheBanter)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::HandToolTips, Step::TipBubble));
	EXPECT_TRUE(recorder.Before(Step::TipBubble, Step::Banter));
}

TEST(GameTurnOrder, TheAdvisorsCheerTheTurningAfterTheGuidance)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::Guidance, Step::TurnCheering));
	EXPECT_TRUE(recorder.Before(Step::TurnCheering, Step::CameraChecks));
}

TEST(GameTurnOrder, TheCreaturesTurnIsPartOfTheTurnOfTheLiving)
{
	const auto turn = PlayTurnSteps();
	for (const auto step : k_CreaturesTurn)
	{
		EXPECT_EQ(std::ranges::count(turn.Steps(), step), 0) << "a creature step is not a step of the turn alone";
	}
	TurnRecorder creatures;
	PlayCreaturesTurn([&creatures](Step step) { creatures(step); });
	ASSERT_EQ(creatures.Steps().size(), k_CreaturesTurn.size());
	EXPECT_EQ(creatures.Steps().front(), Step::CreaturePens);
	EXPECT_EQ(creatures.Steps().back(), Step::CreatureAlignment);
}

// The body's actions play on in the creature's own turn before its pose, as its sway and its fight moves do, and its
// hands take hold of and let go of things after the pose
TEST(GameTurnOrder, TheBodysActionsPlayInTheTurnAroundThePose)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::CreatureSway, Step::CreatureBodyActions));
	EXPECT_TRUE(recorder.Before(Step::CreatureBodyActions, Step::CreatureFightMoves));
	EXPECT_TRUE(recorder.Before(Step::CreaturePoses, Step::CreatureHandMoments));
	EXPECT_TRUE(recorder.Before(Step::CreatureHandMoments, Step::CreatureCarryOver));
}

// The checksum of a network game reads the world as the turn left it, before a tool's run record
TEST(GameTurnOrder, TheChecksumComesAtTheEndOfTheTurn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::GlobalStatistics, Step::SyncChecksum));
	EXPECT_TRUE(recorder.Before(Step::SyncChecksum, Step::RunRecord));
}

// The frames only draw: a creature that the turn made gets its skeleton and its first pose in the turn, before the
// checksum reads the world
TEST(GameTurnOrder, ANewCreatureGetsItsFirstPoseInTheTurn)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::DrawnPlaces, Step::CreaturesTurnStart));
	EXPECT_TRUE(recorder.Before(Step::Living, Step::CreatureShapes));
	EXPECT_TRUE(recorder.Before(Step::CreatureShapes, Step::SyncChecksum));
}

// The frames only read the map's cells: the things that the turn made go into them at the end of the turn, before the
// checksum reads the world
TEST(GameTurnOrder, TheThingsThatTheTurnMadeGoIntoTheMapAtItsEnd)
{
	const auto recorder = PlayOneTurn();
	EXPECT_TRUE(recorder.Before(Step::WalkerLookahead, Step::MapCellsAtTurnEnd));
	EXPECT_TRUE(recorder.Before(Step::MapCellsAtTurnEnd, Step::SyncChecksum));
}

TEST(GameTurnOrder, TheFullTurnMeasuresTheInfluenceFromTheTurnHands)
{
	// The players' commands and the hand's own work come before the influence of the turn, and measure as it does
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::DrawnPlaces));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::PlayerCommands));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::HandStillness));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::HandInterface));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::InfluenceTurnStart));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::Influence));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::CreatureHandMoments));
	EXPECT_TRUE(MeasuresInfluenceFromTurnHands(Step::HeldThing));
	// After the end of that part, the frames measure from the hand that this computer shows
	EXPECT_FALSE(MeasuresInfluenceFromTurnHands(Step::InfluenceTurnEnd));
	EXPECT_FALSE(MeasuresInfluenceFromTurnHands(Step::SyncChecksum));
}

TEST(GameTurnOrder, EachStepOfTheTurnBeforeItsEndMeasuresFromTheTurnHands)
{
	// The turn with a creature's own steps in the turn of the living
	const auto recorder = PlayOneTurn();
	const auto& steps = recorder.Steps();
	const auto end = std::ranges::find(steps, k_TurnHandsInfluenceEnd);
	ASSERT_NE(end, steps.end());
	EXPECT_TRUE(std::all_of(steps.begin(), end, MeasuresInfluenceFromTurnHands));
	EXPECT_TRUE(std::none_of(end, steps.end(), MeasuresInfluenceFromTurnHands));
}
