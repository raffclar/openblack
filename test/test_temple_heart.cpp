/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleLanterns.h"
#include "Physics/TempleHeart.h"
#include "Temple/TempleDestruction.h"

using namespace openblack::physics::temple_heart;

namespace
{
entt::entity E(uint32_t id)
{
	return static_cast<entt::entity>(id);
}

Building Standing(uint32_t id, float life)
{
	return {.entity = E(id), .available = true, .life = life, .built = 1.0f};
}
} // namespace

TEST(TempleHeart, ASoundBuildingTakesTheBlowAtOnce)
{
	const std::vector<Town> towns {
	    {.buildings = {Standing(1, 0.2f), Standing(2, 0.3f)}},
	    {.buildings = {Standing(3, 1.0f)}},
	};
	const auto target = Choose(towns);
	EXPECT_EQ(target.kind, TargetKind::Building);
	EXPECT_EQ(target.entity, E(2));
}

TEST(TempleHeart, OtherwiseTheFirstStandingBuildingOfAnyTown)
{
	auto field = Standing(1, 1.0f);
	field.field = true;
	auto pitch = Standing(2, 1.0f);
	pitch.footballPitch = true;
	auto unbuilt = Standing(3, 1.0f);
	unbuilt.built = 0.0f;
	auto ruin = Standing(4, 0.0f);
	const std::vector<Town> towns {
	    {.buildings = {field, pitch, unbuilt, ruin}},
	    {.buildings = {Standing(5, 0.25f), Standing(6, 0.1f)}},
	};
	const auto target = Choose(towns);
	EXPECT_EQ(target.kind, TargetKind::Building);
	EXPECT_EQ(target.entity, E(5));
}

TEST(TempleHeart, ThenAHomelessVillagerThenTheHeart)
{
	const std::vector<Town> homeless {
	    {.homeless = {{.entity = E(7), .available = false}}},
	    {.homeless = {{.entity = E(8), .available = true}, {.entity = E(9), .available = true}}},
	};
	const auto villager = Choose(homeless);
	EXPECT_EQ(villager.kind, TargetKind::Villager);
	EXPECT_EQ(villager.entity, E(8));
	EXPECT_EQ(Choose(std::vector<Town> {{}}).kind, TargetKind::Heart);
}

TEST(TempleHeart, TheHeartIsHarmedByTheBlowsMomentumUpToAFifth)
{
	EXPECT_FLOAT_EQ(Harm({0.0f, 3.0f, 4.0f}, 1000.0f), 0.025f);
	EXPECT_FLOAT_EQ(Harm({0.0f, 30.0f, 40.0f}, 1000.0f), 0.2f);
}

TEST(TempleHeartBeam, EveryTwoSecondsAtTheSameTargetAndAtOnceAtANewOne)
{
	EXPECT_EQ(BeamInterval(100), 20u);
	Beam beam;
	const auto first = static_cast<entt::entity>(1);
	const auto second = static_cast<entt::entity>(2);
	EXPECT_TRUE(BeamAtTargetDue(beam, first, 21, 20));
	EXPECT_EQ(beam.turn, 21u);
	// Strictly more than the interval later
	EXPECT_FALSE(BeamAtTargetDue(beam, first, 41, 20));
	EXPECT_TRUE(BeamAtTargetDue(beam, first, 42, 20));
	// A new target starts the wait afresh, from nothing
	EXPECT_TRUE(BeamAtTargetDue(beam, second, 43, 20));
	// Taking the blow itself forgets the target, and beams at itself afresh too
	EXPECT_TRUE(BeamAtItselfDue(beam, 44, 20));
	EXPECT_TRUE(beam.target == entt::null);
	EXPECT_FALSE(BeamAtItselfDue(beam, 50, 20));
	// Too early in the game for any beam
	Beam early;
	EXPECT_FALSE(BeamAtTargetDue(early, first, 20, 20));
	EXPECT_EQ(early.target, first);
}

TEST(TempleHeartDamage, OnlyAHurtHeartMakesTheEffect)
{
	EXPECT_FALSE(DamageEffectLife(1.0f).has_value());
	EXPECT_FALSE(DamageEffectLife(1.5f).has_value());
	EXPECT_FLOAT_EQ(DamageEffectLife(0.5f).value(), 0.5f);
	EXPECT_FLOAT_EQ(DamageEffectLife(-0.2f).value(), 0.0f);
}

TEST(TempleHeartDamage, TheWaitShrinksFromThreeSecondsToFourTenths)
{
	// 100 ms turns: 10 turns a second
	EXPECT_EQ(DamageInterval(100, 0.0f), 4u);
	EXPECT_EQ(DamageInterval(100, 1.0f), 30u);
	// 1.7 s of turns in float, cut short to whole turns
	EXPECT_EQ(DamageInterval(100, 0.5f), 16u);
	EXPECT_LT(DamageInterval(100, 0.2f), DamageInterval(100, 0.8f));
}

TEST(TempleHeartDamage, ABeamIsDueOnceTheIntervalIsStrictlyPast)
{
	uint32_t last = 10;
	EXPECT_FALSE(DamageBeamDue(last, 14, 4));
	EXPECT_EQ(last, 10u);
	EXPECT_TRUE(DamageBeamDue(last, 15, 4));
	EXPECT_EQ(last, 15u);
	EXPECT_FALSE(DamageBeamDue(last, 16, 4));
}

TEST(TempleHeartDamage, ItSoundsOnlyAtSixtyPercentLifeOrLess)
{
	EXPECT_TRUE(DamageBeamSounds(0.0f));
	EXPECT_TRUE(DamageBeamSounds(0.6f));
	EXPECT_FALSE(DamageBeamSounds(0.61f));
	EXPECT_FALSE(DamageBeamSounds(0.99f));
}

TEST(TempleHeartDamage, TheSoundsGoRoundFromSample206)
{
	uint32_t counter = 0;
	std::vector<uint32_t> samples;
	for (int i = 0; i < 7; ++i)
	{
		samples.push_back(NextDamageSample(counter));
	}
	EXPECT_EQ(samples, (std::vector<uint32_t> {206, 207, 208, 209, 210, 206, 207}));
}

TEST(TempleLanterns, TheyAppearOnceMoreThanSevenPercentIsBuilt)
{
	namespace tl = openblack::temple_lanterns;
	EXPECT_FALSE(tl::Present(0.0f));
	EXPECT_FALSE(tl::Present(0.07f));
	EXPECT_TRUE(tl::Present(0.08f));
	EXPECT_TRUE(tl::Present(1.0f));
}

TEST(TempleLanterns, TheyStandWithTheTemplesScaleAndTurn)
{
	namespace tl = openblack::temple_lanterns;
	const glm::vec3 position(100.0f, 5.0f, 200.0f);
	const glm::vec3 first = tl::WorldPosition(0, position, glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FLOAT_EQ(first.x, 100.0f - 8.85f);
	EXPECT_FLOAT_EQ(first.y, 5.0f);
	EXPECT_FLOAT_EQ(first.z, 200.0f - 10.34f);
	const glm::vec3 scaled = tl::WorldPosition(1, position, glm::mat3(1.0f), glm::vec3(2.0f));
	EXPECT_FLOAT_EQ(scaled.x, 100.0f - 4.6f);
	EXPECT_FLOAT_EQ(scaled.z, 200.0f - 26.8f);
}

TEST(TempleLanterns, TheyAreNotDrawnPastTwentyOnePointEightSecondsOfDestruction)
{
	namespace td = openblack::temple_destruction;
	EXPECT_TRUE(td::LanternsShown(0.0f));
	EXPECT_TRUE(td::LanternsShown(21.8f));
	EXPECT_FALSE(td::LanternsShown(21.9f));
}
