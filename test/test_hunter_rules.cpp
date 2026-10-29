/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Animals/HunterRules.h"

using namespace openblack;
using namespace openblack::animals;
using namespace openblack::animals::hunters;

namespace
{
/// The distances of every hunter's table, in metres
constexpr float k_Attack = 50.0f;
constexpr float k_Stalking = 100.0f;
constexpr float k_Hunting = 100.0f;

/// A float random that gives the same share of whatever it is asked for
auto Share(float share)
{
	return [share](float max) { return max * share; };
}
} // namespace

TEST(HunterRules, OnlyLionsTigersLeopardsAndWolvesAreWildHunters)
{
	EXPECT_TRUE(IsWildHunter(AnimalInfo::Lion));
	EXPECT_TRUE(IsWildHunter(AnimalInfo::Tiger));
	EXPECT_TRUE(IsWildHunter(AnimalInfo::Leopard));
	EXPECT_TRUE(IsWildHunter(AnimalInfo::Wolf));
	EXPECT_FALSE(IsWildHunter(AnimalInfo::SpellWolf));
	EXPECT_FALSE(IsWildHunter(AnimalInfo::Cow));
	EXPECT_FALSE(ClipsOf(AnimalInfo::SpellWolf).has_value());
}

TEST(HunterRules, EachKindHasItsOwnClips)
{
	const auto lion = ClipsOf(AnimalInfo::Lion);
	ASSERT_TRUE(lion.has_value());
	EXPECT_EQ(lion->pounce, AnimId::ALionPounceHi);
	EXPECT_EQ(lion->prowl, AnimId::ALionProwl);
	const auto wolf = ClipsOf(AnimalInfo::Wolf);
	ASSERT_TRUE(wolf.has_value());
	EXPECT_EQ(wolf->pounce, AnimId::AWolfPounce);
	EXPECT_EQ(wolf->upFromEat, AnimId::AWolfUpfromEat);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Lion), SpeedThreshold::AnimalLion);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Tiger), SpeedThreshold::AnimalTiger);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Leopard), SpeedThreshold::AnimalLeopard);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Wolf), SpeedThreshold::AnimalWolf);
}

TEST(HunterRules, SlowerThanItsUsualSpeedItProwlsAndFasterThanItsRunSpeedItRuns)
{
	const auto clips = *ClipsOf(AnimalInfo::Lion);
	EXPECT_EQ(MoveClip(clips, 819, 983, 4000), AnimId::ALionProwl);
	EXPECT_EQ(MoveClip(clips, 983, 983, 4000), AnimId::ALionWalk);
	EXPECT_EQ(MoveClip(clips, 4000, 983, 4000), AnimId::ALionWalk);
	EXPECT_EQ(MoveClip(clips, 4001, 983, 4000), AnimId::ALionRun);
}

TEST(HunterRules, TheChaseSpeedComesFromTheDistanceToThePrey)
{
	EXPECT_EQ(ChaseSpeed(49.9f, k_Attack, k_Stalking, k_Hunting, 0), k_SprintSpeed);
	EXPECT_EQ(ChaseSpeed(50.0f, k_Attack, k_Stalking, k_Hunting, 0), k_StalkSpeed);
	EXPECT_EQ(ChaseSpeed(99.9f, k_Attack, k_Stalking, k_Hunting, 0), k_StalkSpeed);
	EXPECT_FALSE(ChaseSpeed(100.0f, k_Attack, k_Stalking, k_Hunting, 0).has_value());
}

TEST(HunterRules, BetweenItsStalkingAndHuntingDistancesItCreepsForAThirdOfTheTurns)
{
	// The tables give no hunter such a band; with one it creeps for turns 0 to 32 of each 100
	EXPECT_EQ(ChaseSpeed(80.0f, 10.0f, 50.0f, 100.0f, 132), k_StalkSpeed);
	EXPECT_EQ(ChaseSpeed(80.0f, 10.0f, 50.0f, 100.0f, 133), k_HuntSpeed);
	EXPECT_EQ(ChaseSpeed(80.0f, 10.0f, 50.0f, 100.0f, 199), k_HuntSpeed);
}

TEST(HunterRules, CatsSleepAfterTwentyTwoHoursAndWolvesHuntAfterTwentyThree)
{
	EXPECT_FALSE(CatsSleepAt(22.0f));
	EXPECT_TRUE(CatsSleepAt(22.01f));
	EXPECT_FALSE(WolvesHuntAt(23.0f));
	EXPECT_TRUE(WolvesHuntAt(23.5f));
	EXPECT_FALSE(WolvesHuntAt(3.0f));
}

TEST(HunterRules, ItBreedsFirstThenEatsThenSleeps)
{
	const grazers::NeedLimits limits {.hunger = 120, .sleep = 6000, .breed = 3000, .grownUpAge = 0};
	grazers::Needs needs {.hunger = 120, .sleep = 6000, .breed = 3000};
	EXPECT_EQ(FirstNeed(needs, limits, grazers::HerdSize {.members = 3, .most = 4}), Need::Breed);
	EXPECT_EQ(needs.breed, 3000);
	// A flock that lost no members does not breed: the need starts again
	EXPECT_EQ(FirstNeed(needs, limits, grazers::HerdSize {.members = 4, .most = 4}), Need::Hunger);
	EXPECT_EQ(needs.breed, 0);
	needs.hunger = 119;
	EXPECT_EQ(FirstNeed(needs, limits, std::nullopt), Need::Sleep);
	needs.sleep = 0;
	EXPECT_EQ(FirstNeed(needs, limits, std::nullopt), Need::None);
}

TEST(HunterRules, AWolfPackGoesOnlyToAWeakerFlockOnTheGroundNearItsPlace)
{
	// The distance is in map units and counts in hundreds against the reach in metres
	const std::array flocks {
	    FlockSeen {.leaderStrength = 0.2f, .height = 0.0f, .distance = 9000}, // 90 hundreds: too far
	    FlockSeen {.leaderStrength = 0.9f, .height = 0.0f, .distance = 100},  // stronger
	    FlockSeen {.leaderStrength = 0.2f, .height = 3.0f, .distance = 100},  // in the air
	    FlockSeen {.leaderStrength = 0.4f, .height = 2.0f, .distance = 7999}, // 79 hundreds
	    FlockSeen {.leaderStrength = 0.1f, .height = 0.0f, .distance = 7950}, // also 79: the first one wins
	};
	EXPECT_EQ(PackGoal(flocks, 0.4f, 80), 3U);
	EXPECT_FALSE(PackGoal(std::span(flocks).first(3), 0.4f, 80).has_value());
}

TEST(HunterRules, ThePointInTheSquareIsAboutTheCentre)
{
	const glm::ivec2 centre {map_coords::ToFixed(100.0f), map_coords::ToFixed(200.0f)};
	// The middle of the random range is the centre itself
	EXPECT_EQ(RandomPointInSquare(centre, 4.0f, Share(0.5f)), centre);
	const auto corner = RandomPointInSquare(centre, 4.0f, Share(0.0f));
	EXPECT_NEAR(map_coords::ToMetres(corner.x), 102.0f, 0.01f);
	EXPECT_NEAR(map_coords::ToMetres(corner.y), 202.0f, 0.01f);
}
