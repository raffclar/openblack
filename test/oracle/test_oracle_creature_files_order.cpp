/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureGesture.h"
#include "OracleData.h"

using namespace openblack;

// The breath period that the body sets when it stops: five divided by one over the square root of the size
TEST(OracleCreatureFilesOrder, TheBreathPeriodIsTheGames)
{
	const auto table = oracle::Table::Load("creature_breath_period.txt");
	ASSERT_TRUE(table.has_value());
	const auto size = table->Column("size");
	const auto period = table->Column("period");
	ASSERT_TRUE(size && period);
	ASSERT_GE(table->Rows(), 400u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(creature_animation::BreathPeriod(table->Float(row, *size))),
		          table->Bits(row, *period))
		    << "row " << row;
	}
}

// The point that the hand of a creature that draws a gesture goes to, from its heading, its radius and its head
TEST(OracleCreatureFilesOrder, TheGestureHandTargetIsTheGames)
{
	const auto table = oracle::Table::Load("creature_gesture_target.txt");
	ASSERT_TRUE(table.has_value());
	const auto heading = table->Column("heading");
	const auto radius = table->Column("radius");
	const auto headX = table->Column("head_x");
	const auto shapeX = table->Column("shape_x");
	const auto targetX = table->Column("target_x");
	ASSERT_TRUE(heading && radius && headX && shapeX && targetX);
	ASSERT_GE(table->Rows(), 500u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 head {table->Float(row, *headX), table->Float(row, *headX + 1), table->Float(row, *headX + 2)};
		const glm::vec3 shape {table->Float(row, *shapeX), table->Float(row, *shapeX + 1), table->Float(row, *shapeX + 2)};
		const auto target =
		    creature_gesture::HandTarget(head, GameHeading(table->Float(row, *heading)), table->Float(row, *radius), shape);
		EXPECT_EQ(std::bit_cast<uint32_t>(target.x), table->Bits(row, *targetX)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(target.y), table->Bits(row, *targetX + 1)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(target.z), table->Bits(row, *targetX + 2)) << "row " << row;
	}
}
