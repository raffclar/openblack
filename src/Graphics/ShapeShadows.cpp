/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShapeShadows.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <utility>

#include "Common/X87.h"

namespace openblack::graphics::shape_shadows
{

namespace
{

constexpr auto k_Sun = tree_shadows::k_Sun;
/// Samples per unit along z and x for each texel along a side: a 40th and an 80th, as the game keeps them in floats
constexpr float k_ColumnsPerTexelUnit = 0.025f;
constexpr float k_RowsPerTexelUnit = 0.0125f;
/// The edges' columns are followed in 16.16 fixed point
constexpr float k_FixedOne = 65536.0f;

/// The columns where each row's covered samples start and end. Every row a triangle's edges go through is passed by one
/// edge going down the rows and one going up, so both are written for each row filled, and needn't be cleared first.
struct Edges
{
	std::array<int, k_Rows> starts;
	std::array<int, k_Rows> ends;
	/// The sample rows of the block: two for each texel along x
	int rows {k_Rows};
	/// The rows any edge went through: from the first to before the last
	int first {k_Rows};
	int last {0};
};

/// Follows an edge from one corner to another through the rows between them, from the first row up to, not including,
/// the last. An edge going down the rows bounds them where they start, one going up where they end.
void Edge(Edges& edges, int fromRow, float fromColumn, int toRow, float toColumn)
{
	if (fromRow == toRow)
	{
		return;
	}
	auto* side = &edges.ends;
	if (toRow < fromRow)
	{
		std::swap(fromRow, toRow);
		std::swap(fromColumn, toColumn);
		side = &edges.starts;
	}
	if (fromRow >= edges.rows || toRow <= 0)
	{
		return;
	}
	const float slope = (toColumn - fromColumn) / static_cast<float>(toRow - fromRow);
	toRow = std::min(toRow, edges.rows);
	if (fromRow < 0)
	{
		fromColumn -= static_cast<float>(fromRow) * slope;
		fromRow = 0;
	}
	edges.first = std::min(edges.first, fromRow);
	edges.last = std::max(edges.last, toRow);
	const auto step = static_cast<uint32_t>(x87::ToInteger(slope * k_FixedOne));
	auto column = static_cast<uint32_t>(x87::ToInteger(fromColumn * k_FixedOne));
	for (int row = fromRow; row < toRow; ++row)
	{
		(*side)[static_cast<size_t>(row)] = static_cast<int32_t>(column) >> 16;
		column += step;
	}
}

/// Covers the samples of a row from one column up to, not including, another
void Cover(tree_shadows::Coverage& coverage, int row, int from, int to, int side)
{
	const auto texelRow = static_cast<size_t>(row >> 1) * static_cast<size_t>(side);
	const int rowBits = 4 * (row & 1);
	for (int column = from; column < to; ++column)
	{
		coverage[texelRow + static_cast<size_t>(column >> 2)] |= static_cast<uint8_t>(1u << ((column & 3) + rowBits));
	}
}

} // namespace

glm::vec2 Cast(glm::vec3 offset, glm::vec2 origin)
{
	const float height = std::max(offset.y, 0.0f);
	const float along = -k_Sun.y / (height - k_Sun.y);
	const float x = offset.x + origin.x;
	const float z = offset.z + origin.y;
	return {(x - k_Sun.x) * along + k_Sun.x, (z - k_Sun.z) * along + k_Sun.z};
}

Sample SampleOf(glm::vec2 cast, glm::vec2 block, int side)
{
	const auto texels = static_cast<float>(side);
	return {.column = (cast.y - block.y) * (texels * k_ColumnsPerTexelUnit),
	        .row = (cast.x - block.x) * (texels * k_RowsPerTexelUnit)};
}

void Fill(tree_shadows::Coverage& coverage, const std::array<Sample, 3>& corners, bool bothSides, int side)
{
	const int columns = 4 * side;
	std::array<int, 3> rows {};
	std::ranges::transform(corners, rows.begin(), [](const Sample& corner) { return x87::ToInteger(corner.row); });
	const auto turn = [&rows, &corners](size_t a, size_t b, size_t c) {
		return static_cast<float>(rows[a] - rows[c]) * (corners[b].column - corners[c].column);
	};

	Edges edges;
	edges.rows = 2 * side;
	edges.first = edges.rows;
	const auto follow = [&edges, &rows, &corners](size_t from, size_t to) {
		Edge(edges, rows[from], corners[from].column, rows[to], corners[to].column);
	};
	if (bothSides)
	{
		// Turned around when it faces away
		if (turn(0, 2, 1) >= turn(2, 0, 1))
		{
			follow(0, 1);
			follow(1, 2);
			follow(2, 0);
		}
		else
		{
			follow(0, 2);
			follow(2, 1);
			follow(1, 0);
		}
	}
	else
	{
		if (!(turn(0, 1, 2) >= turn(1, 0, 2)))
		{
			return;
		}
		follow(0, 2);
		follow(2, 1);
		follow(1, 0);
	}

	for (int row = edges.first; row < edges.last; ++row)
	{
		const int from = std::max(edges.starts[static_cast<size_t>(row)], 0);
		const int to = std::min(edges.ends[static_cast<size_t>(row)], columns);
		if (from < to && from < columns && to > 0)
		{
			Cover(coverage, row, from, to, side);
		}
	}
}

} // namespace openblack::graphics::shape_shadows
