/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "OracleData.h"
#include "Visual/LandDetail.h"

// The game's own functions give the data (tools/oracle/cases/land_detail_cases.py). Each test compares our result
// with every row, bit for bit, and shows the first rows that differ.

using namespace openblack::graphics::land_detail;
namespace oracle = openblack::oracle;

namespace
{
constexpr size_t k_FailuresShown = 20;

/// Counts the rows that differ and shows the first of them
class Differences
{
public:
	void Add(const std::string& what)
	{
		++_count;
		if (_count <= k_FailuresShown)
		{
			ADD_FAILURE() << what;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The columns of a table, by name, in the order given
[[nodiscard]] std::optional<std::vector<size_t>> ColumnsOf(const oracle::Table& table, std::initializer_list<const char*> names)
{
	std::vector<size_t> columns;
	for (const auto* name : names)
	{
		const auto index = table.Column(name);
		if (!index.has_value())
		{
			return std::nullopt;
		}
		columns.push_back(*index);
	}
	return columns;
}

/// The bytes of a row's corners, four a word from the column r0, the first in the low byte
[[nodiscard]] std::array<uint8_t, k_CornerCount> Corners(const oracle::Table& table, size_t row, size_t first)
{
	std::array<uint8_t, k_CornerCount> corners {};
	for (size_t i = 0; i < k_CornerCount; ++i)
	{
		corners.at(i) = static_cast<uint8_t>(table.Bits(row, first + i / 4) >> (8 * (i % 4)));
	}
	return corners;
}

/// The corner altitudes of a morph case, as the case script makes them from its seed
[[nodiscard]] std::array<uint8_t, k_CornerCount> AltitudesOf(uint32_t seed)
{
	std::array<uint8_t, k_CornerCount> altitudes {};
	for (uint32_t corner = 0; corner < k_CornerCount; ++corner)
	{
		const uint32_t value = seed * 2654435761u + corner * 40503u;
		altitudes.at(corner) = static_cast<uint8_t>(value >> 13);
	}
	return altitudes;
}

[[nodiscard]] std::string Show(const std::array<uint8_t, k_CornerCount>& ours, const std::array<uint8_t, k_CornerCount>& game)
{
	std::string text;
	for (size_t i = 0; i < k_CornerCount; ++i)
	{
		if (ours.at(i) != game.at(i))
		{
			text += " corner " + std::to_string(i) + ": " + std::to_string(ours.at(i)) + " vs " + std::to_string(game.at(i));
		}
	}
	return text;
}

} // namespace

TEST(OracleLandDetail, TheLinesAcrossTheGroundAreTheGames)
{
	const auto table = oracle::Table::Load("land_detail_lines.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = ColumnsOf(*table, {"kind", "cx", "cy", "cz", "fx", "fy", "fz", "depth", "dx", "dz", "px", "pz"});
	ASSERT_TRUE(columns.has_value());
	const auto& c = *columns;
	// Before each call the game's record held this line
	const GroundLine before {.along = {0.25f, -0.5f}, .point = {7.0f, 9.0f}};
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](size_t column) { return table->Float(row, c.at(column)); };
		const glm::vec3 camera {at(1), at(2), at(3)};
		const glm::vec3 forward {at(4), at(5), at(6)};
		const bool texture = table->Bits(row, c.at(0)) == 1;
		const float ground = texture ? std::min(camera.y, k_TextureGroundCap) : 0.0f;
		const auto line = GroundLineAcross(camera, forward, at(7), ground, before);
		const std::array<uint32_t, 4> ours = {Bits(line.along.x), Bits(line.along.y), Bits(line.point.x), Bits(line.point.y)};
		const std::array<uint32_t, 4> game = {table->Bits(row, c.at(8)), table->Bits(row, c.at(9)), table->Bits(row, c.at(10)),
		                                      table->Bits(row, c.at(11))};
		if (ours != game)
		{
			differences.Add("row " + std::to_string(row));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
	EXPECT_GT(table->Rows(), 1000u);
}

TEST(OracleLandDetail, TheMorphOfABlocksCornersIsTheGames)
{
	const auto table = oracle::Table::Load("land_morph.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = ColumnsOf(*table, {"kind", "bx", "bz", "dx", "dz", "px", "pz", "width", "seed", "r0"});
	ASSERT_TRUE(columns.has_value());
	const auto& c = *columns;
	Differences differences;
	size_t blended = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](size_t column) { return table->Float(row, c.at(column)); };
		const auto morph = table->Bits(row, c.at(0)) == 1 ? Morph::ToHalf : Morph::ToQuarter;
		const GroundLine line {.along = {at(3), at(4)}, .point = {at(5), at(6)}};
		const auto altitudes = AltitudesOf(table->Bits(row, c.at(8)));
		const auto ours = MorphedCorners(altitudes, morph, line, at(7), {at(1), at(2)});
		const auto game = Corners(*table, row, c.at(9));
		if (ours != game)
		{
			differences.Add("row " + std::to_string(row) + Show(ours, game));
		}
		for (size_t corner = 0; corner < k_CornerCount; ++corner)
		{
			blended += game.at(corner) != altitudes.at(corner) ? 1 : 0;
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
	// The cases lie across the bands, so many corners move
	EXPECT_GT(blended, 1000u);
}

TEST(OracleLandDetail, TheDrawnCornerOfTheObjectDrawListIsTheMorphsCorner)
{
	const auto table = oracle::Table::Load("land_morph.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = ColumnsOf(*table, {"kind", "bx", "bz", "dx", "dz", "px", "pz", "width", "seed", "r0"});
	ASSERT_TRUE(columns.has_value());
	const auto& c = *columns;
	Differences differences;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](size_t column) { return table->Float(row, c.at(column)); };
		const bool toHalf = table->Bits(row, c.at(0)) == 1;
		FrameDetail frame;
		frame.lines.meshBand = at(7) * 0.5f;
		const GroundLine line {.along = {at(3), at(4)}, .point = {at(5), at(6)}};
		(toHalf ? frame.meshNearGround : frame.meshFarGround) = line;
		const auto altitudes = AltitudesOf(table->Bits(row, c.at(8)));
		const auto game = Corners(*table, row, c.at(9));
		for (size_t x = 0; x < k_CornersPerSide; ++x)
		{
			for (size_t z = 0; z < k_CornersPerSide; ++z)
			{
				const auto drawn =
				    DrawnCornerAltitude(altitudes, x, z, toHalf ? Morph::ToHalf : Morph::ToQuarter, frame, {at(1), at(2)});
				if (drawn != game.at(x * k_CornersPerSide + z))
				{
					differences.Add("row " + std::to_string(row) + " corner " + std::to_string(x * k_CornersPerSide + z));
				}
			}
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleLandDetail, TheCrossFadeOfABlocksCoarseTextureIsTheGames)
{
	const auto table = oracle::Table::Load("land_cross_fade.txt");
	ASSERT_TRUE(table.has_value());
	const auto columns = ColumnsOf(*table, {"bx", "bz", "dx", "dz", "px", "pz", "width", "band", "r0"});
	ASSERT_TRUE(columns.has_value());
	const auto& c = *columns;
	Differences differences;
	size_t partial = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&](size_t column) { return table->Float(row, c.at(column)); };
		const GroundLine line {.along = {at(2), at(3)}, .point = {at(4), at(5)}};
		const auto ours = CrossFadeCorners(line, at(6), at(7), {at(0), at(1)});
		const auto game = Corners(*table, row, c.at(8));
		if (ours != game)
		{
			differences.Add("row " + std::to_string(row) + Show(ours, game));
		}
		for (const auto alpha : game)
		{
			partial += alpha != 0 && alpha != 255 ? 1 : 0;
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
	EXPECT_GT(partial, 1000u);
}
