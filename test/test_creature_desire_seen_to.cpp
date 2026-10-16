/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreaturePhysiology.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;
using creature_mind::Step;

namespace
{
creature_mind::Random NoRandom()
{
	return [](uint32_t) { return 0U; };
}

creature_mind::Senses Fighting(bool fighting)
{
	return {.seconds = 0.1f, .fighting = fighting};
}

/// The commands of the turn on which the fight of a planned agenda ends
creature_mind::Commands FightEnds(creature_mind::IdleMind& mind)
{
	static_cast<void>(creature_mind::Think(mind, Fighting(true), NoRandom()));
	static_cast<void>(creature_mind::Think(mind, Fighting(true), NoRandom()));
	return creature_mind::Think(mind, Fighting(false), NoRandom());
}
} // namespace

TEST(CreatureDesireSeenTo, AFightThatIsTheLastStepSeesToTheDesire)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, {{.kind = Step::Kind::Fight, .object = 7}});
	const auto commands = FightEnds(mind);
	EXPECT_TRUE(commands.seesToDesire);
	EXPECT_EQ(mind.step, 1U);
}

TEST(CreatureDesireSeenTo, AFightWithMoreStepsAfterItDoesNotSeeToTheDesire)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Fight, .object = 7}, {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto commands = FightEnds(mind);
	EXPECT_FALSE(commands.seesToDesire);
	EXPECT_EQ(mind.step, 1U);
}

TEST(CreatureDesireSeenTo, OnlyARepairToFullLifeSeesToTheDesire)
{
	EXPECT_FALSE(creature_mind::RepairedToFullLife(0.8f));
	EXPECT_FALSE(creature_mind::RepairedToFullLife(0.99f));
	EXPECT_TRUE(creature_mind::RepairedToFullLife(1.0f));
	EXPECT_TRUE(creature_mind::RepairedToFullLife(1.2f));
}

TEST(CreatureDesireSeenTo, StrengthFadesByTheSpeciesRateEachTurn)
{
	using namespace creature_physiology;
	const Species species {.strengthDecay = 0.5f};
	auto needs = Start(species);
	Shape shape {.fatness = 0.5f, .strength = 0.8f, .size = 1.0f};
	const Turn turn {.moving = false, .phase = 1, .temperature = species.comfortTemperature, .turnsPerSecond = 10.0f};
	TickTurn(needs, shape, species, turn);
	EXPECT_FLOAT_EQ(shape.strength, 0.4f);
	TickTurn(needs, shape, species, turn);
	EXPECT_FLOAT_EQ(shape.strength, 0.2f);
	// Before the body starts to work, nothing changes
	const Turn young {.moving = false, .phase = 0, .temperature = species.comfortTemperature, .turnsPerSecond = 10.0f};
	TickTurn(needs, shape, species, young);
	EXPECT_FLOAT_EQ(shape.strength, 0.2f);
}

TEST(CreatureDesireSeenTo, TheBodyShowsAStrengthGainButNotTheFade)
{
	using namespace creature_physiology;
	const Species species {.strengthDecay = 0.5f, .carryStrengthMinutes = 1.0f};
	auto needs = Start(species);
	Shape shape {.fatness = 0.5f, .strength = 0.8f, .size = 1.0f};
	// Standing still, the strength fades and the body does not show it
	const Turn still {.moving = false, .phase = 1, .temperature = species.comfortTemperature, .turnsPerSecond = 10.0f};
	TickTurn(needs, shape, species, still);
	EXPECT_FALSE(shape.shownStrength.has_value());
	// Carrying a thing, the body shows the strength just after the gain, before the fade of the turn
	const Turn carrying {
	    .moving = true, .phase = 1, .carriedWeight = 1.0f, .temperature = species.comfortTemperature, .turnsPerSecond = 10.0f};
	const auto before = shape.strength;
	TickTurn(needs, shape, species, carrying);
	ASSERT_TRUE(shape.shownStrength.has_value());
	EXPECT_GT(*shape.shownStrength, before);
	EXPECT_FLOAT_EQ(shape.strength, *shape.shownStrength * 0.5f);
	// An action that changes the strength by an amount is shown too
	Shape acted {.strength = 0.3f};
	ModifyStrength(acted, 0.1f);
	ASSERT_TRUE(acted.shownStrength.has_value());
	EXPECT_FLOAT_EQ(*acted.shownStrength, 0.4f);
}

TEST(CreatureDesireSeenTo, ScriptsSetTheFatnessOfACreatureByItsOwnNumber)
{
	EXPECT_EQ(static_cast<int>(script::ObjectPropertyType::CreatureFatness), 27);
	EXPECT_EQ(static_cast<int>(script::ObjectPropertyType::CreatureEnergy), 28);
}
