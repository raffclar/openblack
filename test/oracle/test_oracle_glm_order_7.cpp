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
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Matrix.h"
#include "Magic/HandHoldPose.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
	[[nodiscard]] glm::vec3 Vec(const std::string& x, const std::string& y, const std::string& z) const
	{
		return {F(x), F(y), F(z)};
	}
};

/// Adds the names of the parts that differ from the game's bits to the text
bool SameBits(const Row& row, const std::vector<std::string>& names, const std::vector<float>& ours, std::string& text)
{
	bool same = true;
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (std::bit_cast<uint32_t>(ours[i]) != row.U(names[i]))
		{
			same = false;
			text += " " + names[i] + " game " + oracle::Hex(row.F(names[i])) + " ours " + oracle::Hex(ours[i]);
		}
	}
	return same;
}

std::vector<std::string> Names(const std::string& prefix, int count)
{
	std::vector<std::string> names;
	for (int i = 0; i < count; ++i)
	{
		names.push_back(prefix + std::to_string(i));
	}
	return names;
}

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

} // namespace

TEST(OracleGlmOrder7, ATurnAboutAUnitAxis)
{
	const auto table = oracle::Table::Load("turn_about.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto turn = x87::TurnAbout(row.Vec("x", "y", "z"), row.F("angle"));
		std::vector<float> values;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				values.push_back(turn[i][k]);
			}
		}
		std::string text;
		mismatches.Check(SameBits(row, Names("m", 12), values, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder7, TheUpOfAHeldThing)
{
	const auto table = oracle::Table::Load("held_up.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const auto up =
		    magic::hand_hold::HeldUp(row.Vec("cx", "cy", "cz"), row.Vec("hx", "hy", "hz"), row.F("roll"), row.F("pitch"));
		std::string text;
		mismatches.Check(SameBits(row, {"ux", "uy", "uz"}, {up.x, up.y, up.z}, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}

TEST(OracleGlmOrder7, TheFrameOfAHeldThing)
{
	const auto table = oracle::Table::Load("held_basis.txt");
	ASSERT_TRUE(table.has_value());
	Mismatches mismatches;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		// The level turn whose facing is the game's: its first column is (minus the negative cosine, 0, the sine)
		glm::mat3 levelTurn {1.0f};
		levelTurn[0] = {-row.F("cosine"), 0.0f, row.F("sine")};
		const auto basis = magic::hand_hold::HeldBasis(levelTurn, row.Vec("ux", "uy", "uz"));
		std::vector<float> values;
		for (int i = 0; i < 3; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				values.push_back(basis[i][k]);
			}
		}
		std::string text;
		mismatches.Check(SameBits(row, Names("r", 9), values, text), index, text);
	}
	EXPECT_EQ(mismatches.Count(), 0u);
}
