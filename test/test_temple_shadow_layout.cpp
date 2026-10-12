/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/DrawLayoutComponents.h"

namespace components = openblack::ecs::components;
using openblack::ecs::k_ChangesDrawLayout;

// A temple casts its shadow once built: the draw lists are made again when it is made and when its building ends
TEST(TempleShadowLayout, TempleAndBuildingChangeTheDrawLists)
{
	EXPECT_TRUE(k_ChangesDrawLayout<components::Temple>);
	EXPECT_TRUE(k_ChangesDrawLayout<const components::BuildProgress>);
	EXPECT_TRUE(k_ChangesDrawLayout<components::Abode>);
}
