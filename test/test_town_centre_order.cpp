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

#include "ECS/TownCentreOrder.h"

using namespace openblack::ecs;

namespace
{
constexpr auto k_First = static_cast<entt::entity>(1);
constexpr auto k_Second = static_cast<entt::entity>(2);
constexpr auto k_Third = static_cast<entt::entity>(3);
constexpr auto k_Fourth = static_cast<entt::entity>(4);
} // namespace

TEST(TownCentreOrder, TheCentresOfALandDrawNewestFirst)
{
	std::vector<entt::entity> order;
	const std::vector present {k_Third, k_Second, k_First};
	town_centre_order::Update(order, present);
	EXPECT_EQ(order, present);
}

TEST(TownCentreOrder, ANewCentreGoesFirst)
{
	std::vector order {k_Second, k_First};
	town_centre_order::Update(order, std::vector {k_Third, k_Second, k_First});
	EXPECT_EQ(order, (std::vector {k_Third, k_Second, k_First}));
}

TEST(TownCentreOrder, ACentreThatGoesKeepsTheOrderOfTheOthers)
{
	std::vector order {k_Third, k_Second, k_First};
	// The registry can give the remaining centres in another order after a removal
	town_centre_order::Update(order, std::vector {k_First, k_Third});
	EXPECT_EQ(order, (std::vector {k_Third, k_First}));
}

TEST(TownCentreOrder, ANewCentreAfterARemovalStillGoesFirst)
{
	std::vector order {k_Third, k_Second, k_First};
	town_centre_order::Update(order, std::vector {k_Fourth, k_First, k_Third});
	EXPECT_EQ(order, (std::vector {k_Fourth, k_Third, k_First}));
}
