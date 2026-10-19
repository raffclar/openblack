/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <format>
#include <string>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Angle.h"
#include "ECS/PlacedTurn.h"
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

	/// The matrix in the columns with this prefix: each row of the game's matrix is one column
	[[nodiscard]] glm::mat3 Matrix(const std::string& prefix) const
	{
		glm::mat3 matrix;
		for (glm::length_t column = 0; column < 3; ++column)
		{
			for (glm::length_t value = 0; value < 3; ++value)
			{
				matrix[column][value] = F(prefix + std::to_string((3 * column) + value));
			}
		}
		return matrix;
	}
};

/// Whether our value is the game's value, bit for bit. A value that is not a number only has to be not a number.
bool Same(float ours, uint32_t game)
{
	const float gameValue = std::bit_cast<float>(game);
	if (std::isnan(gameValue))
	{
		return std::isnan(ours);
	}
	return std::bit_cast<uint32_t>(ours) == game;
}

/// The values of our matrix that differ from the game's columns with this prefix, as text
std::string MatrixMismatch(const Row& row, const std::string& prefix, const glm::mat3& ours)
{
	std::string text;
	for (glm::length_t column = 0; column < 3; ++column)
	{
		for (glm::length_t value = 0; value < 3; ++value)
		{
			const auto name = prefix + std::to_string((3 * column) + value);
			if (!Same(ours[column][value], row.U(name)))
			{
				text += std::format(" {}: ours {:08x} game {:08x}", name, std::bit_cast<uint32_t>(ours[column][value]),
				                    row.U(name));
			}
		}
	}
	return text;
}

/// The values of our angles that differ from the game's angles, as text
std::string AnglesMismatch(const Row& row, const glm::vec3& ours, bool canLean)
{
	std::string text;
	const auto check = [&](const char* name, float value, uint32_t game) {
		if (!Same(value, game))
		{
			text += std::format(" {}: ours {:08x} game {:08x}", name, std::bit_cast<uint32_t>(value), game);
		}
	};
	check("ax", ours.x, canLean ? row.U("ax") : 0u);
	check("ay", ours.y, row.U("ay"));
	check("az", ours.z, canLean ? row.U("az") : 0u);
	return text;
}

/// The data file of the bodies and the angles that the game takes from them
oracle::Table Bodies()
{
	auto table = oracle::Table::Load("physics_angles.txt");
	EXPECT_TRUE(table.has_value());
	return table.value_or(oracle::Table {});
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePhysicsAngles, AThingThatLeansTakesTheGameAnglesAndTurn)
{
	const auto table = Bodies();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto taken = ecs::placed_turn::TurnFromBody(row.Matrix("b"), true);
		const auto mismatch = AnglesMismatch(row, taken.angles, true) + MatrixMismatch(row, "t", taken.turn);
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePhysicsAngles, AThingThatDoesNotLeanTakesTheGameHeadingAndTurn)
{
	const auto table = Bodies();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto taken = ecs::placed_turn::TurnFromBody(row.Matrix("b"), false);
		const auto mismatch = AnglesMismatch(row, taken.angles, false) + MatrixMismatch(row, "u", taken.turn);
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePhysicsAngles, TheUprightTurnMatchesTheGame)
{
	const auto table = Bodies();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto mismatch = MatrixMismatch(row, "u", x87::GameUprightTurn(row.F("ay")));
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}
