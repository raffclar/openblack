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

#include "Creature/PrimaryCreature.h"

using namespace openblack;

namespace
{
constexpr auto k_First = static_cast<entt::entity>(4);
constexpr auto k_Second = static_cast<entt::entity>(2);
constexpr auto k_Third = static_cast<entt::entity>(9);
} // namespace

TEST(PrimaryCreature, CreaturesAreKeptInTheOrderGot)
{
	std::vector<entt::entity> acquired;
	primary_creature::Acquire(acquired, k_First);
	primary_creature::Acquire(acquired, k_Second);
	primary_creature::Acquire(acquired, k_First);
	primary_creature::Acquire(acquired, k_Third);
	EXPECT_EQ(acquired, (std::vector<entt::entity> {k_First, k_Second, k_Third}));
}

TEST(PrimaryCreature, TheEarliestGotIsPrimaryWhateverItsNumber)
{
	const std::vector<entt::entity> acquired {k_First, k_Second, k_Third};
	EXPECT_EQ(primary_creature::Primary(acquired, [](entt::entity) { return true; }), k_First);
}

TEST(PrimaryCreature, ACreatureGoneOrGivenAwayPassesItOnToTheNext)
{
	const std::vector<entt::entity> acquired {k_First, k_Second, k_Third};
	EXPECT_EQ(primary_creature::Primary(acquired, [](entt::entity creature) { return creature != k_First; }), k_Second);
	EXPECT_FALSE(primary_creature::Primary(acquired, [](entt::entity) { return false; }).has_value());
	EXPECT_FALSE(primary_creature::Primary({}, [](entt::entity) { return true; }).has_value());
}
