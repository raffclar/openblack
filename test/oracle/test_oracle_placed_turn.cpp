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
#include <format>
#include <string>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Angle.h"
#include "OracleData.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

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
};

/// The rows of the game's matrix that differ in their bits from the columns of our matrix, as text
std::string MatrixMismatch(const Row& row, const std::string& prefix, const glm::mat3& ours)
{
	std::string text;
	for (glm::length_t column = 0; column < 3; ++column)
	{
		for (glm::length_t value = 0; value < 3; ++value)
		{
			const auto name = prefix + std::to_string((3 * column) + value);
			const auto bits = std::bit_cast<uint32_t>(ours[column][value]);
			if (bits != row.U(name))
			{
				text += std::format(" {}: ours {:08x} game {:08x}", name, bits, row.U(name));
			}
		}
	}
	return text;
}

/// The data file of the game's turns
oracle::Table Turns()
{
	auto table = oracle::Table::Load("placed_turn.txt");
	EXPECT_TRUE(table.has_value());
	return table.value_or(oracle::Table {});
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePlacedTurn, TheTurnMatchesTheGame)
{
	const auto table = Turns();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto turn = x87::GameTurnMatrix(row.F("x"), row.F("y"), row.F("z"));
		const auto mismatch = MatrixMismatch(row, "m", turn);
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePlacedTurn, TheScaledTurnMatchesTheGame)
{
	const auto table = Turns();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto turn = x87::GameTurnMatrixScaled(row.F("x"), row.F("y"), row.F("z"), row.F("scale"));
		const auto mismatch = MatrixMismatch(row, "s", turn);
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePlacedTurn, TheAnglesOfATurnMatchTheGame)
{
	const auto table = Turns();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto turn = x87::GameTurnMatrix(row.F("x"), row.F("y"), row.F("z"));
		const auto angles = x87::GameTurnMatrixAngles(turn);
		EXPECT_EQ(std::bit_cast<uint32_t>(angles.x), row.U("ax")) << "row " << index;
		EXPECT_EQ(std::bit_cast<uint32_t>(angles.y), row.U("ay")) << "row " << index;
		EXPECT_EQ(std::bit_cast<uint32_t>(angles.z), row.U("az")) << "row " << index;
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OraclePlacedTurn, AScriptPlacesAThingWithTheGameTurn)
{
	// The scripts' angles of a placed thing go through the same turn
	const auto table = Turns();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		const auto turn = script::property_rules::PlacedRotation({.x = row.F("x"), .y = row.F("y"), .z = row.F("z")});
		const auto mismatch = MatrixMismatch(row, "m", turn);
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
		const auto angles = script::property_rules::PlacedAngles(turn);
		EXPECT_EQ(std::bit_cast<uint32_t>(angles.y), row.U("ay")) << "row " << index;
	}
}
