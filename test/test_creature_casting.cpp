/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsAngle.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
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
	// The sadness is a draw from 0 to 0.3, and only a sad creature draws it
	std::vector<float> asked;
	const auto draw = [&asked](float most) {
		asked.push_back(most);
		return most / 2.0f;
	};
	const auto sad = ReactToFizzle(Always(0), draw);
	ASSERT_EQ(sad.agenda.size(), 2u);
	EXPECT_EQ(sad.agenda[0].animation, 73u);
	EXPECT_EQ(sad.agenda[1].animation, 56u);
	ASSERT_TRUE(sad.sadness.has_value());
	EXPECT_FLOAT_EQ(*sad.sadness, 0.15f);
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked[0], 0.3f);
	const auto embarrassed = ReactToFizzle(Always(1), draw);
	ASSERT_EQ(embarrassed.agenda.size(), 1u);
	EXPECT_FALSE(embarrassed.sadness.has_value());
	EXPECT_EQ(asked.size(), 1u);
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

TEST(CreatureCastAgenda, AShieldForAPlanAboutNothingIdlesInstead)
{
	const auto* executor = openblack::creature_plan_actions::For("CastShield");
	ASSERT_NE(executor, nullptr);
	const openblack::creature_plan_actions::CastInfo cast {.magicType = 19, .height = 15.0f};
	openblack::creature_plan_actions::Situation situation;
	const auto idle = openblack::creature_plan_actions::Agenda(*executor, 7u, {}, situation, Always(0), cast);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->size(), BeIdle(Always(0)).size());
	EXPECT_TRUE(std::ranges::none_of(*idle, [](const Step& step) { return step.kind == Step::Kind::Cast; }));
	situation.activity = 3u;
	const auto shield = openblack::creature_plan_actions::Agenda(*executor, 7u, {}, situation, Always(0), cast);
	ASSERT_TRUE(shield.has_value());
	ASSERT_EQ(shield->size(), 2u);
	EXPECT_EQ(shield->back().kind, Step::Kind::Cast);
	EXPECT_EQ(shield->back().cast.magicType, 19u);
}

TEST(CreatureCastAgenda, ATownGetsFoodWoodShieldsAndWaterEachAtItsOwnThings)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::Target;
	struct Expected
	{
		std::string_view action;
		Target target;
		Build build;
	};
	const std::array expected {
	    Expected {"CastMagicFoodByWorshipSite", Target::WorshipSite, Build::CastFood},
	    Expected {"CastMagicWoodByBuildingSite", Target::BuildingBeingBuilt, Build::CastHelpful},
	    Expected {"CastMagicWoodByWorkshop", Target::Workshop, Build::CastHelpful},
	    Expected {"CastShieldAroundTown", Target::BuiltStoragePit, Build::CastShield},
	    Expected {"SprinkleMagicWaterOnCrops", Target::Waterable, Build::CastWater},
	    Expected {"SprinkleMagicWaterPU1OnCrops", Target::Waterable, Build::CastWater},
	    Expected {"WaterTree", Target::AnyTree, Build::CastWater},
	    Expected {"WaterField", Target::DryField, Build::CastWater},
	    Expected {"WaterTreeForTown", Target::Anything, Build::WaterTreeForTown},
	    Expected {"WaterTreePU1ForTown", Target::Anything, Build::WaterTreeForTown},
	};
	for (const auto& row : expected)
	{
		const auto* executor = plan_actions::For(row.action);
		ASSERT_NE(executor, nullptr) << row.action;
		EXPECT_EQ(executor->target, row.target) << row.action;
		EXPECT_EQ(executor->build, row.build) << row.action;
		EXPECT_TRUE(plan_actions::IsCast(*executor)) << row.action;
	}
	EXPECT_EQ(plan_actions::For("WaterTreeForTown")->use, Target::AnyTree);
	EXPECT_EQ(plan_actions::For("WaterTreePU1ForTown")->use, Target::AnyTree);
	// The creature table gives wood by a workshop no miracle; the agenda casts wood
	EXPECT_EQ(plan_actions::For("CastMagicWoodByWorkshop")->castMagic, std::optional<uint32_t>(21u));
	// The power-up of the water on crops casts plain water
	const plan_actions::CastInfo powerUp {.magicType = 23, .height = 15.0f};
	const auto* crops = plan_actions::For("SprinkleMagicWaterPU1OnCrops");
	const auto sprinkled = plan_actions::Agenda(*crops, 7u, {}, {}, Always(0), powerUp);
	ASSERT_TRUE(sprinkled.has_value());
	EXPECT_EQ(sprinkled->back().cast.magicType, 22u);
	EXPECT_EQ(sprinkled->back().sequence, k_ScatterPose);
	// The power-up of the water for a tree casts plain water too
	plan_actions::Situation withTree;
	withTree.instrument = 9u;
	const auto watered = plan_actions::Agenda(*plan_actions::For("WaterTreePU1ForTown"), 5u, {}, withTree, Always(0), powerUp);
	ASSERT_TRUE(watered.has_value());
	EXPECT_EQ(watered->back().cast.magicType, 22u);
}

TEST(CreatureCastAgenda, ForATownItWatersTheTreeItUsesOrFailsWithoutOne)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* executor = plan_actions::For("WaterTreeForTown");
	ASSERT_NE(executor, nullptr);
	const plan_actions::CastInfo cast {.magicType = 22, .height = 15.0f};
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Agenda(*executor, 5u, {}, situation, Always(0), cast).has_value());
	situation.instrument = 9u;
	const auto agenda = plan_actions::Agenda(*executor, 5u, {}, situation, Always(0), cast);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3u);
	EXPECT_EQ(agenda->at(0).movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(*agenda->at(0).movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda->at(0).movement.maxDistance, 30.0f);
	EXPECT_EQ(*agenda->at(1).movement.object, 9u);
	EXPECT_EQ(*agenda->at(2).cast.object, 9u);
	EXPECT_EQ(agenda->at(2).cast.magicType, 22u);
	EXPECT_FLOAT_EQ(agenda->at(2).seconds, 3.0f);
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

TEST(CreatureCastAgenda, AForestIsScatteredAtThePlaceOfTheVillagerForFourSeconds)
{
	const glm::vec3 place {40.0f, 3.0f, 50.0f};
	const auto agenda = CastForestAt(13, 7, place, 15.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(*agenda[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 37.5f);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->whileGoing);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[2].cast.magicType, 13u);
	EXPECT_FALSE(agenda[2].cast.object.has_value());
	ASSERT_TRUE(agenda[2].cast.place.has_value());
	EXPECT_EQ(*agenda[2].cast.place, place);
	EXPECT_EQ(agenda[2].sequence, k_ScatterPose);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 4.0f);
	// The creature draws no gesture
	EXPECT_TRUE(std::ranges::none_of(agenda, [](const Step& step) { return step.kind == Step::Kind::Gesture; }));
	// It casts at the place of the villager. Without a place, it cannot cast.
	const auto* executor = openblack::creature_plan_actions::For("CastMagicForest");
	ASSERT_NE(executor, nullptr);
	EXPECT_TRUE(openblack::creature_plan_actions::IsCast(*executor));
	openblack::creature_plan_actions::CastInfo cast {.magicType = 13, .height = 15.0f};
	EXPECT_FALSE(openblack::creature_plan_actions::Agenda(*executor, 7u, {}, {}, Always(1), cast).has_value());
	cast.place = place;
	const auto built = openblack::creature_plan_actions::Agenda(*executor, 7u, {}, {}, Always(1), cast);
	ASSERT_TRUE(built.has_value());
	EXPECT_EQ(*built->back().cast.place, place);
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

TEST(CreatureCastMoves, ABallIsMeasuredThroughTheAirFromTheCellsMiddle)
{
	// On the ground a ball works as a circle does
	EXPECT_TRUE(cast_moves::BlocksCell(cast_moves::CollideSphere {.centre = {19.5f, 0.0f, 5.0f}, .radius = 1.0f}, 1, 0));
	// High up, the same ball is too far from the cell's middle
	EXPECT_FALSE(cast_moves::BlocksCell(cast_moves::CollideSphere {.centre = {15.0f, 20.0f, 5.0f}, .radius = 3.0f}, 1, 0));
	// Just within reach through the air: 6 up and 0 across, less a radius of 1.5, is less than 5
	EXPECT_TRUE(cast_moves::BlocksCell(cast_moves::CollideSphere {.centre = {15.0f, 6.0f, 5.0f}, .radius = 1.5f}, 1, 0));
}

TEST(CreatureCastMoves, AWorshipSitesTenBallsStandAlongItsModelAtTheirHeights)
{
	const glm::mat3 still(1.0f);
	const auto spheres = cast_moves::WorshipSiteSpheres({100.0f, 30.0f, 200.0f}, still, 1.0f);
	ASSERT_EQ(spheres.size(), 10U);
	EXPECT_FLOAT_EQ(spheres[0].centre.x, 102.448f);
	EXPECT_FLOAT_EQ(spheres[0].centre.y, 31.4f);
	EXPECT_FLOAT_EQ(spheres[0].centre.z, 200.0f - 34.171f);
	EXPECT_FLOAT_EQ(spheres[0].radius, 1.3f * 1.4f);
	EXPECT_FLOAT_EQ(spheres[9].centre.x, 126.905f);
	EXPECT_FLOAT_EQ(spheres[9].radius, 1.3f * 3.0f);

	// A site turned a quarter turn about the up axis, at twice the size: the balls turn and spread, but keep their widths
	const glm::mat3 quarter(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	const auto turned = cast_moves::WorshipSiteSpheres({0.0f, 0.0f, 0.0f}, quarter, 2.0f);
	EXPECT_NEAR(turned[4].centre.x, 2.0f * 39.084f, 1e-4f);
	EXPECT_NEAR(turned[4].centre.y, 4.0f, 1e-5f);
	EXPECT_NEAR(turned[4].centre.z, 2.0f * 3.93f, 1e-4f);
	EXPECT_FLOAT_EQ(turned[4].radius, 1.3f * 2.0f);
}

TEST(CreatureCastMoves, WhatAThingInACellDoesToTheCellTest)
{
	const std::vector<cast_moves::CollideSphere> onTheCell {{.centre = {15.0f, 0.0f, 5.0f}, .radius = 2.0f}};
	const std::vector<cast_moves::CollideSphere> farAway {{.centre = {100.0f, 0.0f, 100.0f}, .radius = 2.0f}};
	// The temple keeps every cell it is in from being clear, whatever its balls
	EXPECT_TRUE(cast_moves::BlocksCell({.temple = true, .living = false, .tested = false, .spheres = {}}, 1, 0, false));
	// A thing that is looked at blocks only by its balls
	EXPECT_TRUE(cast_moves::BlocksCell({.temple = false, .living = false, .tested = true, .spheres = onTheCell}, 1, 0, false));
	EXPECT_FALSE(cast_moves::BlocksCell({.temple = false, .living = false, .tested = true, .spheres = farAway}, 1, 0, false));
	// A thing the creature need not go round is passed over, its balls or not
	EXPECT_FALSE(
	    cast_moves::BlocksCell({.temple = false, .living = false, .tested = false, .spheres = onTheCell}, 1, 0, false));
	// Something living, which has no balls, blocks only when the test asks for it
	EXPECT_FALSE(cast_moves::BlocksCell({.temple = false, .living = true, .tested = false, .spheres = {}}, 1, 0, false));
	EXPECT_TRUE(cast_moves::BlocksCell({.temple = false, .living = true, .tested = false, .spheres = {}}, 1, 0, true));
}

TEST(CreatureCastAgenda, TheGiftsToAStoreGoOnlyToABuiltStoragePit)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Target;
	EXPECT_EQ(plan_actions::For("GiveFishToStoragePit")->target, Target::BuiltStoragePit);
	EXPECT_EQ(plan_actions::For("GiveWoodFromTreeToStoragePit")->target, Target::BuiltStoragePit);
	// The creature casts wood into a storage pit that is built or not
	EXPECT_EQ(plan_actions::For("CastMagicWood")->target, Target::StoragePit);
}

TEST(CreatureCastAgenda, TheOneShotActionsCastByTheKindOfTheirMiracle)
{
	namespace plan_actions = openblack::creature_plan_actions;
	using plan_actions::Build;
	using plan_actions::OneShotKind;
	using plan_actions::Target;
	struct Expected
	{
		std::string_view action;
		Target target;
		Target use;
		Build build;
		OneShotKind kind;
	};
	constexpr std::array expected {
	    Expected {"CastOneOffSpellInHandAggressive", Target::Anything, Target::None, Build::CastOneShotInHand,
	              OneShotKind::Aggressive},
	    Expected {"CastOneOffSpellInHandCompassiona", Target::Anything, Target::None, Build::CastOneShotInHand,
	              OneShotKind::Compassionate},
	    Expected {"CastOneOffSpellInHandPlayful", Target::Creature, Target::None, Build::CastOneShotInHand,
	              OneShotKind::Playful},
	    Expected {"PickUpAndCastOneOffSpellAggressi", Target::Anything, Target::OneShotAggressive, Build::PickUpAndCastOneShot,
	              OneShotKind::Aggressive},
	    Expected {"PickUpAndCastOneOffSpellCompassi", Target::Anything, Target::OneShotCompassionate,
	              Build::PickUpAndCastOneShot, OneShotKind::Compassionate},
	    Expected {"PickUpAndCastOneOffSpellPlayful", Target::Creature, Target::OneShotPlayful, Build::PickUpAndCastOneShot,
	              OneShotKind::Playful},
	};
	for (const auto& row : expected)
	{
		const auto* executor = plan_actions::For(row.action);
		ASSERT_NE(executor, nullptr) << row.action;
		EXPECT_EQ(executor->target, row.target) << row.action;
		EXPECT_EQ(executor->use, row.use) << row.action;
		EXPECT_EQ(executor->build, row.build) << row.action;
		EXPECT_EQ(executor->oneShot, row.kind) << row.action;
		// The miracle comes from the globe, not from the creature table
		EXPECT_FALSE(plan_actions::IsCast(*executor)) << row.action;
	}
}

TEST(CreatureCastAgenda, ItCastsTheOneShotInItsHandOnlyWhenTheKindIsRight)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* aggressive = plan_actions::For("CastOneOffSpellInHandAggressive");
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Possible(*aggressive, situation));
	situation.handFull = true;
	situation.heldOneShot = plan_actions::OneShot {.magicType = 10, .compassionate = true};
	EXPECT_FALSE(plan_actions::Possible(*aggressive, situation));
	EXPECT_TRUE(plan_actions::Possible(*plan_actions::For("CastOneOffSpellInHandCompassiona"), situation));
	situation.heldOneShot = plan_actions::OneShot {.magicType = 1, .aggressive = true};
	EXPECT_TRUE(plan_actions::Possible(*aggressive, situation));
	// The actions that pick up a globe need no globe in the hand
	EXPECT_TRUE(plan_actions::Possible(*plan_actions::For("PickUpAndCastOneOffSpellCompassi"), {}));
}

TEST(CreatureCastAgenda, ItGoesNearTurnsLetsTheGlobeGoAndCastsTheOneShotInItsHand)
{
	namespace plan_actions = openblack::creature_plan_actions;
	plan_actions::Situation situation;
	situation.handFull = true;
	situation.height = 15.0f;
	situation.heldOneShot = plan_actions::OneShot {.magicType = 4, .aggressive = true};
	const auto agenda =
	    plan_actions::Agenda(*plan_actions::For("CastOneOffSpellInHandAggressive"), 7u, {}, situation, Always(0));
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 5u);
	EXPECT_EQ(agenda->at(0).movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda->at(0).movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda->at(1).movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda->at(1).movement.maxDistance, 30.0f);
	EXPECT_EQ(agenda->at(2).movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda->at(2).seconds, 0.1f);
	EXPECT_EQ(agenda->at(3).kind, Step::Kind::UseUpHeld);
	EXPECT_EQ(agenda->at(4).kind, Step::Kind::Cast);
	EXPECT_EQ(agenda->at(4).cast.magicType, 4u);
	EXPECT_EQ(*agenda->at(4).cast.object, 7u);
	EXPECT_EQ(agenda->at(4).sequence, (std::array<size_t, 3> {40, 41, 42}));
	EXPECT_FLOAT_EQ(agenda->at(4).seconds, 3.0f);

	// With a kind miracle in its hand, it does not get away first
	situation.heldOneShot = plan_actions::OneShot {.magicType = 10, .compassionate = true};
	const auto kind =
	    plan_actions::Agenda(*plan_actions::For("CastOneOffSpellInHandCompassiona"), 7u, {}, situation, Always(0));
	ASSERT_TRUE(kind.has_value());
	ASSERT_EQ(kind->size(), 4u);
	EXPECT_EQ(kind->at(1).movement.kind, Movement::Kind::TurnToFaceObject);
}

TEST(CreatureCastAgenda, ItPicksUpTheGlobeWithAnEmptyHandAndTheGlobeGoesAfterTheCast)
{
	namespace plan_actions = openblack::creature_plan_actions;
	const auto* executor = plan_actions::For("PickUpAndCastOneOffSpellCompassi");
	plan_actions::Situation situation;
	situation.height = 15.0f;
	// Without a globe to use, the action fails
	EXPECT_FALSE(plan_actions::Agenda(*executor, 7u, {}, situation, Always(0)).has_value());
	situation.instrument = 9u;
	situation.usedOneShot = plan_actions::OneShot {.magicType = 10, .compassionate = true};
	const auto agenda = plan_actions::Agenda(*executor, 7u, {}, situation, Always(0));
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 7u);
	EXPECT_EQ(agenda->at(0).order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(*agenda->at(0).order.object, 9u);
	EXPECT_FLOAT_EQ(agenda->at(1).movement.maxDistance, 50.0f);
	EXPECT_EQ(agenda->at(2).movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_EQ(agenda->at(4).kind, Step::Kind::UseUpHeld);
	EXPECT_EQ(agenda->at(5).cast.magicType, 10u);
	EXPECT_EQ(agenda->at(6).kind, Step::Kind::UseUp);
	EXPECT_EQ(*agenda->at(6).object, 9u);

	// With a full hand it picks nothing up: what it holds goes, and the globe goes after the cast
	situation.handFull = true;
	const auto full = plan_actions::Agenda(*executor, 7u, {}, situation, Always(0));
	ASSERT_TRUE(full.has_value());
	ASSERT_EQ(full->size(), 6u);
	EXPECT_EQ(full->at(0).movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(full->at(3).kind, Step::Kind::UseUpHeld);
	EXPECT_EQ(*full->back().object, 9u);
}

TEST(CreatureCastAgenda, TheOneShotInItsHandLooksAtTheThingAndPullsTheFaceOfCompassion)
{
	namespace plan_actions = openblack::creature_plan_actions;
	plan_actions::Situation situation;
	situation.handFull = true;
	situation.height = 15.0f;
	situation.heldOneShot = plan_actions::OneShot {.magicType = 4, .aggressive = true};
	const auto agenda =
	    plan_actions::Agenda(*plan_actions::For("CastOneOffSpellInHandAggressive"), 7u, {}, situation, Always(0));
	ASSERT_TRUE(agenda.has_value());
	// It looks at the thing as it goes, then at the thing itself. The face is that of compassion, also for an aggressive
	// globe.
	ASSERT_TRUE(agenda->at(0).gaze.has_value());
	EXPECT_TRUE(agenda->at(0).gaze->whileGoing);
	EXPECT_EQ(agenda->at(0).face, creature_face::Cue::Compassion);
	ASSERT_TRUE(agenda->at(1).gaze.has_value());
	EXPECT_EQ(agenda->at(1).gaze->object, 7u);
	EXPECT_EQ(agenda->at(1).face, creature_face::Cue::None);
	ASSERT_TRUE(agenda->at(2).gaze.has_value());
	EXPECT_EQ(agenda->at(2).gaze->object, 7u);
	EXPECT_EQ(agenda->at(2).face, creature_face::Cue::Compassion);
}

TEST(CreatureCastAgenda, TheOneShotThatItPicksUpPullsTheFaceOfItsKind)
{
	namespace plan_actions = openblack::creature_plan_actions;
	plan_actions::Situation situation;
	situation.height = 15.0f;
	situation.instrument = 9u;
	situation.usedOneShot = plan_actions::OneShot {.magicType = 4, .aggressive = true};
	const auto agenda =
	    plan_actions::Agenda(*plan_actions::For("PickUpAndCastOneOffSpellAggressi"), 7u, {}, situation, Always(0));
	ASSERT_TRUE(agenda.has_value());
	// It looks at the globe as it goes to pick it up, with no face
	ASSERT_TRUE(agenda->at(0).gaze.has_value());
	EXPECT_TRUE(agenda->at(0).gaze->whileGoing);
	EXPECT_EQ(agenda->at(0).face, creature_face::Cue::None);
	EXPECT_EQ(agenda->at(1).face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda->at(3).face, creature_face::Cue::Anger);

	situation.usedOneShot = plan_actions::OneShot {.magicType = 10, .compassionate = true};
	const auto kind =
	    plan_actions::Agenda(*plan_actions::For("PickUpAndCastOneOffSpellCompassi"), 7u, {}, situation, Always(0));
	ASSERT_TRUE(kind.has_value());
	EXPECT_EQ(kind->at(1).face, creature_face::Cue::Compassion);
}

TEST(CreatureCastAgenda, AGrownUpCreatureCastsAGlobeThatItWasGiven)
{
	using creature_giving::CastGlobe;
	using creature_giving::GlobeTarget;
	// Too young before stage 8
	EXPECT_FALSE(CastGlobe(7, MagicType::Heal, 1).has_value());
	const auto heal = CastGlobe(8, MagicType::Heal, 1);
	ASSERT_TRUE(heal.has_value());
	EXPECT_EQ(heal->desire, 1u);
	EXPECT_EQ(heal->action, "CastOneOffSpellInHandCompassiona");
	EXPECT_EQ(heal->target, GlobeTarget::MostUseful);
	// Every other desire casts with the aggressive action
	const auto bolt = CastGlobe(12, MagicType::LightningBolt, 2);
	ASSERT_TRUE(bolt.has_value());
	EXPECT_EQ(bolt->desire, 2u);
	EXPECT_EQ(bolt->action, "CastOneOffSpellInHandAggressive");
	EXPECT_EQ(CastGlobe(12, MagicType::Teleport, 6)->action, "CastOneOffSpellInHandAggressive");
	// A creature spell goes at the creature itself for compassion, else at the nearest creature
	EXPECT_EQ(CastGlobe(13, MagicType::CreatureSpellCompassion, 1)->target, GlobeTarget::Itself);
	EXPECT_EQ(CastGlobe(13, MagicType::CreatureSpellFreeze, 2)->target, GlobeTarget::NearestCreature);
}

TEST(CreatureCastAgenda, ATryThatFizzledIsForgottenWhenItsThingGoes)
{
	const FizzleRecord record {.lastThing = 5u, .pending = true};
	EXPECT_EQ(ForgetGoneThing(record, 6u), record);
	EXPECT_EQ(ForgetGoneThing(record, 5u), FizzleRecord {});
	EXPECT_EQ(ForgetGoneThing({}, 5u), FizzleRecord {});
}

TEST(CreatureCastAgenda, EachStepOfACastLooksAndPullsItsFace)
{
	using creature_face::Cue;
	const auto agenda = CastAt(CastStyle::Helpful, 10, 0, 7, 10.0f, Always(0));
	ASSERT_EQ(agenda.size(), 5u);
	EXPECT_EQ(agenda[0].face, Cue::Compassion);
	EXPECT_FALSE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[1].face, Cue::Compassion);
	EXPECT_EQ(agenda[1].gaze->object, 7u);
	EXPECT_TRUE(agenda[1].gaze->whileGoing);
	EXPECT_EQ(agenda[2].face, Cue::None);
	EXPECT_EQ(agenda[2].gaze->object, 7u);
	EXPECT_FALSE(agenda[2].gaze->whileGoing);
	EXPECT_EQ(agenda[3].face, Cue::Compassion);
	EXPECT_EQ(agenda[3].gaze->object, 7u);
	EXPECT_EQ(agenda[4].face, Cue::Grimace);
	EXPECT_FALSE(agenda[4].gaze.has_value());

	// Before an explosion it shows its anger with an amazed face
	EXPECT_EQ(CastAt(CastStyle::Explosion, 7, 0, 7, 10.0f, Always(0))[0].face, Cue::Amazed);
	// Water is cast with a smile
	const auto water = CastWaterAt(1, 7, 10.0f);
	EXPECT_EQ(water[0].face, Cue::None);
	EXPECT_EQ(water[1].face, Cue::Smile);
	EXPECT_EQ(water[2].face, Cue::Smile);
}

TEST(CreatureCastAgenda, ToAmuseItPicksASpellAtRandomAndCastsFromFourTimesItsHeight)
{
	const std::array spells {AmusingSpell {.magicType = 26}, AmusingSpell {.magicType = 31, .gesture = 5}};
	// The first number picks the spell. The second number decides whether it shows how it feels first.
	std::array<uint32_t, 2> draws {1, 0};
	size_t next = 0;
	const Random random = [&](uint32_t range) { return std::min(draws.at(next++), range - 1); };
	const auto agenda = CastAmusingAt(spells, 7, 10.0f, random);
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_FeelPlayful);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 40.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Gesture);
	EXPECT_EQ(agenda[4].animation, 5u);
	EXPECT_EQ(agenda[5].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[5].cast.magicType, 31u);
	EXPECT_EQ(agenda[5].cast.object, 7u);

	// Four times in five it does not show how it feels first
	const auto quiet = CastAmusingAt(spells, 7, 10.0f, Always(1));
	ASSERT_EQ(quiet.size(), 5u);
	EXPECT_EQ(quiet[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(quiet[4].cast.magicType, 31u);
}

TEST(CreatureCastAgenda, WithNoSpellToAmuseItOnlyShowsThatItFeelsPlayful)
{
	const auto agenda = CastAmusingAt({}, 7, 10.0f, Always(0));
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_FeelPlayful);
}

TEST(CreatureCastAgenda, AnotherCreatureSeemsFriendlyUnlessAngerOrFearComesBetweenThem)
{
	using creature_desires::Desire;
	EXPECT_TRUE(SeemsFriendly(Desire::Play, Desire::Play));
	EXPECT_TRUE(SeemsFriendly(std::nullopt, std::nullopt));
	EXPECT_TRUE(SeemsFriendly(Desire::Play, Desire::Fear));
	EXPECT_FALSE(SeemsFriendly(Desire::Anger, Desire::Play));
	EXPECT_FALSE(SeemsFriendly(Desire::Fear, Desire::Play));
	EXPECT_FALSE(SeemsFriendly(Desire::Play, Desire::Anger));
}

TEST(CreatureCastAgenda, TheAmusingSpellIsACastAtACreatureThatSeemsFriendly)
{
	const auto* amuse = creature_plan_actions::For("CastAmusingSpellOnCreature");
	ASSERT_NE(amuse, nullptr);
	EXPECT_TRUE(creature_plan_actions::IsCast(*amuse));
	EXPECT_EQ(amuse->target, creature_plan_actions::Target::FriendlyCreature);
	const creature_plan_actions::CastInfo cast {.height = 10.0f, .amusingSpells = {AmusingSpell {.magicType = 33}}};
	const auto agenda = creature_plan_actions::Agenda(*amuse, 7u, {}, {}, Always(1), cast);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->back().cast.magicType, 33u);
}

TEST(CreatureCastMoves, OnTopMeasuresBetweenMapPositions)
{
	// A tenth of a metre becomes 655 map units, which is a little less than a tenth of a metre
	EXPECT_LT(cast_moves::MapPointDistance({0.0f, 0.0f}, {0.1f, 0.0f}), cast_moves::k_OnTopDistance);
	EXPECT_FLOAT_EQ(cast_moves::MapPointDistance({0.0f, 0.0f}, {30.0f, 40.0f}), 50.0f);
}

TEST(CreatureCastMoves, SameDestinationComparesTheSquareOfTheDistance)
{
	EXPECT_TRUE(cast_moves::SameDestination({10.0f, 10.0f}, {10.05f, 10.05f}));
	EXPECT_FALSE(cast_moves::SameDestination({10.0f, 10.0f}, {10.1f, 10.0f}));
}

TEST(CreatureCastMoves, HeadsForComparesGameAnglesWithoutGoingRound)
{
	const glm::vec2 creature {5005.0f, 5005.0f};
	// A point a little round from +x, which is game angle 0
	const glm::vec2 point {5105.0f, 5006.5f};
	const float justBelowZero = gutils::k_ScawenOffset - 8.5f / gutils::k_Angle3DToGame;
	const float justAboveZero = gutils::k_ScawenOffset + 10.5f / gutils::k_Angle3DToGame;
	ASSERT_EQ(gutils::ConvertScawenAngleToGameAngle(justBelowZero), 2040u);
	ASSERT_EQ(gutils::ConvertScawenAngleToGameAngle(justAboveZero), 10u);
	EXPECT_TRUE(cast_moves::HeadsFor(creature, point, justAboveZero));
	// Near angle 0 the difference does not go round the circle, so a close heading counts as far
	EXPECT_FALSE(cast_moves::HeadsFor(creature, point, justBelowZero));
	// In the same cell of the land, any heading heads for the point
	EXPECT_TRUE(cast_moves::HeadsFor(creature, {5009.0f, 5001.0f}, justBelowZero));
}
