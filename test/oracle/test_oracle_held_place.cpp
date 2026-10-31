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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
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
};

/// The data file of the game's places of held things
oracle::Table Places()
{
	auto table = oracle::Table::Load("held_place.txt");
	EXPECT_TRUE(table.has_value());
	return table.value_or(oracle::Table {});
}

/// The text of one value that differs from the game, or an empty text
std::string Mismatch(const std::string& name, uint32_t ours, uint32_t game)
{
	return ours == game ? std::string {} : std::format(" {}: ours {:08x} game {:08x}", name, ours, game);
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleHeldPlace, ThePlaceMatchesTheGame)
{
	const auto table = Places();
	ASSERT_GT(table.Rows(), 0u);
	for (size_t index = 0; index < table.Rows(); ++index)
	{
		const Row row {table, index};
		map_coords::MapCoords asked {};
		const auto ground = row.F("ground");
		const auto place =
		    map_coords::HeldPlace({row.F("x"), row.F("y"), row.F("z")}, [&asked, ground](const map_coords::MapCoords& coords) {
			    asked = coords;
			    return ground;
		    });
		const auto mismatch = Mismatch("place_x", static_cast<uint32_t>(place.x), row.U("place_x")) +
		                      Mismatch("place_z", static_cast<uint32_t>(place.z), row.U("place_z")) +
		                      Mismatch("altitude", std::bit_cast<uint32_t>(place.altitude), row.U("altitude")) +
		                      Mismatch("ground_x", static_cast<uint32_t>(asked.x), row.U("ground_x")) +
		                      Mismatch("ground_z", static_cast<uint32_t>(asked.z), row.U("ground_z"));
		EXPECT_TRUE(mismatch.empty()) << "row " << index << mismatch;
	}
}
