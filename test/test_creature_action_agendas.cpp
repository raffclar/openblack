/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t AlwaysFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t NeverFirst(uint32_t range)
{
	return range - 1;
}
} // namespace

// "force MyCreature CREATURE_HURL TinyStone with Trainer": with its hand empty it may show its anger, picks up the trainer,
// goes to where it can throw at the stone and throws the trainer at it
TEST(CreatureActionAgendas, HurlingPicksUpWhatToThrowThenThrowsItAtTheThing)
{
	const auto agenda = HurlAt(4, 9, {10.0f, 20.0f}, 15.0f, false, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, animations::k_Angry);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[3].order.point, glm::vec2(10.0f, 20.0f));
	// The throw finishes the action, which sees to whichever desire it served
	EXPECT_EQ(agenda[3].effect, Effect::None);
	ASSERT_TRUE(agenda[3].gaze.has_value());
	EXPECT_EQ(agenda[3].gaze->object, 9u);
	// Most of the time it doesn't show its anger first; with something in its hand already it throws that
	EXPECT_EQ(HurlAt(4, 9, {}, 15.0f, false, NeverFirst).size(), 3);
	const auto holding = HurlAt(4, 9, {}, 15.0f, true, AlwaysFirst);
	ASSERT_EQ(holding.size(), 2);
	EXPECT_EQ(holding[0].movement.kind, Movement::Kind::ToThrowPosition);
}

// A hurl, a script's or the creature's own, is made from the thing to throw it is given
TEST(CreatureActionAgendas, AScriptsHurlNeedsNoPlaceOfItsOwn)
{
	const auto* hurl = plan_actions::For("Hurl");
	ASSERT_NE(hurl, nullptr);
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Agenda(*hurl, 9, {1.0f, 2.0f}, situation, NeverFirst).has_value());
	situation.instrument = 4;
	situation.height = 15.0f;
	const auto agenda = plan_actions::Agenda(*hurl, 9, {1.0f, 2.0f}, situation, NeverFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->front().order.object, 4u);
	EXPECT_EQ(agenda->back().order.point, glm::vec2(1.0f, 2.0f));
}

// "force Guide CREATURE_SLEEP Guide": the guide goes home to sleep
TEST(CreatureActionAgendas, SleepingAtHomeWalksHomeFacesDownTheSlopeAndSleepsUntilRested)
{
	const auto agenda = SleepAtHome({100.0f, 200.0f}, 30.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[0].sleepyEyes);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(100.0f, 200.0f));
	// No further than 5 from home, however tall it is
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Static);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_TRUE(agenda[3].closedEyes);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	EXPECT_FLOAT_EQ(SleepAtHome({}, 3.0f, NeverFirst).front().movement.maxDistance, 3.0f);
	// Without a home it can't
	const auto* sleep = plan_actions::For("SleepAtHome");
	ASSERT_NE(sleep, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*sleep, {}));
}

// "force MyCreature CREATURE_EXAMINE_BY_LOOKING Guide"
TEST(CreatureActionAgendas, LookingSomethingOverGoesNearItFacesItAndWatchesItsFoot)
{
	const auto agenda = ExamineByLooking(3, 10.0f, 40.0f);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	// The thing's height when it is taller than two and a half of its own
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 40.0f);
	EXPECT_FLOAT_EQ(ExamineByLooking(3, 10.0f, 5.0f)[0].movement.maxDistance, 25.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 2.0f);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 1.1f);
	ASSERT_TRUE(agenda[2].gaze.has_value());
	EXPECT_TRUE(agenda[2].gaze->bottom);
	// Then it takes in what kind of thing it is
	EXPECT_EQ(agenda[3].kind, Step::Kind::Observe);
	EXPECT_EQ(agenda[3].object, 3u);
}

// Taking in what a thing is happens at once, as the step starts, and says which thing it was
TEST(CreatureActionAgendas, TakingInAThingIsDoneAtOnce)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Observe, .object = 3}});
	const auto commands = Think(mind, {.seconds = 0.1f, .choosesNext = false}, NeverFirst);
	EXPECT_EQ(commands.effect, Effect::Observed);
	EXPECT_EQ(commands.effectObject, 3u);
	EXPECT_EQ(mind.step, 1u);
}

// A kind of thing it has looked over is a hundredth as interesting, unless the thing is up to something interesting
TEST(CreatureActionAgendas, LookedOverKindsAreFarLessInteresting)
{
	EXPECT_FLOAT_EQ(CuriosityShare(false, false), 1.0f);
	EXPECT_FLOAT_EQ(CuriosityShare(true, false), 0.01f);
	EXPECT_FLOAT_EQ(CuriosityShare(true, true), 1.0f);
}

// "force Guide CREATURE_SMILE_AT_FRIEND MyCreature" and "force MyCreature CREATURE_WAVE_AT_OBJECT Guide"
TEST(CreatureActionAgendas, SmilingAndWavingGoNearAndFaceTheOther)
{
	const auto smile = SmileAt(5, 10.0f, 0.5f, AlwaysFirst);
	ASSERT_EQ(smile.size(), 3);
	EXPECT_FLOAT_EQ(smile[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(smile[0].face, creature_face::Cue::Smile);
	EXPECT_FLOAT_EQ(smile[1].seconds, 6.0f);
	EXPECT_EQ(smile[2].animation, animations::k_Happy);
	EXPECT_EQ(SmileAt(5, 10.0f, 0.0f, NeverFirst).size(), 2);

	const auto wave = WaveAt(5, 10.0f);
	ASSERT_EQ(wave.size(), 3);
	EXPECT_FLOAT_EQ(wave[0].movement.maxDistance, 35.0f);
	EXPECT_FLOAT_EQ(wave[1].seconds, 1.4f);
	EXPECT_EQ(wave[2].animation, animations::k_FriendlyWave);
}

TEST(CreatureActionAgendas, FrightenedPuttingDownAndLyingDead)
{
	const auto frightened = BeFrightenedOnTheSpot();
	ASSERT_EQ(frightened.size(), 3);
	EXPECT_EQ(frightened[0].animation, animations::k_Frightened);
	EXPECT_FLOAT_EQ(frightened[1].seconds, 3.0f);
	EXPECT_EQ(frightened[1].face, creature_face::Cue::Frightened);
	EXPECT_EQ(frightened[2].animation, animations::k_Frightened);

	const auto putDown = PutDown();
	ASSERT_EQ(putDown.size(), 1);
	EXPECT_EQ(putDown[0].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(putDown[0].order.animation, creature_throw::k_PutDown);

	const auto dead = DeadForever();
	ASSERT_EQ(dead.size(), 1);
	EXPECT_EQ(dead[0].kind, Step::Kind::Static);
	EXPECT_TRUE(dead[0].holdLoop);
	EXPECT_FLOAT_EQ(dead[0].seconds, 864000.0f);
	EXPECT_EQ(dead[0].effect, Effect::None);
}

// The actions Land 1's scripts force have an agenda, a casting one once it knows what it casts
TEST(CreatureActionAgendas, TheForcedActionsOfLandOneCanBeCarriedOut)
{
	// Holding a thing is the leash's orders' own agenda
	for (const auto* name : {"Hurl", "SleepAtHome", "ExamineByLooking", "SmileAtFriend", "PutDown", "WaveAtObject",
	                         "CastHealSpellPU1", "BeIdle", "BeFrightenedOnTheSpot", "DeadForever"})
	{
		SCOPED_TRACE(name);
		EXPECT_NE(plan_actions::For(name), nullptr);
	}
	const auto* heal = plan_actions::For("CastHealSpellPU1");
	ASSERT_NE(heal, nullptr);
	EXPECT_TRUE(plan_actions::IsCast(*heal));
	EXPECT_FALSE(plan_actions::Agenda(*heal, 3, {}, {}, NeverFirst).has_value());
	EXPECT_TRUE(plan_actions::Agenda(*heal, 3, {}, {}, NeverFirst, plan_actions::CastInfo {.magicType = 1, .height = 15.0f})
	                .has_value());
}

// A lesson's DEV_FUNCTION 6 has the creature point out the nearest highlight
TEST(CreatureActionAgendas, PointingOutAHighlightCallsAFarCameraFirst)
{
	const auto near = PointOutHighlight(8, 20.0f);
	ASSERT_EQ(near.size(), 2);
	EXPECT_EQ(near[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(near[0].order.object, 8u);
	EXPECT_EQ(near[0].order.seconds, 1.0f);
	EXPECT_EQ(near[0].face, creature_face::Cue::Amazed);
	EXPECT_FLOAT_EQ(near[1].seconds, 2.0f);
	ASSERT_TRUE(near[1].gaze.has_value());
	EXPECT_TRUE(near[1].gaze->camera);

	const auto far = PointOutHighlight(8, 31.0f);
	ASSERT_EQ(far.size(), 4);
	EXPECT_FLOAT_EQ(far[0].seconds, 1.0f);
	EXPECT_EQ(far[0].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(far[1].animation, 69u);
	// Without a camera to show it to, it can't
	const auto* point = plan_actions::For("PointOutHighlight");
	ASSERT_NE(point, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*point, {}));
}

// "force CreatureGuardian CREATURE_SLEEP_BY_OBJECT CreatureGuardian": it walks to the clear place it found until within
// its own radius, may yawn, faces down the slope, sleeps until rested and gets up with a grimace
TEST(CreatureActionAgendas, SleepingByAThingGoesToItsClearPlaceFirst)
{
	const auto agenda = SleepByObject({30.0f, 40.0f}, 6.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(30.0f, 40.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 6.0f);
	EXPECT_EQ(agenda[1].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[1].sleepyEyes);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	// Half the time without the yawn
	EXPECT_EQ(SleepByObject({}, 6.0f, NeverFirst).size(), 4);
}

// Only scripts make it sleep by a thing for now; without a clear place it sleeps at home, and without either it can't
TEST(CreatureActionAgendas, SleepingByAThingFallsBackToHome)
{
	const auto* sleep = plan_actions::For("SleepByObject");
	ASSERT_NE(sleep, nullptr);
	plan_actions::Situation situation {.controlledByScript = true, .radius = 6.0f, .height = 15.0f};
	EXPECT_FALSE(plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst).has_value());
	situation.home = glm::vec2(100.0f, 200.0f);
	const auto atHome = plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst);
	ASSERT_TRUE(atHome.has_value());
	EXPECT_EQ(atHome->front().movement.point, glm::vec2(100.0f, 200.0f));
	situation.clearArea = glm::vec2(10.0f, 20.0f);
	const auto byThing = plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst);
	ASSERT_TRUE(byThing.has_value());
	EXPECT_EQ(byThing->front().movement.point, glm::vec2(10.0f, 20.0f));
	situation.controlledByScript = false;
	EXPECT_FALSE(plan_actions::Possible(*sleep, situation));
}

// "force CreatureGuardian CREATURE_EAT_FROM_MAGIC_FOOD_PILE FoodFound": it walks up to the pile, faces it, takes a
// handful of food from it and eats that, leaving the pile where it is
TEST(CreatureActionAgendas, EatingFromAPileTakesAHandfulFromIt)
{
	const auto* eat = plan_actions::For("EatFromFoodPile");
	ASSERT_NE(eat, nullptr);
	EXPECT_EQ(eat->target, plan_actions::Target::MagicFoodPile);
	const auto agenda = plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, {.radius = 6.0f, .thingRadius = 4.0f}, NeverFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ((*agenda)[0].movement.point, glm::vec2(50.0f, 60.0f));
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 12.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_FLOAT_EQ((*agenda)[1].seconds, 0.1f);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[2].order.object, 9u);
	EXPECT_EQ((*agenda)[2].effect, Effect::TookFromPile);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ((*agenda)[3].effect, Effect::Eat);
}

// Throwing a ball at something throws a toy it can pick up; kicking one about is only done to a ball
TEST(CreatureActionAgendas, BallGamesArePlayedWithToys)
{
	const auto* throwBall = plan_actions::For("ThrowBallAtObject");
	ASSERT_NE(throwBall, nullptr);
	EXPECT_EQ(throwBall->target, plan_actions::Target::Anything);
	EXPECT_EQ(throwBall->use, plan_actions::Target::Toy);
	const auto* kick = plan_actions::For("KickBallAround");
	ASSERT_NE(kick, nullptr);
	EXPECT_EQ(kick->target, plan_actions::Target::ToyBall);
}
