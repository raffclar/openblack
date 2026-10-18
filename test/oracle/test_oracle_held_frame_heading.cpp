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

#include "3D/HandOrientation.h"
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
	// The hand takes a new heading only when the line leans enough across the land
	const auto table = oracle::Table::Load("held_heading.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 line {Cell(*table, index, "x"), Cell(*table, index, "y"), Cell(*table, index, "z")};
		const bool takes = CellBits(*table, index, "takes") != 0;
		bool same = hand_orientation::LeansAcrossLand(line) == takes;
		std::string text;
		if (same && takes)
		{
			const float heading = hand_orientation::HeadingAngle(line);
			same = std::bit_cast<uint32_t>(heading) == CellBits(*table, index, "heading");
			text = " game " + oracle::Hex(Cell(*table, index, "heading")) + " ours " + oracle::Hex(heading);
		}
		else if (!same)
		{
			text = " the lean test does not agree";
		}
		mismatches.Check(same, index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

namespace
{

/// Compares the nine parts of a frame with the game's, bit for bit, the sign of a zero too
bool SameFrame(const oracle::Table& table, size_t index, const glm::mat3& frame, std::string& text)
{
	bool same = true;
	for (int i = 0; i < 9; ++i)
	{
		const auto name = "r" + std::to_string(i);
		const float ours = frame[i / 3][i % 3];
		if (std::bit_cast<uint32_t>(ours) != CellBits(table, index, name))
		{
			same = false;
			text += " " + name + " game " + oracle::Hex(Cell(table, index, name)) + " ours " + oracle::Hex(ours);
		}
	}
	return same;
}

} // namespace

TEST(OracleHeldFrameHeading, TheFrameOfAHeldThingFromTheHeading)
{
	const auto table = oracle::Table::Load("held_frame.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 up {Cell(*table, index, "ux"), Cell(*table, index, "uy"), Cell(*table, index, "uz")};
		// The game made these rows with a hand of size 1 that is not mirrored
		const auto frame = magic::hand_hold::HeldFrame(Cell(*table, index, "heading"), up, 1.0f, false);
		std::string text;
		mismatches.Check(SameFrame(*table, index, frame, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleHeldFrameHeading, TheFrameOfAHeldThingTimesTheHandsSize)
{
	const auto table = oracle::Table::Load("held_frame_scaled.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 up {Cell(*table, index, "ux"), Cell(*table, index, "uy"), Cell(*table, index, "uz")};
		const bool mirrored = CellBits(*table, index, "mirror") != 0;
		const auto frame =
		    magic::hand_hold::HeldFrame(Cell(*table, index, "heading"), up, Cell(*table, index, "drawn"), mirrored);
		std::string text;
		mismatches.Check(SameFrame(*table, index, frame, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
