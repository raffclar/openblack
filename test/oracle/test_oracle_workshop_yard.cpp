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
#include <format>
#include <string>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/WorkshopRules.h"
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

	/// A matrix of the 3D engine from twelve columns: three rows, then the place
	[[nodiscard]] glm::mat4 Matrix(const std::string& prefix) const
	{
		glm::mat4 matrix {1.0f};
		for (glm::length_t part = 0; part < 4; ++part)
		{
			for (glm::length_t axis = 0; axis < 3; ++axis)
			{
				matrix[part][axis] = F(prefix + std::to_string((3 * part) + axis));
			}
		}
		return matrix;
	}
};

/// The values of our place that differ in their bits from the game's, as text
std::string Mismatch(const Row& row, const ecs::workshop::SpecialPlace& ours)
{
	std::string text;
	const auto check = [&text, &row](const std::string& name, uint32_t bits) {
		if (bits != row.U(name))
		{
			text += std::format(" {}: ours {:08x} game {:08x}", name, bits, row.U(name));
		}
	};
	check("cx", std::bit_cast<uint32_t>(ours.coords.x));
	check("cz", std::bit_cast<uint32_t>(ours.coords.z));
	check("calt", std::bit_cast<uint32_t>(ours.coords.altitude));
	check("ax", std::bit_cast<uint32_t>(ours.angles.x));
	check("ay", std::bit_cast<uint32_t>(ours.angles.y));
	check("az", std::bit_cast<uint32_t>(ours.angles.z));
	return text;
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleWorkshopYard, ThePlacesOfTheYardAreTheSpecialPointsSixToEight)
{
	const auto table = oracle::Table::Load("workshop_yard.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t mismatches = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		// The data puts the game's chosen special point of the place at that point of the model
		const auto ours = ecs::workshop::SpecialPlaceOf(row.Matrix("p"), row.Matrix("b"));
		const auto text = Mismatch(row, ours);
		if (!text.empty() && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << index << " place " << row.U("place") << text;
		}
	}
	EXPECT_EQ(mismatches, 0u);
	EXPECT_EQ(ecs::workshop::k_PlacePoints, (std::array<size_t, 3> {6, 7, 8}));
}
