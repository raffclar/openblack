/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace cast_moves = openblack::creature_cast_moves;

namespace
{
Random Always(uint32_t value)
{
	return [value](uint32_t range) { return std::min(value, range - 1); };
}
} // namespace

TEST(CreatureCastAgenda, LightningFromFiftyBackingOffToTenMoreThanItsHeight)
{
	const auto agenda = CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1));
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 25.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 0.1f);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[3].cast.magicType, 4u);
	EXPECT_EQ(*agenda[3].cast.object, 7u);
	EXPECT_EQ(agenda[3].sequence, (std::array<size_t, 3> {40, 41, 42}));
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.0f);
}

TEST(CreatureCastAgenda, OneTimeInFiveItShowsHowItFeelsFirst)
{
	const auto playful = CastAt(CastStyle::Playful, 26, 0, 7, 10.0f, Always(0));
	ASSERT_EQ(playful.size(), 5u);
	EXPECT_EQ(playful[0].kind, Step::Kind::Action);
	EXPECT_EQ(playful[0].animation, 67u);
	EXPECT_FLOAT_EQ(playful[1].movement.maxDistance, 50.0f);
	EXPECT_FLOAT_EQ(playful[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(playful[3].face, creature_face::Cue::Compassion);
	const auto helpful = CastAt(CastStyle::Helpful, 10, 0, 7, 10.0f, Always(0));
	EXPECT_EQ(helpful[0].animation, 64u);
	EXPECT_FLOAT_EQ(helpful[1].movement.maxDistance, 20.0f);
	// A power-up draws its gesture before casting
	const auto powered = CastAt(CastStyle::Helpful, 11, 1, 7, 10.0f, Always(1));
	ASSERT_EQ(powered.size(), 5u);
	EXPECT_EQ(powered[3].kind, Step::Kind::Gesture);
	EXPECT_EQ(powered[3].animation, 1u);
}

TEST(CreatureCastAgenda, AttacksFromFiveTimesItsHeightBackingOffToTwice)
{
	// A tornado, the lightning storm and the power-ups of the explosion
	const auto agenda = CastAt(CastStyle::Aggressive, 18, 0, 7, 10.0f, Always(0));
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, 53u);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[4].cast.magicType, 18u);
}

TEST(CreatureCastAgenda, AnExplosionFromAHundredAngryOneTimeInThree)
{
	uint32_t asked = 0;
	const auto agenda = CastAt(CastStyle::Explosion, 7, 0, 7, 10.0f, [&asked](uint32_t range) {
		asked = range;
		return 0u;
	});
	EXPECT_EQ(asked, 3u);
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].animation, 53u);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 100.0f);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[4].cast.magicType, 7u);
}

TEST(CreatureCastAgenda, AFireballFromAClearAreaNearItGettingAwayOnlyFromTheLiving)
{
	// At a living thing, angry: to the clear area, the anger, away from the thing, the turn and the cast
	const auto living = CastFireballAt(1, 0, 7, true, {30.0f, 40.0f}, 10.0f, Always(0));
	ASSERT_EQ(living.size(), 5u);
	EXPECT_EQ(living[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(living[0].movement.point, glm::vec2(30.0f, 40.0f));
	EXPECT_FLOAT_EQ(living[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(living[1].kind, Step::Kind::Action);
	EXPECT_EQ(living[1].animation, 53u);
	EXPECT_EQ(living[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(living[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(living[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(living[4].kind, Step::Kind::Cast);
	EXPECT_EQ(living[4].cast.magicType, 1u);
	// At a thing not living, not angry: to the clear area, the turn and the cast
	const auto still = CastFireballAt(2, 0, 7, false, {30.0f, 40.0f}, 10.0f, Always(1));
	ASSERT_EQ(still.size(), 3u);
	EXPECT_EQ(still[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(still[2].cast.magicType, 2u);
}

TEST(CreatureCastAgenda, AFizzleEmbarrassesItAndOneTimeInTwoMakesItSadder)
{
	const auto sad = ReactToFizzle(Always(0), 0.5f);
	ASSERT_EQ(sad.agenda.size(), 2u);
	EXPECT_EQ(sad.agenda[0].animation, 73u);
	EXPECT_EQ(sad.agenda[1].animation, 56u);
	ASSERT_TRUE(sad.sadness.has_value());
	EXPECT_FLOAT_EQ(*sad.sadness, 0.15f);
	const auto embarrassed = ReactToFizzle(Always(1), 0.5f);
	ASSERT_EQ(embarrassed.agenda.size(), 1u);
	EXPECT_FALSE(embarrassed.sadness.has_value());
}

TEST(CreatureCastAgenda, EachMiracleThatAttacksIsCastItsOwnWay)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using Build = plan_actions::Build;
	const auto buildOf = [](std::string_view action) {
		const auto* executor = plan_actions::For(action);
		return executor != nullptr ? std::optional(executor->build) : std::nullopt;
	};
	EXPECT_EQ(buildOf("CastLightningBoltPU1"), Build::CastLightning);
	EXPECT_EQ(buildOf("CastLightningBoltPU2"), Build::CastLightning);
	EXPECT_EQ(buildOf("CastFireball"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastFireballPU1"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastFireballPU2"), Build::CastFireball);
	EXPECT_EQ(buildOf("CastExplosion"), Build::CastExplosion);
	EXPECT_EQ(buildOf("CastExplosionPU1"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastExplosionPU2"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastTornado"), Build::CastAggressive);
	EXPECT_EQ(buildOf("CastLightningStorm"), Build::CastAggressive);
	// With no clear area near it, the creature can't cast a fireball
	const auto* fireball = plan_actions::For("CastFireball");
	ASSERT_NE(fireball, nullptr);
	EXPECT_TRUE(plan_actions::IsCast(*fireball));
	const plan_actions::CastInfo noRoom {.magicType = 1, .height = 10.0f};
	EXPECT_FALSE(plan_actions::Agenda(*fireball, 7u, {}, {}, Always(1), noRoom).has_value());
	auto room = noRoom;
	room.clearArea = glm::vec2(5.0f, 5.0f);
	EXPECT_TRUE(plan_actions::Agenda(*fireball, 7u, {}, {}, Always(1), room).has_value());
}

TEST(CreatureCastAgenda, TheMiracleIsCastAsThePoseLoopsAndHeldThirtyTurns)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {CastAt(CastStyle::Lightning, 4, 0, 7, 15.0f, Always(1)).back()});
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.startSequence.has_value());
	EXPECT_FALSE(commands.cast.has_value());
	senses.bodyBusy = true;
	commands = Think(mind, senses, Always(0));
	EXPECT_FALSE(commands.cast.has_value());
	senses.bodyLooping = true;
	commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.cast.has_value());
	EXPECT_EQ(commands.cast->magicType, 4u);
	for (int turn = 0; turn < 30; ++turn)
	{
		commands = Think(mind, senses, Always(0));
		EXPECT_FALSE(commands.releaseCast) << turn;
	}
	commands = Think(mind, senses, Always(0));
	EXPECT_TRUE(commands.releaseCast);
	EXPECT_TRUE(commands.endSit);
	senses.bodyBusy = false;
	senses.bodyLooping = false;
	commands = Think(mind, senses, Always(0));
	EXPECT_FALSE(commands.releaseCast);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.gaveUp);
}

TEST(CreatureCastAgenda, AMoveTheBodySaysFailedGivesUpTheRest)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, CastAt(CastStyle::Playful, 30, 0, 7, 15.0f, Always(1)));
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::GoNearObject);
	senses.subMove = SubMove::Running;
	(void)Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 0u);
	senses.subMove = SubMove::Done;
	(void)Think(mind, senses, Always(0));
	EXPECT_EQ(mind.step, 1u);
	senses.subMove = SubMove::Running;
	(void)Think(mind, senses, Always(0));
	senses.subMove = SubMove::Failed;
	(void)Think(mind, senses, Always(0));
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_EQ(mind.step, mind.agenda.size());
}

TEST(CreatureCastAgenda, AMoveTheBodySaysIsDoneTakesItsEffect)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, GiveFishToStore(std::nullopt, 7, 12.0f));
	Senses senses {.seconds = 0.1f};
	auto commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::ToThrowPosition);
	senses.subMove = SubMove::Done;
	commands = Think(mind, senses, Always(0));
	EXPECT_EQ(commands.effect, Effect::None);
	senses.subMove = SubMove::Running;
	commands = Think(mind, senses, Always(0));
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::TurnToFaceObject);
	senses.subMove = SubMove::Done;
	commands = Think(mind, senses, Always(0));
	EXPECT_EQ(commands.effect, Effect::Completed);
	EXPECT_EQ(mind.step, 2u);
}

TEST(CreatureCastAgenda, EachHelpfulMiracleIsCastAtItsOwnThings)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using Build = plan_actions::Build;
	using Target = plan_actions::Target;
	const auto executorOf = [](std::string_view action) {
		const auto* executor = plan_actions::For(action);
		EXPECT_NE(executor, nullptr) << action;
		return executor != nullptr ? *executor : plan_actions::Executor {};
	};
	// The helpful miracles cast the way the heal is
	EXPECT_EQ(executorOf("CastMagicFoodPU1").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastMagicFoodPU1").target, Target::Anything);
	EXPECT_EQ(executorOf("CastMagicWood").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastMagicWood").target, Target::StoragePit);
	EXPECT_EQ(executorOf("CastPhysicalShield").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastPhysicalShield").target, Target::Anything);
	EXPECT_EQ(executorOf("CastShieldPU1").build, Build::CastHelpful);
	EXPECT_EQ(executorOf("CastShieldPU1").target, Target::Anything);
	// The miracles with their own way of casting
	EXPECT_EQ(executorOf("CastMagicFood").build, Build::CastFood);
	EXPECT_EQ(executorOf("CastMagicFood").target, Target::BuiltFoodStore);
	EXPECT_EQ(executorOf("CastShield").build, Build::CastShield);
	EXPECT_EQ(executorOf("CastShield").target, Target::Anything);
	EXPECT_EQ(executorOf("CastMagicWater").build, Build::CastWaterOnItself);
	EXPECT_EQ(executorOf("CastMagicWater").target, Target::None);
	EXPECT_EQ(executorOf("CastImpressiveSpell").build, Build::CastToImpress);
	EXPECT_EQ(executorOf("CastImpressiveSpell").target, Target::Villager);
	for (const auto* name : {"CastMagicFood", "CastShield", "CastMagicWater", "CastImpressiveSpell"})
	{
		EXPECT_TRUE(plan_actions::IsCast(executorOf(name))) << name;
	}
}

TEST(CreatureCastAgenda, FoodIsScatteredFromTwoAwayWithItsGesture)
{
	const auto agenda = CastFoodAt(14, 3, 7);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 2.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Gesture);
	EXPECT_EQ(agenda[2].animation, 3u);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[3].cast.magicType, 14u);
	EXPECT_EQ(*agenda[3].cast.object, 7u);
	EXPECT_EQ(agenda[3].sequence, (std::array<size_t, 3> {46, 47, 48}));
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.0f);
	// No gesture, no gesture step
	EXPECT_EQ(CastFoodAt(14, 0, 7).size(), 3u);
}

TEST(CreatureCastAgenda, AShieldIsCastFromWhereItStands)
{
	const auto agenda = CastShieldAt(19, 7);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[1].cast.magicType, 19u);
	EXPECT_EQ(agenda[1].sequence, (std::array<size_t, 3> {40, 41, 42}));
}

TEST(CreatureCastAgenda, WaterIsScatteredFromTwiceItsHeight)
{
	const auto agenda = CastWaterAt(22, 7, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[2].cast.magicType, 22u);
	EXPECT_EQ(agenda[2].sequence, k_ScatterPose);
}

TEST(CreatureCastAgenda, OnFireItGoesToAClearAreaCastsWaterOnItselfAndCools)
{
	const auto agenda = CastWaterOnItself(22, 9, {4.0f, 5.0f}, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(4.0f, 5.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 30.0f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->whileGoing);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Cast);
	EXPECT_EQ(*agenda[1].cast.object, 9u);
	EXPECT_EQ(agenda[1].sequence, k_ScatterPose);
	EXPECT_EQ(agenda[2].kind, Step::Kind::CoolDown);
	// With no clear area, the creature cannot stop its own fire
	const auto* executor = openblack::creature_plan_actions::For("CastMagicWater");
	ASSERT_NE(executor, nullptr);
	const openblack::creature_plan_actions::CastInfo noRoom {.magicType = 22, .height = 10.0f};
	EXPECT_FALSE(openblack::creature_plan_actions::Agenda(*executor, std::nullopt, {}, {}, Always(1), noRoom).has_value());
}

TEST(CreatureCastAgenda, ToImpressItPointsBeforeOrAfterAndBeckonsTwoTimesInThree)
{
	// Draws of 0: it points first and does not beckon
	const auto first = CastToImpress(10, 7, 10.0f, Always(0));
	ASSERT_EQ(first.size(), 5u);
	EXPECT_EQ(first[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(first[0].movement.maxDistance, 15.0f);
	EXPECT_EQ(first[1].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(first[1].movement.maxDistance, 20.0f);
	EXPECT_EQ(first[2].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_FLOAT_EQ(*first[2].order.seconds, 1.0f);
	EXPECT_EQ(first[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(first[4].kind, Step::Kind::Cast);
	EXPECT_EQ(first[4].cast.magicType, 10u);
	// Draws of 1: it beckons, casts, then points
	const auto after = CastToImpress(16, 7, 10.0f, Always(1));
	ASSERT_EQ(after.size(), 6u);
	EXPECT_EQ(after[2].kind, Step::Kind::Action);
	EXPECT_EQ(after[2].animation, 52u);
	EXPECT_EQ(after[4].cast.magicType, 16u);
	EXPECT_EQ(after[5].order.kind, ObjectOrder::Kind::PointAt);
	// When it knows no miracle that impresses, it casts nothing
	const auto none = CastToImpress(std::nullopt, 7, 10.0f, Always(1));
	EXPECT_TRUE(std::ranges::none_of(none, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
}

TEST(CreatureCastAgenda, ToImpressItCastsTheMiracleCastTheLongestTimeAgo)
{
	EXPECT_EQ(PickImpressiveMiracle({false, false, false, false}, {0, 0, 0, 0}), std::nullopt);
	// The last of equals
	EXPECT_EQ(PickImpressiveMiracle({true, true, true, true}, {0, 0, 0, 0}), 3u);
	EXPECT_EQ(PickImpressiveMiracle({true, true, true, false}, {50, 20, 20, 0}), 2u);
	EXPECT_EQ(PickImpressiveMiracle({true, false, true, false}, {10, 0, 20, 0}), 0u);
}

TEST(CreatureCastAgenda, ItChoosesToImpressKnowingAMiracleSeenMoreThanHalfItsTimes)
{
	const ImpressiveMiracleState known {.known = true, .canPay = true, .seen = 3.0f, .needed = 5.0f};
	EXPECT_TRUE(CanCastToImpress(std::array {known}));
	auto half = known;
	half.seen = 2.5f;
	EXPECT_FALSE(CanCastToImpress(std::array {half}));
	auto unpaid = known;
	unpaid.canPay = false;
	EXPECT_FALSE(CanCastToImpress(std::array {unpaid}));
	auto unknown = known;
	unknown.known = false;
	EXPECT_FALSE(CanCastToImpress(std::array {unknown}));
	EXPECT_TRUE(CanCastToImpress(std::array {unknown, known}));
	// Food lets it choose to impress, but it never casts food to impress
	EXPECT_EQ(k_ImpressMiraclesItMayKnow.front(), 14u);
	EXPECT_TRUE(std::ranges::find(k_ImpressiveMiracles, 14u) == k_ImpressiveMiracles.end());
}

TEST(CreatureCastMoves, RoutePlanRadiusAndArrival)
{
	// A thing as low as nothing: seven tenths of the creature's radius off its own
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 0.0f, false, 15.0f, 5.0f), 10.0f - 0.7f * 5.0f);
	// Half of four fifths of the creature's height
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 6.0f, false, 15.0f, 5.0f), 10.0f - (0.7f - 0.3f * 0.5f) * 5.0f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, false, 15.0f, 5.0f), 10.0f - 0.4f * 5.0f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(10.0f, 20.0f, true, 15.0f, 5.0f), 0.25f);
	EXPECT_FLOAT_EQ(cast_moves::RoutePlanRadius(1.0f, 20.0f, true, 15.0f, 5.0f), 0.1f);
	EXPECT_TRUE(cast_moves::Arrived(10.9f, 2.0f, 3.0f, 5.0f));
	EXPECT_FALSE(cast_moves::Arrived(11.0f, 2.0f, 3.0f, 5.0f));
}

TEST(CreatureCastMoves, GettingAway)
{
	EXPECT_FLOAT_EQ(cast_moves::GetAwayDistance(20.0f, 5.0f), 26.0f);
	EXPECT_FLOAT_EQ(cast_moves::GetAwayDistance(20.0f, std::nullopt), 20.0f);
	const auto away = cast_moves::GetAwayPoint({10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 26.0f);
	EXPECT_FLOAT_EQ(away.x, 36.0f);
	const auto onTop = cast_moves::GetAwayPoint({10.0f, 0.0f, 10.0f}, {10.0f, 0.0f, 10.0f}, 5.0f);
	EXPECT_FLOAT_EQ(onTop.x, 15.0f);
}

TEST(CreatureCastMoves, ToTurnItsBackToAThingItFacesThePointOnTheOtherSide)
{
	const auto point = cast_moves::PointToFaceAwayFrom({10.0f, 20.0f}, {13.0f, 16.0f});
	EXPECT_FLOAT_EQ(point.x, 7.0f);
	EXPECT_FLOAT_EQ(point.y, 24.0f);
}

TEST(CreatureCastMoves, TheNearestClearAreaIsTheMiddleOfAClearSquare)
{
	// All clear: a square of two cells about the point, its middle at a cell corner
	const auto open = cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t, int32_t) { return true; });
	ASSERT_TRUE(open.has_value());
	EXPECT_FLOAT_EQ(open->x, 127.5f);
	EXPECT_FLOAT_EQ(open->y, 127.5f);
	// Nothing clear west of x = 200
	const auto east = cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t x, int32_t) { return x >= 20; });
	ASSERT_TRUE(east.has_value());
	EXPECT_FLOAT_EQ(east->x, 207.5f);
	EXPECT_FALSE(cast_moves::FindClearArea({125.0f, 125.0f}, 15.0f, [](int32_t, int32_t) { return false; }).has_value());
}

TEST(CreatureCastMoves, AThingKeepsCirclesClear)
{
	const glm::mat2 still(1.0f);
	// Squarish: one circle as wide as its longer side
	const auto square = cast_moves::CollideCircles({3.0f, 2.5f}, {10.0f, 10.0f}, still);
	ASSERT_EQ(square.size(), 1u);
	EXPECT_FLOAT_EQ(square[0].radius, 3.0f);
	// Long: a row of three as wide as its shorter side, along its length
	const auto wall = cast_moves::CollideCircles({6.0f, 2.5f}, {10.0f, 10.0f}, still);
	ASSERT_EQ(wall.size(), 3u);
	EXPECT_FLOAT_EQ(wall[0].radius, 2.5f);
	EXPECT_FLOAT_EQ(wall[0].centre.x, 10.0f - 4.0f);
	EXPECT_FLOAT_EQ(wall[2].centre.x, 10.0f + 4.0f);
	// Sides under 1 count as 1
	EXPECT_FLOAT_EQ(cast_moves::CollideCircles({0.2f, 0.2f}, {0.0f, 0.0f}, still)[0].radius, 1.0f);
	// A cell is blocked by a circle coming within 5 of its middle
	EXPECT_TRUE(cast_moves::BlocksCell({.centre = {19.5f, 5.0f}, .radius = 1.0f}, 1, 0));
	EXPECT_FALSE(cast_moves::BlocksCell({.centre = {5.0f, 5.0f}, .radius = 0.3f}, 1, 0));
}
