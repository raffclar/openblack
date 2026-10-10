/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/SeaReflection.h"

using openblack::graphics::sea_reflection::ReflectsVillager;

TEST(SeaReflection, VillagersOnTheLandAreNotReflected)
{
	EXPECT_FALSE(ReflectsVillager({}));
	EXPECT_FALSE(ReflectsVillager({.handReflected = true}));
}

TEST(SeaReflection, VillagersInThePhysicsAreReflected)
{
	EXPECT_TRUE(ReflectsVillager({.inPhysics = true}));
}

TEST(SeaReflection, HeldVillagersAreReflectedWithTheHand)
{
	EXPECT_TRUE(ReflectsVillager({.inHand = true, .handReflected = true}));
	EXPECT_FALSE(ReflectsVillager({.inHand = true, .handReflected = false}));
}
