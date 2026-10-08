/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// TreeSystem: the trees' shared state. AnyBent tells the bend pass whether the last pass left a tree bent; it starts
// true, so the first pass always runs.

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/TreeSystem.h"

using openblack::ecs::systems::TreeSystem;

TEST(TreeSystem, TheFirstBendPassAlwaysRuns)
{
	const TreeSystem trees;
	EXPECT_TRUE(trees.AnyBent());
}

TEST(TreeSystem, AnyBentIsWhatTheLastPassLeft)
{
	TreeSystem trees;
	trees.SetAnyBent(false);
	EXPECT_FALSE(trees.AnyBent());
	trees.SetAnyBent(true);
	EXPECT_TRUE(trees.AnyBent());
}
