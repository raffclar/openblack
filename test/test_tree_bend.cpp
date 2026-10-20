/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What bends the trees: the thing that the hand holds, the moving bodies and the creature write bend points into
// slots, and each tree reads the point in the slot that marked it last

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

TEST(TreeBend, AMirroredModelReachesNoTree)
{
	const auto source = tree_bend::HandSource(tree_bend::HeldModel {.position = {}, .scale = -0.5f, .halfDiagonal = 4.0f});
	ASSERT_TRUE(source.has_value());
	EXPECT_FLOAT_EQ(source->radius, -2.0f);
	EXPECT_FALSE(tree_bend::ReadBend(*source, {1.0f, 0.0f, 0.0f}, 10.0f).has_value());
}

TEST(TreeBend, TheMovingBodiesTakeTheSlotsFromThreeToThirteenInTurn)
{
	tree_bend::BendTable table;
	EXPECT_EQ(table.NextCycledSlot(), 3);
	for (uint8_t expected = 3; expected <= 13; ++expected)
	{
		EXPECT_EQ(table.Write({.position = {static_cast<float>(expected), 0.0f, 0.0f}, .radius = 1.0f}), expected);
	}
	// After the thirteenth slot, the cycle starts again at the third and writes over its old point
	EXPECT_EQ(table.Write({.position = {99.0f, 0.0f, 0.0f}, .radius = 2.0f}), 3);
	EXPECT_FLOAT_EQ(table.At(3).position.x, 99.0f);
	EXPECT_FLOAT_EQ(table.At(4).position.x, 4.0f);
}

TEST(TreeBend, TheHandAndTheCreatureKeepTheirOwnSlotsAndDoNotMoveTheCycle)
{
	tree_bend::BendTable table;
	EXPECT_EQ(table.Write({.position = {1.0f, 0.0f, 0.0f}, .radius = 1.0f}, tree_bend::k_HandSlot), 1);
	EXPECT_EQ(table.Write({.position = {2.0f, 0.0f, 0.0f}, .radius = 1.0f}, tree_bend::k_CreatureSlot), 2);
	EXPECT_EQ(table.NextCycledSlot(), 3);
	// A slot that nothing wrote has no reach, so it bends no tree
	EXPECT_FALSE(tree_bend::ReadBend(table.At(5), {0.0f, 0.0f, 0.0f}, 10.0f).has_value());
}

TEST(TreeBend, APointMarksTheThreeByThreeCellsRoundItsOwnCell)
{
	const auto cells = tree_bend::CellsMarkedBy({125.0f, 3.0f, 9.99f});
	EXPECT_EQ(cells.first, glm::ivec2(11, -1));
	EXPECT_EQ(cells.last, glm::ivec2(13, 1));
	// The cell is the tenth of the position, cut towards zero
	EXPECT_EQ(tree_bend::CellOf(-3.0f), 0);
	EXPECT_EQ(tree_bend::CellOf(-13.0f), -1);
	EXPECT_EQ(tree_bend::CellOf(19.99f), 1);
}

TEST(TreeBend, ATreeBendsAwayLessTheFurtherThePointIs)
{
	const tree_bend::Source source {.position = {0.0f, 2.0f, 0.0f}, .radius = 4.0f};
	const auto near = tree_bend::ReadBend(source, {1.0f, 0.0f, 0.0f}, 10.0f);
	const auto far = tree_bend::ReadBend(source, {0.0f, 0.0f, 3.0f}, 10.0f);
	ASSERT_TRUE(near.has_value());
	ASSERT_TRUE(far.has_value());
	// 1 - (((4 - 0.75) x 1 / 4) + 0.75) / 4
	EXPECT_EQ(near->amount, 1.0f - ((((4.0f - 0.75f) * 1.0f) / 4.0f) + 0.75f) / 4.0f);
	EXPECT_FLOAT_EQ(near->amount, 0.609375f);
	EXPECT_EQ(near->direction, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_FLOAT_EQ(far->amount, 0.20312500f);
	EXPECT_EQ(far->direction, glm::vec3(0.0f, 0.0f, 1.0f));
}

TEST(TreeBend, APointOfSmallReachPullsTheTreeTowardsIt)
{
	const auto bend = tree_bend::ReadBend({.position = {0.0f, 0.0f, 0.0f}, .radius = 0.5f}, {0.25f, 0.0f, 0.0f}, 10.0f);
	ASSERT_TRUE(bend.has_value());
	EXPECT_LT(bend->amount, 0.0f);
}

TEST(TreeBend, ATreeLosesItsMarkWhenThePointIsAboveItOrOutOfReach)
{
	const glm::vec3 tree {0.0f, 1.0f, 0.0f};
	// The top of the tree is at 1 + 5 = 6
	EXPECT_FALSE(tree_bend::ReadBend({.position = {1.0f, 6.5f, 0.0f}, .radius = 4.0f}, tree, 5.0f).has_value());
	EXPECT_TRUE(tree_bend::ReadBend({.position = {1.0f, 6.0f, 0.0f}, .radius = 4.0f}, tree, 5.0f).has_value());
	// Height does not count for the reach: only the distance across the ground
	EXPECT_TRUE(tree_bend::ReadBend({.position = {3.9f, -50.0f, 0.0f}, .radius = 4.0f}, tree, 5.0f).has_value());
	EXPECT_FALSE(tree_bend::ReadBend({.position = {4.0f, 2.0f, 0.0f}, .radius = 4.0f}, tree, 5.0f).has_value());
}

TEST(TreeBend, APointAtTheTrunkKeepsTheMarkButDoesNotBend)
{
	const auto bend = tree_bend::ReadBend({.position = {2.0f, 0.0f, 2.0f}, .radius = 3.0f}, {2.0f, 0.0f, 2.0f}, 5.0f);
	ASSERT_TRUE(bend.has_value());
	EXPECT_EQ(bend->amount, 0.0f);
}
