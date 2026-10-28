/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Graphics/DrawnHand.h"

using namespace openblack::graphics;

namespace
{
// Fake entities: the hand of the god of this computer, the hand of another god and a thing that is not a hand
constexpr auto k_OwnHand = entt::entity {3};
constexpr auto k_OtherGodHand = entt::entity {1211};
constexpr auto k_RightHand = entt::entity {4};
constexpr auto k_Barrel = entt::entity {20};
} // namespace

// The frame draws the hand of the god of this computer
TEST(DrawnHand, TheOwnHandShows)
{
	EXPECT_TRUE(drawn_hand::Drawn(true, k_OwnHand, k_OwnHand));
}

// The game draws no model for the hand of another god or for the right hand
TEST(DrawnHand, TheOtherHandsDoNotShow)
{
	EXPECT_FALSE(drawn_hand::Drawn(true, k_OtherGodHand, k_OwnHand));
	EXPECT_FALSE(drawn_hand::Drawn(true, k_RightHand, k_OwnHand));
}

// With no hand of this computer, no hand shows
TEST(DrawnHand, NoHandShowsWithNoOwnHand)
{
	EXPECT_FALSE(drawn_hand::Drawn(true, k_OwnHand, entt::null));
	EXPECT_FALSE(drawn_hand::Drawn(true, entt::null, entt::null));
}

// A thing that is not a hand shows as before
TEST(DrawnHand, OtherThingsShow)
{
	EXPECT_TRUE(drawn_hand::Drawn(false, k_Barrel, k_OwnHand));
	EXPECT_TRUE(drawn_hand::Drawn(false, k_Barrel, entt::null));
}
