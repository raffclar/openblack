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
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureEyes.h"
#include "Creature/CreatureRig.h"
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
	[[nodiscard]] glm::vec3 Indexed(const std::string& prefix, size_t first) const
	{
		return {F(prefix + std::to_string(first)), F(prefix + std::to_string(first + 1)),
		        F(prefix + std::to_string(first + 2))};
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

} // namespace

TEST(OracleGlmOrder5, APointOnASkinnedMesh)
{
	const auto table = oracle::Table::Load("skin_point.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		std::array<glm::vec3, 3> placed {};
		for (size_t b = 0; b < 3; ++b)
		{
			// Each bone: three rows, then the place. The columns of our matrix are the game's rows.
			const size_t base = 12 * b;
			const glm::mat4 bone {glm::vec4(row.Indexed("bone", base), 0.0f), glm::vec4(row.Indexed("bone", base + 3), 0.0f),
			                      glm::vec4(row.Indexed("bone", base + 6), 0.0f),
			                      glm::vec4(row.Indexed("bone", base + 9), 1.0f)};
			placed.at(b) = creature::PlaceByBone(bone, row.Indexed("t", 3 * b), creature::k_SkinPointOrder.at(b));
		}
		const auto point = creature_eyes::PointOnTriangle(placed[0], placed[1], placed[2], row.F("s"), row.F("t"));
		std::string text;
		if (!SameBits(row, {"px", "py", "pz", "nx", "ny", "nz"},
		              {point.position.x, point.position.y, point.position.z, point.normal.x, point.normal.y, point.normal.z},
		              text) &&
		    ++different <= 20)
		{
			ADD_FAILURE() << "row " << index << ":" << text;
		}
	}
	EXPECT_EQ(different, 0U);
}
