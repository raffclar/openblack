/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureSwap.h"

using namespace openblack;
using namespace openblack::creature_swap;
using creature_mind::Senses;
using creature_mind::Step;

namespace
{
uint32_t NoRandom(uint32_t /*range*/)
{
	return 0;
}

creature_mind::IdleMind Swapping()
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, {Step {.kind = Step::Kind::SwapMind, .animation = 109}});
	return mind;
}

Senses SwapSenses(bool spellsGone, bool settled, bool bodyBusy = false)
{
	return {
	    .seconds = 0.1f, .bodyBusy = bodyBusy, .choosesNext = false, .swap = {.spellsGone = spellsGone, .settled = settled}};
}
} // namespace

// The breeder's five bonus creatures are there only once unlocked; every other species always is
TEST(CreatureSwap, OnlyTheBonusCreaturesNeedUnlocking)
{
	const Unlocked none;
	for (const int32_t bonus : {3, 6, 14, 15, 16})
	{
		EXPECT_FALSE(IsAvailable(bonus, none)) << bonus;
	}
	for (const int32_t other : {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 17, 99, -1})
	{
		EXPECT_TRUE(IsAvailable(other, none)) << other;
	}
}

TEST(CreatureSwap, AnUnlockedBonusCreatureIsAvailable)
{
	Unlocked unlocked;
	unlocked.set(static_cast<size_t>(Bonus::Rhino));
	EXPECT_TRUE(IsAvailable(15, unlocked));
	EXPECT_FALSE(IsAvailable(16, unlocked));
	EXPECT_EQ(BonusOf(3), Bonus::Leopard);
	EXPECT_EQ(BonusOf(6), Bonus::Horse);
	EXPECT_EQ(BonusOf(14), Bonus::Mandrill);
	EXPECT_EQ(BonusOf(16), Bonus::Gorilla);
	EXPECT_FALSE(BonusOf(11).has_value());
}

// The debug option names the bonus creatures to unlock: all, none, or a list of names
TEST(CreatureSwap, TheDebugOptionNamesTheBonusCreatures)
{
	EXPECT_EQ(ParseUnlocked("all"), Unlocked {}.set());
	EXPECT_EQ(ParseUnlocked("ALL"), Unlocked {}.set());
	EXPECT_EQ(ParseUnlocked("none"), Unlocked {});
	const auto two = ParseUnlocked("Leopard, rhino");
	ASSERT_TRUE(two.has_value());
	EXPECT_TRUE(IsAvailable(3, *two));
	EXPECT_TRUE(IsAvailable(15, *two));
	EXPECT_FALSE(IsAvailable(6, *two));
	EXPECT_FALSE(IsAvailable(14, *two));
	EXPECT_FALSE(IsAvailable(16, *two));
	EXPECT_FALSE(ParseUnlocked("leopard,tiger").has_value());
	EXPECT_FALSE(ParseUnlocked("").has_value());
	for (size_t i = 0; i < k_BonusCount; ++i)
	{
		const auto one = ParseUnlocked(NameOf(static_cast<Bonus>(i)));
		ASSERT_TRUE(one.has_value());
		EXPECT_EQ(one->count(), 1U);
		EXPECT_TRUE(one->test(i));
	}
}

// While the bodies change, a beam runs between them every third game turn
TEST(CreatureSwap, ABeamRunsEveryThirdTurn)
{
	EXPECT_TRUE(IsBeamTurn(0));
	EXPECT_FALSE(IsBeamTurn(1));
	EXPECT_FALSE(IsBeamTurn(2));
	EXPECT_TRUE(IsBeamTurn(3));
	EXPECT_TRUE(IsBeamTurn(3000));
}

// A beam lasts a second and a half, leaves and arrives along the line between the two points, and fades in to 40 to 74
TEST(CreatureSwap, ABeamRunsAlongTheLineBetweenTheBodies)
{
	const auto beam = Beam({0.0f, 2.0f, 0.0f}, {3.0f, 2.0f, 4.0f}, 0);
	EXPECT_EQ(beam.start, glm::vec3(0.0f, 2.0f, 0.0f));
	EXPECT_EQ(beam.end, glm::vec3(3.0f, 2.0f, 4.0f));
	EXPECT_NEAR(beam.startTangent.x, 0.6f, 1e-3f);
	EXPECT_NEAR(beam.startTangent.y, 0.0f, 1e-6f);
	EXPECT_NEAR(beam.startTangent.z, 0.8f, 1e-3f);
	EXPECT_EQ(beam.startTangent, beam.endTangent);
	EXPECT_FLOAT_EQ(beam.life, 1.5f);
	EXPECT_FLOAT_EQ(beam.speed, 1.0f);
	EXPECT_EQ(beam.alpha, 40);
	EXPECT_EQ(Beam({}, {1.0f, 0.0f, 0.0f}, k_BeamAlphaDraws - 1).alpha, 74);
	// Two points all but the same: straight up
	const auto still = Beam({1.0f, 1.0f, 1.0f}, {1.001f, 1.0f, 1.0f}, 5);
	EXPECT_NEAR(still.startTangent.y, 1.0f, 1e-3f);
	EXPECT_FLOAT_EQ(still.startTangent.x, 0.0f);
	EXPECT_EQ(still.alpha, 45);
}

// Each turn the body closes 4% of the gap to its new size and alignment
TEST(CreatureSwap, EachTurnClosesAShareOfTheGap)
{
	const auto next = Ease({.size = 1.0f, .alignment = 0.0f}, {.size = 2.0f, .alignment = -1.0f});
	EXPECT_FLOAT_EQ(next.size, 1.04f);
	EXPECT_FLOAT_EQ(next.alignment, -0.04f);
}

TEST(CreatureSwap, TheAlignmentStaysWithinItsRange)
{
	const auto next = Ease({.size = 1.0f, .alignment = 0.99f}, {.size = 1.0f, .alignment = 2.0f});
	EXPECT_FLOAT_EQ(next.alignment, 1.0f);
}

TEST(CreatureSwap, SettledWhenBothGapsAreUnderAHundredth)
{
	EXPECT_TRUE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.0099f, .alignment = 0.4901f}));
	EXPECT_FALSE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.011f, .alignment = 0.5f}));
	EXPECT_FALSE(IsSettled({.size = 1.0f, .alignment = 0.5f}, {.size = 1.0f, .alignment = 0.489f}));
}

// Growing by a whole size takes about 113 turns, eleven seconds at ten turns a second
TEST(CreatureSwap, AWholeSizeTakesAboutElevenSeconds)
{
	Shape body {.size = 1.0f, .alignment = 0.0f};
	const Shape target {.size = 2.0f, .alignment = 0.0f};
	int turns = 0;
	while (!IsSettled(body, target) && turns < 1000)
	{
		body = Ease(body, target);
		++turns;
	}
	EXPECT_NEAR(turns, 113, 1);
}

// The mind keeps the same share of the sightings each miracle needs in its new species, rounded down
TEST(CreatureSwap, SightingsKeepTheirShareInTheNewSpecies)
{
	EXPECT_EQ(RescaledSightings(10, 10.0f, 15.0f), 15u);
	EXPECT_EQ(RescaledSightings(7, 10.0f, 5.0f), 3u);
	EXPECT_EQ(RescaledSightings(7, 0.0f, 5.0f), 7u);
	EXPECT_EQ(RescaledSightings(0, 10.0f, 5.0f), 0u);
}

// The swap waits for the spells on both creatures to end, bringing its own to their end meanwhile
TEST(CreatureSwap, TheSwapWaitsForTheSpellsToEnd)
{
	auto mind = Swapping();
	const auto commands = creature_mind::Think(mind, SwapSenses(false, false), NoRandom);
	EXPECT_TRUE(commands.finishSpells);
	EXPECT_FALSE(commands.playOnce.has_value());
	EXPECT_FALSE(mind.stepStarted);
}

// Then its action plays and the beam starts, once its body is free
TEST(CreatureSwap, TheActionAndTheBeamStartOnceTheBodyIsFree)
{
	auto mind = Swapping();
	auto commands = creature_mind::Think(mind, SwapSenses(true, false, true), NoRandom);
	EXPECT_FALSE(commands.playOnce.has_value());
	EXPECT_FALSE(commands.finishSpells);
	commands = creature_mind::Think(mind, SwapSenses(true, false), NoRandom);
	EXPECT_EQ(commands.playOnce, 109u);
	EXPECT_TRUE(commands.startSwapBeam);
	EXPECT_FALSE(commands.easeSwap);
}

// From the next turn it takes on its new shape, done once it has, whether or not the action has played out
TEST(CreatureSwap, ItEasesUntilSettled)
{
	auto mind = Swapping();
	static_cast<void>(creature_mind::Think(mind, SwapSenses(true, false), NoRandom));
	auto commands = creature_mind::Think(mind, SwapSenses(true, false, true), NoRandom);
	EXPECT_TRUE(commands.easeSwap);
	EXPECT_EQ(mind.step, 0u);
	commands = creature_mind::Think(mind, SwapSenses(true, true, true), NoRandom);
	EXPECT_FALSE(commands.easeSwap);
	EXPECT_EQ(mind.step, 1u);
}
