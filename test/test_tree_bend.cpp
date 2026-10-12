/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the hand bends the trees with: never the empty hand, only the thing it holds, as far as its drawn model reaches

#include <optional>

#include <gtest/gtest.h>

#include "Nature/TreeBend.h"

using namespace openblack::nature;

TEST(TreeBend, TheEmptyHandBendsNothing)
{
	EXPECT_FALSE(tree_bend::HandSource(std::nullopt).has_value());
}

TEST(TreeBend, AHeldThingBendsFromItsModelAsFarAsItsScaledSize)
{
	const auto source =
	    tree_bend::HandSource(tree_bend::HeldModel {.position = {10.0f, 5.0f, 20.0f}, .scale = 2.0f, .halfDiagonal = 1.5f});
	ASSERT_TRUE(source.has_value());
	EXPECT_EQ(source->position, glm::vec3(10.0f, 5.0f, 20.0f));
	EXPECT_FLOAT_EQ(source->radius, 3.0f);
}

TEST(TreeBend, AMirroredModelStillReachesItsSize)
{
	const auto source = tree_bend::HandSource(tree_bend::HeldModel {.position = {}, .scale = -0.5f, .halfDiagonal = 4.0f});
	ASSERT_TRUE(source.has_value());
	EXPECT_FLOAT_EQ(source->radius, 2.0f);
}
