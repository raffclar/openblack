/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/FelledTreeDrawRules.h"
#include "Graphics/ObjectLandLight.h"

using namespace openblack;

TEST(FelledTreeDrawRules, AFelledTreeIsDrawnTwoTimesFirstAsItBurnsThenWithTheUsualLook)
{
	using ecs::felled_tree_draw::Look;
	EXPECT_EQ(ecs::felled_tree_draw::k_Draws, 2u);
	ASSERT_EQ(ecs::felled_tree_draw::k_Looks.size(), ecs::felled_tree_draw::k_Draws);
	EXPECT_EQ(ecs::felled_tree_draw::k_Looks.front(), Look::AsBurning);
	EXPECT_EQ(ecs::felled_tree_draw::k_Looks.back(), Look::Usual);
}

TEST(ObjectLandLight, TheScaffoldPhantomsTakeTheLightOfTheirPlaceOnTheMapWithNoHaze)
{
	using graphics::object_land_light::Rule;
	EXPECT_EQ(graphics::object_land_light::Of(true), Rule::AtMapPlace);
	EXPECT_FALSE(graphics::object_land_light::Hazed(Rule::AtMapPlace));
	EXPECT_EQ(graphics::object_land_light::UniformValue(Rule::AtMapPlace), 1.0f);
}

TEST(ObjectLandLight, OtherModelsTakeTheLightOfTheirPointAndTheHaze)
{
	using graphics::object_land_light::Rule;
	EXPECT_EQ(graphics::object_land_light::Of(false), Rule::AtPoint);
	EXPECT_TRUE(graphics::object_land_light::Hazed(Rule::AtPoint));
	EXPECT_EQ(graphics::object_land_light::UniformValue(Rule::AtPoint), 0.0f);
}
