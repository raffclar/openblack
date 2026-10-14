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
