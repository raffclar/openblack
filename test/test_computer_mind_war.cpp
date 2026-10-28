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

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/ComputerMindWarRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindWar, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionDestroyTown), MindDomain::War);
	EXPECT_EQ(DomainOf(NodeKind::ActionAttackCreature), MindDomain::War);
	EXPECT_EQ(DomainOf(NodeKind::ObjectThrowrockatCreature), MindDomain::War);
}

TEST(ComputerMindWar, WantsANearCreatureFully)
{
	EXPECT_FLOAT_EQ(AttackCreatureWish(99.0f, 0.2f), 1.0f);
	// The creature at the near distance itself takes the nearness
	EXPECT_FLOAT_EQ(AttackCreatureWish(100.0f, 0.8f), 0.8f);
	EXPECT_FLOAT_EQ(AttackCreatureWish(400.0f, 0.2f), 0.2f);
}

TEST(ComputerMindWar, NearnessOfACreatureFallsOverItsReach)
{
	const float near = gutils::GetDistanceModifier(150.0f, k_AttackCreatureReach);
	const float far = gutils::GetDistanceModifier(450.0f, k_AttackCreatureReach);
	EXPECT_GT(near, far);
	EXPECT_FLOAT_EQ(AttackCreatureWish(150.0f, near), near);
}

TEST(ComputerMindWar, BuildingValueIsLifeTimesBuiltTimesDesire)
{
	EXPECT_FLOAT_EQ(BuildingTargetValue(0.5f, 0.8f, 2.0f), 0.8f);
	EXPECT_FLOAT_EQ(BuildingTargetValue(1.0f, 1.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(BuildingTargetValue(0.0f, 1.0f, 3.0f), 0.0f);
}

TEST(ComputerMindWar, FieldValueIsShareOfFoodTimesDesire)
{
	EXPECT_FLOAT_EQ(FieldTargetValue(50.0f, 200.0f, 2.0f), 0.5f);
	EXPECT_FLOAT_EQ(FieldTargetValue(0.0f, 200.0f, 2.0f), 0.0f);
}

TEST(ComputerMindWar, NearTargetValueScalesByNearness)
{
	EXPECT_FLOAT_EQ(NearTargetValue(2.0f, 0.25f), 0.5f);
}

TEST(ComputerMindWar, MostWantedTargetIsTheFirstLargestAboveZero)
{
	EXPECT_FALSE(MostWantedTarget({}).has_value());
	const std::vector<float> none {0.0f, -1.0f, 0.0f};
	EXPECT_FALSE(MostWantedTarget(none).has_value());
	const std::vector<float> values {0.2f, 0.7f, 0.7f, 0.1f};
	EXPECT_EQ(MostWantedTarget(values), std::optional<size_t> {1});
	const std::vector<float> last {0.0f, 0.0f, 0.3f};
	EXPECT_EQ(MostWantedTarget(last), std::optional<size_t> {2});
}

TEST(ComputerMindWar, RockScoreNeedsAThrowableRockOrTree)
{
	EXPECT_FLOAT_EQ(RockScore({.throwable = true, .destroysBuildings = true, .weight = 40.0f}), 40.0f);
	EXPECT_FLOAT_EQ(RockScore({.throwable = true, .tree = true, .weight = 12.0f}), 12.0f);
	EXPECT_FLOAT_EQ(RockScore({.throwable = true, .weight = 12.0f}), 0.0f);
	EXPECT_FLOAT_EQ(RockScore({.throwable = false, .destroysBuildings = true, .weight = 40.0f}), 0.0f);
}
