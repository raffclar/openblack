/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureRestAndHome.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
namespace actions = openblack::creature_plan_actions;

namespace
{
constexpr float k_Turn = 0.1f;

Random Draws(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0 : value % range; };
}

/// A body that plays what the mind tells it: a sequence loops until told to end, then ends at once
struct RestingBody
{
	bool looping {false};
	bool better {false};

	[[nodiscard]] Senses SensesNow() const
	{
		return {.seconds = k_Turn, .bodyBusy = looping, .bodyLooping = looping, .better = better};
	}

	void Obey(const Commands& commands)
	{
		if (commands.endSit)
		{
			looping = false;
		}
		if (commands.startSequence.has_value())
		{
			looping = true;
		}
	}
};

actions::Situation AtHome(float homeDistance)
{
	actions::Situation situation;
	situation.home = glm::vec2(100.0f, 0.0f);
	situation.homeDistance = homeDistance;
	situation.height = 10.0f;
	situation.self = 7;
	return situation;
}

const actions::Executor& ExecutorFor(std::string_view action)
{
	const auto* executor = actions::For(action);
	EXPECT_NE(executor, nullptr) << action;
	return *executor;
}
} // namespace

TEST(CreatureRestAndHome, AnotherCreatureNearerThanEightHeightsIsFrightening)
{
	const std::array<float, 2> far {80.0f, 200.0f};
	const std::array<float, 2> near {79.9f, 200.0f};
	EXPECT_TRUE(NothingFrighteningNear(10.0f, far));
	EXPECT_FALSE(NothingFrighteningNear(10.0f, near));
	EXPECT_TRUE(NothingFrighteningNear(10.0f, {}));
}

TEST(CreatureRestAndHome, HomeDistancesDecideGoingHomeAndPraying)
{
	EXPECT_FALSE(FarFromHome(20.0f));
	EXPECT_TRUE(FarFromHome(20.1f));
	EXPECT_TRUE(NotTooFarFromHome(true, 999.0f));
	EXPECT_FALSE(NotTooFarFromHome(true, 1000.0f));
	EXPECT_FALSE(NotTooFarFromHome(false, 10.0f));
}

TEST(CreatureRestAndHome, ItIsBetterOnlyAtFullLifeAndNoTiredness)
{
	EXPECT_TRUE(IsBetter(1.0f, 0.0f));
	EXPECT_FALSE(IsBetter(0.999f, 0.0f));
	EXPECT_FALSE(IsBetter(1.0f, 0.001f));
}

TEST(CreatureRestAndHome, RestingToGetBetterFacesDownTheSlopeRestsAndGetsUpDazed)
{
	const auto agenda = RestToGetBetter();
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Static);
	EXPECT_EQ(agenda[1].sequence,
	          (std::array<size_t, 3> {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}));
	EXPECT_TRUE(agenda[1].untilBetter);
	EXPECT_EQ(agenda[1].effect, Effect::Completed);
	EXPECT_EQ(agenda[2].animation, animations::k_Confused);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Grimace);
}

TEST(CreatureRestAndHome, GoingHomeToRecoverWalksHomeWithinItsHeightButNoMoreThanFive)
{
	const auto agenda = GoHomeToRecover({3.0f, 4.0f}, 12.0f);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 5.0f);
	EXPECT_EQ(GoHomeToRecover({}, 2.0f)[0].movement.maxDistance, 2.0f);
	EXPECT_TRUE(agenda[2].untilBetter);
}

TEST(CreatureRestAndHome, TheRestEndsOnlyWhenBetterAndAfterThirtyTurns)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, RestToGetBetter());
	RestingBody body;
	// The turn to face down the slope, then the turn that the rest starts
	auto commands = Think(mind, body.SensesNow(), Draws(1));
	body.Obey(commands);
	int startTurn = -1;
	for (int turn = 0; turn < 5 && startTurn < 0; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(1));
		body.Obey(commands);
		if (commands.startSequence.has_value())
		{
			startTurn = turn;
			EXPECT_EQ(commands.eyes, Eyes::Closed);
			EXPECT_TRUE(commands.mirrored);
		}
	}
	ASSERT_GE(startTurn, 0);
	EXPECT_TRUE(IsResting(mind));
	EXPECT_FALSE(IsAsleep(mind));
	// Better at once, it still rests to its thirty-first turn
	body.better = true;
	int endTurn = 0;
	for (int turn = 2; turn < 100; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(1));
		EXPECT_TRUE(commands.coolDown);
		if (commands.endSit)
		{
			endTurn = turn;
			// The action counts as done only once it is up
			EXPECT_EQ(commands.effect, Effect::None);
			body.Obey(commands);
			break;
		}
		body.Obey(commands);
	}
	EXPECT_EQ(endTurn, 31);
	EXPECT_FALSE(IsResting(mind));
	commands = Think(mind, body.SensesNow(), Draws(1));
	EXPECT_EQ(commands.effect, Effect::Completed);
	EXPECT_FALSE(commands.coolDown);
}

TEST(CreatureRestAndHome, ANotBetterCreatureRestsOn)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {RestStep()});
	RestingBody body;
	auto commands = Think(mind, body.SensesNow(), Draws(0));
	body.Obey(commands);
	for (int turn = 0; turn < 1000; ++turn)
	{
		commands = Think(mind, body.SensesNow(), Draws(0));
		EXPECT_FALSE(commands.endSit);
		body.Obey(commands);
	}
	EXPECT_TRUE(IsResting(mind));
}

TEST(CreatureRestAndHome, HealingItselfCastsTheHealWhereItStandsThenHeals)
{
	const glm::vec3 place {1.0f, 2.0f, 3.0f};
	const auto agenda = HealItself(static_cast<uint32_t>(MagicType::Heal), place);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[0].sequence, k_CastPose);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 3.0f);
	EXPECT_EQ(agenda[0].cast.magicType, static_cast<uint32_t>(MagicType::Heal));
	EXPECT_EQ(agenda[0].cast.place, place);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Grimace);
	EXPECT_EQ(agenda[1].kind, Step::Kind::HealItself);
	EXPECT_EQ(HandsNeededFor(agenda[1]), HandsNeed::Free);

	IdleMind mind;
	Plan(mind, Activity::Planned, {agenda[1]});
	const auto commands = Think(mind, {.seconds = k_Turn}, Draws(0));
	EXPECT_TRUE(commands.healItself);
}

TEST(CreatureRestAndHome, RestingOnTheSpotWaitsExhaustedLookingAtItsFeet)
{
	const auto agenda = RestOnTheSpot(7, 5.5f);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 5.5f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Exhausted);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, 7u);
	EXPECT_TRUE(agenda[0].gaze->bottom);
}

TEST(CreatureRestAndHome, TheGlobesThatRestoreHealthAreCastAtItself)
{
	const auto inHand = CastRestoreHealthInHand(10, 7);
	ASSERT_EQ(inHand.size(), 2u);
	EXPECT_EQ(inHand[0].kind, Step::Kind::UseUpHeld);
	EXPECT_EQ(inHand[1].cast.object, 7u);
	EXPECT_EQ(inHand[1].face, creature_face::Cue::Grimace);

	const auto picked = PickUpAndCastRestoreHealth(10, 7, 9, true, 12.0f);
	ASSERT_EQ(picked.size(), 5u);
	EXPECT_EQ(picked[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(picked[1].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(picked[1].seconds, 12.0f);
	EXPECT_EQ(picked[4].kind, Step::Kind::UseUp);
	EXPECT_EQ(picked[4].object, 9u);
	// With its hand full it does not pick the globe up, but the rest is the same
	EXPECT_EQ(PickUpAndCastRestoreHealth(10, 7, 9, false, 12.0f).size(), 4u);
}

TEST(CreatureRestAndHome, RunningHomeRunsToTwoHeightsAfterAFrightHalfTheTime)
{
	const auto frightened = RunHome({5.0f, 6.0f}, 10.0f, true);
	ASSERT_EQ(frightened.size(), 2u);
	EXPECT_EQ(frightened[0].animation, animations::k_Frightened);
	EXPECT_EQ(frightened[0].heldEyes, Eyes::Wide);
	EXPECT_TRUE(frightened[1].movement.run);
	EXPECT_FLOAT_EQ(frightened[1].movement.maxDistance, 20.0f);
	EXPECT_EQ(RunHome({}, 10.0f, false).size(), 1u);
}

TEST(CreatureRestAndHome, GoingIntoTheTempleStopsShortOfTheHeart)
{
	// 100 away on the ground, heart radius 10: it stops 1.3 x 3.3 short
	const auto stop = TempleStop({0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_NEAR(stop.x, 100.0f - (1.3f * 3.3f), 1e-4f);
	EXPECT_NEAR(stop.y, 0.0f, 1e-6f);
	const auto agenda = EnterTemple({50.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, stop, 10.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 10.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Smile);
	EXPECT_EQ(agenda[2].movement.point, stop);
}

TEST(CreatureRestAndHome, PrayingCountsAsDoneOnArrivalThenPrays)
{
	const auto agenda = PrayAtTemple({1.0f, 2.0f}, 8.0f);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].effect, Effect::Completed);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 8.0f);
	EXPECT_EQ(agenda[1].animation, k_PrayAnimation);
}

TEST(CreatureRestAndHome, LookingAtTheTemplePointsOneTimeInThree)
{
	const auto pointing = LookAtTemple(4, true);
	ASSERT_EQ(pointing.size(), 5u);
	EXPECT_EQ(pointing[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(pointing[0].seconds, 2.0f);
	EXPECT_FLOAT_EQ(pointing[1].seconds, 3.0f);
	EXPECT_EQ(pointing[2].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_TRUE(pointing[3].gaze->bottom);
	EXPECT_FLOAT_EQ(pointing[4].seconds, 2.0f);
	EXPECT_EQ(LookAtTemple(4, false).size(), 4u);
}

TEST(CreatureRestAndHome, ThePlannerHasTheVanillaPreconditions)
{
	// Go home only when more than 20 away
	EXPECT_FALSE(actions::Possible(ExecutorFor("GoHome"), AtHome(20.0f)));
	EXPECT_TRUE(actions::Possible(ExecutorFor("GoHome"), AtHome(21.0f)));
	// Pray and sleep at home only with a temple and within 1000
	auto withTemple = AtHome(500.0f);
	withTemple.hasTemple = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("PrayAtCitadel"), withTemple));
	EXPECT_TRUE(actions::Possible(ExecutorFor("SleepAtHome"), withTemple));
	EXPECT_FALSE(actions::Possible(ExecutorFor("PrayAtCitadel"), AtHome(500.0f)));
	withTemple.homeDistance = 1000.0f;
	EXPECT_FALSE(actions::Possible(ExecutorFor("SleepAtHome"), withTemple));
	// Rest only with nothing frightening near
	auto frightened = AtHome(0.0f);
	frightened.nothingFrighteningNear = false;
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestToGetBetter"), frightened));
	EXPECT_FALSE(actions::Possible(ExecutorFor("RestOnTheSpot"), frightened));
	EXPECT_TRUE(actions::Possible(ExecutorFor("GoHomeToRecover"), frightened));
	// Cast the globe in its hand only when it restores health
	auto holding = AtHome(0.0f);
	holding.heldOneShot = actions::OneShot {.magicType = 10, .compassionate = true};
	EXPECT_FALSE(actions::Possible(ExecutorFor("CastOneOffSpellInHandToRestoreHe"), holding));
	holding.heldOneShot->restoresHealth = true;
	EXPECT_TRUE(actions::Possible(ExecutorFor("CastOneOffSpellInHandToRestoreHe"), holding));
	// Healing itself is a cast
	EXPECT_TRUE(actions::IsCast(ExecutorFor("HealHimself")));
}

TEST(CreatureRestAndHome, EnteringTheTempleFailsWithoutAHeart)
{
	const auto& enter = ExecutorFor("EnterCitadel");
	auto situation = AtHome(50.0f);
	EXPECT_FALSE(actions::Agenda(enter, std::nullopt, {}, situation, Draws(0)).has_value());
	situation.templeHeart = actions::Situation::TempleHeart {.temple = 3, .point = {100.0f, 0.0f, 0.0f}, .radius = 10.0f};
	EXPECT_TRUE(actions::Agenda(enter, std::nullopt, {}, situation, Draws(0)).has_value());
}
