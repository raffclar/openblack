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
#include <string>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Magic/HandHoldPose.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// Counts the rows that differ and reports the first few
class Mismatches
{
public:
	void Check(bool same, size_t index, const std::string& text)
	{
		if (!same)
		{
			if (_count < 5)
			{
				ADD_FAILURE() << "row " << index << ":" << text;
			}
			++_count;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

[[nodiscard]] float Cell(const oracle::Table& table, size_t row, const std::string& name)
{
	return table.Float(row, table.Column(name).value_or(0));
}

[[nodiscard]] uint32_t CellBits(const oracle::Table& table, size_t row, const std::string& name)
{
	return table.Bits(row, table.Column(name).value_or(0));
}

} // namespace

TEST(OracleHeldFrameHeading, TheHeadingFromTheLineOfSight)
{
	const auto table = oracle::Table::Load("held_heading.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 line {Cell(*table, index, "x"), Cell(*table, index, "y"), Cell(*table, index, "z")};
		const bool takes = CellBits(*table, index, "takes") != 0;
		const auto heading = magic::hand_hold::HeadingOfSight(line);
		std::string text;
		bool same = heading.has_value() == takes;
		if (same && heading.has_value())
		{
			same = std::bit_cast<uint32_t>(*heading) == CellBits(*table, index, "heading");
			text = " game " + oracle::Hex(Cell(*table, index, "heading")) + " ours " + oracle::Hex(*heading);
		}
		else if (!same)
		{
			text = takes ? " the game takes a heading, ours does not" : " ours takes a heading, the game does not";
		}
		mismatches.Check(same, index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleHeldFrameHeading, TheFrameOfAHeldThingFromTheHeading)
{
	const auto table = oracle::Table::Load("held_frame.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 up {Cell(*table, index, "ux"), Cell(*table, index, "uy"), Cell(*table, index, "uz")};
		const auto facing = magic::hand_hold::HeldFacing(Cell(*table, index, "heading"));
		const auto basis = magic::hand_hold::HeldBasis(facing, up);
		std::string text;
		bool same = true;
		for (int i = 0; i < 9; ++i)
		{
			const auto name = "r" + std::to_string(i);
			const float ours = basis[i / 3][i % 3];
			// The game then multiplies the frame by the hand's scale. That product adds zero terms and can change the
			// sign of a zero part. openblack applies the scale in a different step, so a zero of either sign is equal.
			const auto game = CellBits(*table, index, name);
			const bool bothZero = ours == 0.0f && std::bit_cast<float>(game) == 0.0f;
			if (std::bit_cast<uint32_t>(ours) != game && !bothZero)
			{
				same = false;
				text += " " + name + " game " + oracle::Hex(Cell(*table, index, name)) + " ours " + oracle::Hex(ours);
			}
		}
		mismatches.Check(same, index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
