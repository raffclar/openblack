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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/HandAnimation.h"
#include "Common/X87.h"
#include "Hand/HandFeel.h"
#include "Magic/HandMotion.h"
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

TEST(OracleHandFeelRest2, TheHandsSizeAndItsDrawnScale)
{
	const auto table = oracle::Table::Load("hand_drawn_scale.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const float creature = Cell(*table, index, "creature");
		// On a creature, the hand's size is multiplied by the creature's size to the power of a half
		float size = HandAnimation::SizeAtDistance(Cell(*table, index, "distance"));
		if (creature != 0.0f)
		{
			size = x87::Pow(creature, 0.5) * size;
		}
		const float drawn = HandAnimation::DrawnScale(Cell(*table, index, "rest"), size);
		const bool same = std::bit_cast<uint32_t>(size) == CellBits(*table, index, "size") &&
		                  std::bit_cast<uint32_t>(drawn) == CellBits(*table, index, "drawn");
		mismatches.Check(same, index, " size " + oracle::Hex(size) + " drawn " + oracle::Hex(drawn));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleHandFeelRest2, TheWayAPourTipsTheHandsUp)
{
	const auto table = oracle::Table::Load("pour_up.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const glm::vec3 camera {Cell(*table, index, "cx"), Cell(*table, index, "cy"), Cell(*table, index, "cz")};
		const glm::vec3 hand {Cell(*table, index, "hx"), Cell(*table, index, "hy"), Cell(*table, index, "hz")};
		const auto up = hand_feel::PourUp(camera, hand, Cell(*table, index, "tilt"));
		const bool same = std::bit_cast<uint32_t>(up.x) == CellBits(*table, index, "ux") &&
		                  std::bit_cast<uint32_t>(up.y) == CellBits(*table, index, "uy") &&
		                  std::bit_cast<uint32_t>(up.z) == CellBits(*table, index, "uz");
		mismatches.Check(same, index, " " + oracle::Hex(up.x) + " " + oracle::Hex(up.y) + " " + oracle::Hex(up.z));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleHandFeelRest2, ThePoursTiltBetweenGameTurns)
{
	const auto table = oracle::Table::Load("pour_tilt.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		magic::PourState pour;
		pour.previous.tilt = Cell(*table, index, "last");
		pour.current.tilt = Cell(*table, index, "current");
		const float tilt = magic::PourPoseAt(pour, Cell(*table, index, "fraction")).tilt;
		mismatches.Check(std::bit_cast<uint32_t>(tilt) == CellBits(*table, index, "tilt"), index, " " + oracle::Hex(tilt));
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
