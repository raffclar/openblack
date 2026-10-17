/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <string>

#include <gtest/gtest.h>

#include "3D/HandOrientation.h"
#include "ECS/VillageTotem.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	const float gameValue = std::bit_cast<float>(game);
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && gameValue == 0.0f);
}

} // namespace

TEST(OracleTotemHand, DataIsComplete)
{
	const auto table = oracle::Table::Load("totem_hand.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_GT(table->Rows(), 1000u);
	const auto headings = oracle::Table::Load("hand_heading.txt");
	ASSERT_TRUE(headings.has_value());
	EXPECT_GT(headings->Rows(), 1000u);
}

TEST(OracleTotemHand, TiltAndRowsAsTheGameDoes)
{
	const auto table = oracle::Table::Load("totem_hand.txt");
	ASSERT_TRUE(table.has_value());
	const auto handY = table->Column("y_out");
	const auto heading = table->Column("heading");
	ASSERT_TRUE(handY && heading);
	constexpr std::array<const char*, 9> k_Names {"r00", "r01", "r02", "r10", "r11", "r12", "r20", "r21", "r22"};
	std::array<size_t, 9> columns {};
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		const auto column = table->Column(k_Names.at(i));
		ASSERT_TRUE(column.has_value()) << k_Names.at(i);
		columns.at(i) = *column;
	}
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float tilt = ecs::village_totem::HandTiltAt(table->Float(row, *handY));
		const auto rows = hand_orientation::TippedRows(table->Float(row, *heading), tilt);
		for (size_t i = 0; i < 9; ++i)
		{
			const float ours = rows.at(i / 3)[static_cast<int>(i % 3)];
			if (!Same(ours, table->Bits(row, columns.at(i))))
			{
				++different;
				if (different <= 20)
				{
					ADD_FAILURE() << "row " << row << " " << k_Names.at(i) << ": height "
					              << oracle::Hex(table->Float(row, *handY)) << " heading "
					              << oracle::Hex(table->Float(row, *heading)) << ": the game gives "
					              << oracle::Hex(table->Float(row, columns.at(i))) << ", ours gives " << oracle::Hex(ours);
				}
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleTotemHand, GripTimeAsTheGameDoes)
{
	const auto table = oracle::Table::Load("totem_hand.txt");
	ASSERT_TRUE(table.has_value());
	const auto radius = table->Column("hold_radius");
	const auto clip = table->Column("clip_ms");
	const auto grip = table->Column("grip_ms");
	ASSERT_TRUE(radius && clip && grip);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = ecs::village_totem::GripTimeMs(ecs::village_totem::ClosureForHoldRadius(table->Float(row, *radius)),
		                                                 table->Bits(row, *clip));
		if (ours != table->Bits(row, *grip) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game gives " << table->Bits(row, *grip) << ", ours gives " << ours;
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleTotemHand, HeadingAngleAsTheGameDoes)
{
	const auto table = oracle::Table::Load("hand_heading.txt");
	ASSERT_TRUE(table.has_value());
	const auto x = table->Column("x");
	const auto z = table->Column("z");
	const auto angle = table->Column("angle");
	ASSERT_TRUE(x && z && angle);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const float ours = hand_orientation::HeadingAngle({table->Float(row, *x), -0.5f, table->Float(row, *z)});
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *angle) && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": x " << oracle::Hex(table->Float(row, *x)) << " z "
			              << oracle::Hex(table->Float(row, *z)) << ": the game gives " << oracle::Hex(table->Float(row, *angle))
			              << ", ours gives " << oracle::Hex(ours);
		}
	}
	EXPECT_EQ(different, 0u);
}
