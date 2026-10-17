/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <optional>
#include <vector>

#include <DanceShapeFile.h>
#include <gtest/gtest.h>

#include "Common/GUtilsAngle.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/DanceShapes.h"

using namespace openblack;
using creature_mind::DanceOrder;
using creature_mind::Step;

namespace
{
creature_mind::Random NoRandom()
{
	return [](uint32_t) { return 0U; };
}

/// The turns a mind takes, its body free
creature_mind::Senses Free()
{
	return {.seconds = 0.1f};
}

std::vector<uint32_t> Ids(uint32_t count)
{
	std::vector<uint32_t> ids;
	for (uint32_t i = 0; i < count; ++i)
	{
		ids.push_back(i + 1);
	}
	return ids;
}
} // namespace

TEST(CreatureDance, ACreatureDancesToImpressAgainOnlyAfterAnotherKindAndTwoHundredTurns)
{
	EXPECT_FALSE(creature_dance::MayDanceToImpress(creature_dance::k_ImpressDanceType, 0, 10000));
	EXPECT_FALSE(creature_dance::MayDanceToImpress(0, 0, 200));
	EXPECT_TRUE(creature_dance::MayDanceToImpress(0, 0, 201));
	EXPECT_TRUE(creature_dance::MayDanceToImpress(1, 100, 301));
}

TEST(CreatureDance, TheDanceNeedsAClearPlaceOneAndAHalfTimesTheCreatureAndAtLeastThirtyWide)
{
	EXPECT_FLOAT_EQ(creature_dance::AreaWidth(10.0f), 30.0f);
	EXPECT_FLOAT_EQ(creature_dance::AreaWidth(30.0f), 45.0f);
}

TEST(CreatureDance, TheAgendaGoesToTheVillagerCallsThePeopleAndDancesFiveTurnsABeat)
{
	const auto agenda = creature_dance::Agenda(
	    {.villager = 7, .place = {10.0f, 20.0f}, .type = 12, .fewestDancers = 8, .beats = 90, .height = 15.0f});
	ASSERT_EQ(agenda.size(), 14U);
	EXPECT_EQ(agenda[0].kind, Step::Kind::StopMoving);
	EXPECT_EQ(agenda[1].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 75.0f);
	EXPECT_EQ(agenda[2].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[3].animation, creature_dance::k_SummonAnimation);
	EXPECT_EQ(agenda[4].dance.kind, DanceOrder::Kind::Set);
	EXPECT_EQ(agenda[4].dance.type, 12U);
	EXPECT_EQ(agenda[5].dance.kind, DanceOrder::Kind::Summon);
	EXPECT_EQ(agenda[5].dance.count, 8U);
	EXPECT_EQ(agenda[6].dance.kind, DanceOrder::Kind::StopLooking);
	// With the head still, the walk and the point give it nothing to look at
	EXPECT_FALSE(agenda[7].gaze.has_value());
	EXPECT_EQ(agenda[8].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_FALSE(agenda[8].gaze.has_value());
	EXPECT_EQ(agenda[9].dance.kind, DanceOrder::Kind::SendToPlaces);
	EXPECT_EQ(agenda[10].dance.kind, DanceOrder::Kind::WaitUntilStill);
	EXPECT_EQ(agenda[11].dance.kind, DanceOrder::Kind::Start);
	EXPECT_EQ(agenda[12].dance.kind, DanceOrder::Kind::TellToDance);
	EXPECT_EQ(agenda[13].dance.kind, DanceOrder::Kind::Dance);
	EXPECT_EQ(agenda[13].dance.count, 450U);
}

TEST(CreatureDance, TooFewFreeVillagersCallNoOne)
{
	const auto free = Ids(7);
	EXPECT_FALSE(creature_dance::Summon(free, 8, 20, [](uint32_t) { return true; }).has_value());
}

TEST(CreatureDance, ACreatureCallsAnEvenNumberOnceItHasMoreThanTwoThirdsOfTheAdults)
{
	// Ten adults: two thirds is six, so it stops at the first even number past six
	const auto free = Ids(10);
	const auto joined = creature_dance::Summon(free, 8, 10, [](uint32_t) { return true; });
	ASSERT_TRUE(joined.has_value());
	EXPECT_EQ(joined->size(), 8U);
	EXPECT_EQ(joined->front(), 1U);
}

TEST(CreatureDance, ACreatureCallsAtMostTwelve)
{
	const auto free = Ids(20);
	const auto joined = creature_dance::Summon(free, 8, 40, [](uint32_t) { return true; });
	ASSERT_TRUE(joined.has_value());
	EXPECT_EQ(joined->size(), 12U);
}

TEST(CreatureDance, OfAnOddNumberFreeACreatureCallsAllButOne)
{
	const auto free = Ids(9);
	const auto joined = creature_dance::Summon(free, 8, 40, [](uint32_t) { return true; });
	ASSERT_TRUE(joined.has_value());
	EXPECT_EQ(joined->size(), 8U);
}

TEST(CreatureDance, AVillagerThatDoesNotComeIsNotCounted)
{
	const auto free = Ids(10);
	const auto joined = creature_dance::Summon(free, 8, 40, [](uint32_t id) { return id != 3; });
	ASSERT_TRUE(joined.has_value());
	EXPECT_EQ(joined->size(), 9U);
	EXPECT_EQ(std::count(joined->begin(), joined->end(), 3U), 0);
}

TEST(CreatureDance, AWorshipperGivesItsTownBeliefByTheTownsSizeTheCreatureAndTheDance)
{
	// (10 + 0.001) / (20 + 0.001) times 2 times 0.0007
	EXPECT_NEAR(creature_dance::WorshipBelief(10.0f, 20, 2.0f, 0.0007f), 0.00070000f * 2.0f * 10.001f / 20.001f, 1e-9f);
}

TEST(CreatureDance, InItsDanceTheCreaturePlaysTheActionOfItsGroupsMove)
{
	const auto turn = creature_dance::Turn({.exists = true, .inGroup = true, .action = 35, .first = 72}, {0.0f, 0.0f});
	ASSERT_TRUE(turn.playOnce.has_value());
	EXPECT_EQ(*turn.playOnce, 72U);
	EXPECT_FALSE(turn.facePoint.has_value());
	EXPECT_FALSE(turn.walkTo.has_value());
}

TEST(CreatureDance, TheCreatureTurnsToAGameAngleOnlyWhenItFacesFarFromIt)
{
	const glm::vec2 position {100.0f, 100.0f};
	creature_mind::Senses::DanceView view {.exists = true, .inGroup = true, .action = 40, .second = 512.0f};
	const auto first = creature_dance::Turn(view, position);
	ASSERT_TRUE(first.facePoint.has_value());
	EXPECT_EQ(gutils::GetAngleFromXZ(position, *first.facePoint), 512);
	// Facing the point within the limit, it does not turn
	view.heading = creature_locomotion::HeadingOf(*first.facePoint - position) + 0.4f;
	EXPECT_FALSE(creature_dance::Turn(view, position).facePoint.has_value());
	view.heading = creature_locomotion::HeadingOf(*first.facePoint - position) + 0.5f;
	EXPECT_TRUE(creature_dance::Turn(view, position).facePoint.has_value());
}

TEST(CreatureDance, TheCreatureCutsTheDanceAngleToWholeRadians)
{
	const glm::vec2 position {100.0f, 100.0f};
	const creature_mind::Senses::DanceView view {.exists = true, .inGroup = true, .action = 33, .second = 1.5708f};
	const auto turn = creature_dance::Turn(view, position);
	ASSERT_TRUE(turn.facePoint.has_value());
	// 1.5708 radians is cut to 1 radian; the point 50 metres away gives the angle back to within one step
	EXPECT_NEAR(static_cast<double>(gutils::GetAngleFromXZ(position, *turn.facePoint)),
	            static_cast<double>(gutils::ConvertAngle3DToGame(1.0f)), 1.0);
}

TEST(CreatureDance, TheCreatureWalksBackToItsPlaceWhenMoreThanAFifthOfAMetreAway)
{
	creature_mind::Senses::DanceView view {.exists = true, .inGroup = true, .place = {3.0f, 4.0f}, .placeDistance = 0.2f};
	EXPECT_FALSE(creature_dance::Turn(view, {0.0f, 0.0f}).walkTo.has_value());
	view.placeDistance = 0.21f;
	const auto turn = creature_dance::Turn(view, {0.0f, 0.0f});
	ASSERT_TRUE(turn.walkTo.has_value());
	EXPECT_EQ(*turn.walkTo, glm::vec2(3.0f, 4.0f));
}

TEST(CreatureDance, TheMusicIsTheTribesChantWithVoicesForMoreThanEightDancers)
{
	EXPECT_EQ(creature_dance::MusicType(0, 8), 28);
	EXPECT_EQ(creature_dance::MusicType(0, 9), 29);
	// The african tribe dances to the celtic chant
	EXPECT_EQ(creature_dance::MusicType(1, 2), 28);
	EXPECT_EQ(creature_dance::MusicType(8, 9), 43);
	EXPECT_EQ(creature_dance::MusicType(9, 9), 5);
}

TEST(CreatureDance, AStopStartsWhileTheBodyMovesAndEndsOnceItPlaysNoAction)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, {{.kind = Step::Kind::StopMoving}, {.kind = Step::Kind::Wait}});
	auto senses = Free();
	senses.bodyBusy = true;
	senses.moving = true;
	senses.bodyPlaying = true;
	const auto commands = creature_mind::Think(mind, senses, NoRandom());
	EXPECT_TRUE(commands.stopMoving);
	EXPECT_TRUE(mind.stepStarted);
	static_cast<void>(creature_mind::Think(mind, senses, NoRandom()));
	EXPECT_EQ(mind.step, 0U);
	senses.bodyPlaying = false;
	static_cast<void>(creature_mind::Think(mind, senses, NoRandom()));
	EXPECT_EQ(mind.step, 1U);
}

TEST(CreatureDance, TheLastDanceGoesTheTurnBeforeTheNewOneComes)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::Set, .type = 12}}});
	const auto first = creature_mind::Think(mind, Free(), NoRandom());
	ASSERT_TRUE(first.dance.has_value());
	EXPECT_EQ(first.dance->kind, DanceOrder::Kind::Clear);
	const auto second = creature_mind::Think(mind, Free(), NoRandom());
	ASSERT_TRUE(second.dance.has_value());
	EXPECT_EQ(second.dance->kind, DanceOrder::Kind::Set);
	creature_mind::DanceOrderDone(mind, true);
	EXPECT_EQ(mind.step, 1U);
}

TEST(CreatureDance, AStageThatFailsGivesUpTheAgenda)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(
	    mind, creature_mind::Activity::Planned,
	    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::Summon, .count = 8}}, {.kind = Step::Kind::Wait}});
	const auto commands = creature_mind::Think(mind, Free(), NoRandom());
	ASSERT_TRUE(commands.dance.has_value());
	EXPECT_EQ(commands.dance->kind, DanceOrder::Kind::Summon);
	creature_mind::DanceOrderDone(mind, false);
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_EQ(mind.step, mind.agenda.size());
}

TEST(CreatureDance, TheCreatureDancesItsTurnsThenStopsTheDance)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::Dance, .count = 3}}});
	auto senses = Free();
	senses.dance = creature_mind::Senses::DanceView {.exists = true, .inGroup = true};
	// The turn it joins the dance, then its three turns of dancing
	EXPECT_EQ(creature_mind::Think(mind, senses, NoRandom()).dance->kind, DanceOrder::Kind::Dance);
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_FALSE(creature_mind::Think(mind, senses, NoRandom()).dance.has_value());
	}
	const auto last = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(last.dance.has_value());
	EXPECT_EQ(last.dance->kind, DanceOrder::Kind::Stop);
	EXPECT_EQ(mind.step, 1U);
}

// The dance move does not start the dance while the creature walks or turns, only when it stands still
TEST(CreatureDance, TheDanceMoveWaitsUntilTheCreatureStandsStill)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::Dance, .count = 10}}});
	auto senses = Free();
	senses.dance = creature_mind::Senses::DanceView {.exists = true, .inGroup = true, .action = 34, .danceRate = 0.8f};
	// The turn it joins the dance
	static_cast<void>(creature_mind::Think(mind, senses, NoRandom()));
	auto walking = senses;
	walking.moving = true;
	walking.bodyBusy = true;
	EXPECT_FALSE(creature_mind::Think(mind, walking, NoRandom()).startDance.has_value());
	const auto still = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(still.startDance.has_value());
	EXPECT_FLOAT_EQ(*still.startDance, 800.0f);
}

TEST(CreatureDance, ItWaitsUntilNoDancerMovesAndGivesUpWithoutADance)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(
	    mind, creature_mind::Activity::Planned,
	    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::WaitUntilStill}}, {.kind = Step::Kind::Wait}});
	auto senses = Free();
	senses.dance = creature_mind::Senses::DanceView {.exists = true, .dancersMoving = true};
	static_cast<void>(creature_mind::Think(mind, senses, NoRandom()));
	EXPECT_EQ(mind.step, 0U);
	senses.dance->dancersMoving = false;
	static_cast<void>(creature_mind::Think(mind, senses, NoRandom()));
	EXPECT_EQ(mind.step, 1U);

	creature_mind::IdleMind gone;
	creature_mind::Plan(gone, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::WaitUntilStill}}});
	senses.dance->exists = false;
	static_cast<void>(creature_mind::Think(gone, senses, NoRandom()));
	EXPECT_TRUE(gone.gaveUp);
}

TEST(CreatureDance, AShapeFileHoldsItsPointsInOrder)
{
	std::vector<uint8_t> bytes {2, 0, 0, 0};
	for (const int32_t value : {65535, 0, -100, 200})
	{
		for (int i = 0; i < 4; ++i)
		{
			bytes.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * i)));
		}
	}
	dance::DanceShapeFile file;
	ASSERT_EQ(file.Open(bytes), dance::DanceResult::Success);
	ASSERT_EQ(file.points.size(), 2U);
	EXPECT_EQ(file.points[1].x, -100);
	EXPECT_EQ(file.points[1].z, 200);
	bytes.push_back(0);
	EXPECT_EQ(file.Open(bytes), dance::DanceResult::ErrTrailingBytes);
	const std::vector<glm::ivec2> points {{65535, 0}, {-100, 200}};
	const auto shape = ecs::dance_shapes::FromPoints(points);
	EXPECT_EQ(shape[1], glm::ivec2(-100, 200));
	EXPECT_EQ(shape[2], glm::ivec2(0, 0));
}

TEST(CreatureDance, ThePlayfulDanceAndTheStoryComeAgainOnlyAfterAnotherKindAndTheirTurns)
{
	using creature_dance::MayDanceAgain;
	EXPECT_FALSE(MayDanceAgain(creature_dance::k_PlayfulDanceType, creature_dance::k_PlayfulDanceType, 0, 10000));
	EXPECT_FALSE(MayDanceAgain(creature_dance::k_PlayfulDanceType, 0, 0, 300));
	EXPECT_TRUE(MayDanceAgain(creature_dance::k_PlayfulDanceType, 0, 0, 301));
	EXPECT_FALSE(MayDanceAgain(creature_dance::k_StoryDanceType, creature_dance::k_ImpressDanceType, 0, 450));
	EXPECT_TRUE(MayDanceAgain(creature_dance::k_StoryDanceType, creature_dance::k_ImpressDanceType, 0, 451));
	// The game, the watched dance and the dance of love are always possible, and the creature does not note them
	EXPECT_TRUE(MayDanceAgain(creature_dance::k_GameDanceType, creature_dance::k_GameDanceType, 0, 1));
	EXPECT_FALSE(creature_dance::TurnsBetween(creature_dance::k_AmorousDanceType).has_value());
	EXPECT_FALSE(creature_dance::TurnsBetween(creature_dance::k_WatchedDanceType).has_value());
}

TEST(CreatureDance, TheDanceMoveDancesAtTheRateOfTheDanceAndKeepsAPose)
{
	const auto turn = creature_dance::Turn({.exists = true, .inGroup = true, .action = 34, .danceRate = 0.8f}, {0.0f, 0.0f});
	ASSERT_TRUE(turn.danceMoveMs.has_value());
	EXPECT_FLOAT_EQ(*turn.danceMoveMs, 800.0f);
	EXPECT_TRUE(turn.endPose);
	EXPECT_FALSE(turn.endDance);
}

TEST(CreatureDance, ThePoseMoveStartsAPoseAndStopsTheDance)
{
	const auto turn = creature_dance::Turn({.exists = true, .inGroup = true, .action = 36, .first = 38}, {0.0f, 0.0f});
	EXPECT_EQ(turn.pose, std::optional<size_t> {38});
	EXPECT_FALSE(turn.endPose);
	EXPECT_TRUE(turn.endDance);
}

TEST(CreatureDance, TheFaceMoveHoldsAFaceForTwoSecondsAndKeepsTheDance)
{
	const auto turn = creature_dance::Turn({.exists = true, .inGroup = true, .action = 38, .first = 24}, {0.0f, 0.0f});
	EXPECT_EQ(turn.face, std::optional<size_t> {24});
	EXPECT_FLOAT_EQ(turn.faceMs, 2000.0f);
	EXPECT_TRUE(turn.endPose);
	EXPECT_FALSE(turn.endDance);
}

TEST(CreatureDance, TheTalkMoveTalksAndStopsThePoseAndTheDance)
{
	const auto turn = creature_dance::Turn({.exists = true, .inGroup = true, .action = 41}, {0.0f, 0.0f});
	EXPECT_EQ(turn.overlay, std::optional<size_t> {205});
	EXPECT_TRUE(turn.endPose);
	EXPECT_TRUE(turn.endDance);
}

TEST(CreatureDance, ADanceAloneWaitsForTheBodyThenDancesForTwentySeconds)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, creature_dance::PracticeDance());
	auto busy = Free();
	busy.bodyPlaying = true;
	busy.bodyBusy = true;
	EXPECT_FALSE(creature_mind::Think(mind, busy, NoRandom()).startDance.has_value());
	// The creature can still move as it starts to dance
	auto moving = Free();
	moving.bodyBusy = true;
	moving.moving = true;
	const auto start = creature_mind::Think(mind, moving, NoRandom());
	ASSERT_TRUE(start.startDance.has_value());
	EXPECT_FLOAT_EQ(*start.startDance, 1000.0f);
	auto dancing = Free();
	dancing.bodyDancing = true;
	dancing.bodyPlaying = true;
	dancing.bodyBusy = true;
	for (int i = 0; i < 199; ++i)
	{
		EXPECT_FALSE(creature_mind::Think(mind, dancing, NoRandom()).endDance);
		EXPECT_EQ(mind.step, 0U);
	}
	EXPECT_TRUE(creature_mind::Think(mind, dancing, NoRandom()).endDance);
	EXPECT_EQ(mind.step, 1U);
}

TEST(CreatureDance, ADanceThatNoStepWantsStops)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, {{.kind = Step::Kind::Wait, .seconds = 1.0f}});
	auto senses = Free();
	senses.bodyDancing = true;
	EXPECT_TRUE(creature_mind::Think(mind, senses, NoRandom()).endDance);
}

// The gesture move draws the gesture of its first argument; the moves other than the gesture and the turn to an angle
// stop the arm that points
TEST(CreatureDance, TheGestureMoveDrawsItsGestureAndOtherMovesStopThePoint)
{
	const auto gesture = creature_dance::Turn({.exists = true, .inGroup = true, .action = 39, .first = 1}, {0.0f, 0.0f});
	ASSERT_TRUE(gesture.gesture.has_value());
	EXPECT_EQ(*gesture.gesture, 1U);
	EXPECT_FALSE(gesture.endPoint);
	EXPECT_TRUE(gesture.endDance);
	EXPECT_TRUE(gesture.endPose);
	const auto faceAngle = creature_dance::Turn({.exists = true, .inGroup = true, .action = 40}, {0.0f, 0.0f});
	EXPECT_FALSE(faceAngle.endPoint);
	const auto dance = creature_dance::Turn({.exists = true, .inGroup = true, .action = 34}, {0.0f, 0.0f});
	EXPECT_TRUE(dance.endPoint);
	EXPECT_FALSE(dance.gesture.has_value());
}
