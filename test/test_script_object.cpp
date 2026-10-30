/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/ScriptDelete.h"
#include "ECS/ScriptObject.h"

using namespace openblack::ecs;

TEST(ScriptObject, AThingWithAModelIsAnObjectButTheHandIsNot)
{
	EXPECT_TRUE(script_object::IsObject({.model = true}));
	EXPECT_FALSE(script_object::IsObject({.model = true, .hand = true}));
}

TEST(ScriptObject, CreaturesAndFieldsAreObjects)
{
	EXPECT_TRUE(script_object::IsObject({.creature = true}));
	EXPECT_TRUE(script_object::IsObject({.field = true}));
}

TEST(ScriptObject, AOneShotGlobeAndAMiracleSeedAreObjects)
{
	EXPECT_TRUE(script_object::IsObject({.globe = true}));
	EXPECT_TRUE(script_object::IsObject({.seed = true}));
}

TEST(ScriptObject, AThingWithOnlyAPlaceIsNotAnObject)
{
	EXPECT_FALSE(script_object::IsObject({}));
}

TEST(ScriptObject, DeleteWithFadeLeavesAGhostOfAGlobeAndRemovesIt)
{
	const auto globe = script_object::IsObject({.globe = true});
	EXPECT_EQ(script_delete::Choose(1, {.object = globe}), script_delete::Step::GhostThenRemove);
	EXPECT_EQ(script_delete::Choose(2, {.object = globe}), script_delete::Step::BreakUpThenRemove);
}
