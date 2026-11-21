/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootprintPaint.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <utility>
#include <vector>

#include "Common/IntegerSteps.h"
#include "Common/NearestWhole.h"

namespace openblack::footprint_paint
{

uint16_t Paint(uint16_t land, uint16_t footprint)
{
	const auto alpha = static_cast<uint8_t>(footprint >> 12);
	const auto channel = [&](int shift) {
		const auto mixed =
		    MixChannel(static_cast<uint8_t>((land >> shift) & 0xF), static_cast<uint8_t>((footprint >> shift) & 0xF), alpha);
		return static_cast<uint16_t>(mixed << shift);
	};
	return static_cast<uint16_t>((land & 0xF000) | channel(8) | channel(4) | channel(0));
}

uint16_t LowerAlpha(uint16_t land, uint16_t channel)
{
	return static_cast<uint16_t>((land & 0x0FFF) | std::min(land & 0xF000, channel & 0xF000));
}

namespace
{
/// A corner of a triangle on the block's texture: its row (along x) and column (along z) in texels, and its texel on
/// the picture
struct Corner
{
	float row;
	float column;
	float u;
	float v;
};

/// The span of one row of the block's texture that the edges of a triangle reach: its lowest and highest columns, and
/// the picture's coordinates at each of them
struct Span
{
	bool used {false};
	int low {0};
	int high {0};
	float uLow {0.0f};
	float uHigh {0.0f};
	float vLow {0.0f};
	float vHigh {0.0f};
};

/// The spans of the rows of the block's texture, and the first and the last row that an edge reached
struct Spans
{
	/// The texels along a side of the block's texture
	int side {k_BlockTexels};
	std::array<Span, k_BlockTexels> rows {};
	int first {k_BlockTexels};
	int last {-1};
};

/// A value to the nearest whole number, a half to the even one, as the game's processor rounds it
[[nodiscard]] int Nearest(float value)
{
	return NearestWhole(value);
}

/// Widens the span of a row to a column an edge reached. A column below the span becomes its low end, else a column
/// above it becomes its high end. The rows outside the block are not kept.
void Reach(Spans& spans, int row, int column, float u, float v)
{
	if (row < 0 || row >= spans.side)
	{
		return;
	}
	auto& span = spans.rows.at(static_cast<size_t>(row));
	if (!span.used)
	{
		span = {.used = true, .low = column, .high = column, .uLow = u, .uHigh = u, .vLow = v, .vHigh = v};
		spans.first = std::min(spans.first, row);
		spans.last = std::max(spans.last, row);
		return;
	}
	if (column < span.low)
	{
		span.low = column;
		span.uLow = u;
		span.vLow = v;
	}
	else if (column > span.high)
	{
		span.high = column;
		span.uHigh = u;
		span.vHigh = v;
	}
}

/// Walks an edge from the corner with the lower row to the other, a texel at a time along its longer axis. The other
/// axis steps when the error that it gathers is more than the longer axis. The picture's coordinates step evenly.
void WalkEdge(Spans& spans, Corner start, Corner end)
{
	if (start.row > end.row)
	{
		std::swap(start, end);
	}
	int row = Nearest(start.row);
	int column = Nearest(start.column);
	const int rows = Nearest(end.row) - row;
	const int columnsSigned = Nearest(end.column) - column;
	const int rowStep = rows < 0 ? -1 : 1;
	const int columnStep = columnsSigned < 0 ? -1 : 1;
	const int rowsAcross = std::abs(rows);
	const int columnsAcross = std::abs(columnsSigned);
	float u = start.u;
	float v = start.v;
	int error = 0;
	Reach(spans, row, column, u, v);
	if (rowsAcross < columnsAcross)
	{
		const float du = (end.u - start.u) / static_cast<float>(columnsAcross);
		const float dv = (end.v - start.v) / static_cast<float>(columnsAcross);
		for (int step = 0; step < columnsAcross; ++step)
		{
			v = dv + v;
			column += columnStep;
			error += rowsAcross;
			u = du + u;
			if (error > columnsAcross)
			{
				row += rowStep;
				error -= columnsAcross;
			}
			Reach(spans, row, column, u, v);
		}
		return;
	}
	if (rowsAcross == 0)
	{
		return;
	}
	const float du = (end.u - start.u) / static_cast<float>(rowsAcross);
	const float dv = (end.v - start.v) / static_cast<float>(rowsAcross);
	for (int step = 0; step < rowsAcross; ++step)
	{
		u = du + u;
		row += rowStep;
		error += columnsAcross;
		v = dv + v;
		if (error > rowsAcross)
		{
			column += columnStep;
			error -= rowsAcross;
		}
		Reach(spans, row, column, u, v);
	}
}

/// Fills each row that the edges reached, from its low column to its high column, both ends too. The picture's
/// coordinates step evenly from those at the low end to those at the high end. Only the texels in the region are
/// painted, and the region is in the block. Each row is empty again after. Only the rows from the first to the last
/// that an edge reached can be used.
void FillSpans(Spans& spans, std::span<uint16_t> block, const Picture& picture, Mix mix, const Region& region)
{
	for (int row = spans.first; row <= spans.last; ++row)
	{
		auto& span = spans.rows.at(static_cast<size_t>(row));
		if (!span.used)
		{
			continue;
		}
		if (row < region.firstRow || row > region.lastRow)
		{
			span.used = false;
			continue;
		}
		float du = 0.0f;
		float dv = 0.0f;
		const int across = span.high - span.low;
		if (across != 0)
		{
			const float inverse = 1.0f / static_cast<float>(across);
			du = (span.uHigh - span.uLow) * inverse;
			dv = (span.vHigh - span.vLow) * inverse;
		}
		float u = span.uLow;
		float v = span.vLow;
		// The coordinates step from the low end also where the region starts after it, so that each texel gets the
		// same coordinates as in a paint of the whole block
		const int last = std::min(span.high, region.lastColumn);
		for (int column = span.low; column <= last; ++column)
		{
			if (column >= region.firstColumn)
			{
				const int texelU = std::clamp(Nearest(u), 0, picture.width - 1);
				const int texelV = std::clamp(Nearest(v), 0, picture.height - 1);
				const auto texel = picture.texels[ToIndex((picture.width * texelV) + texelU)];
				auto& land = block[ToIndex((row * spans.side) + column)];
				land = mix == Mix::Colour ? Paint(land, texel) : LowerAlpha(land, texel);
			}
			u = u + du;
			v = v + dv;
		}
		span.used = false;
	}
	spans.first = spans.side;
	spans.last = -1;
}
} // namespace

Region WholeBlock(int side)
{
	return {.firstRow = 0, .lastRow = side - 1, .firstColumn = 0, .lastColumn = side - 1};
}

Region RegionOf(const std::pair<glm::vec2, glm::vec2>& bounds, glm::ivec2 blockIndex, int side)
{
	const float rowStart = static_cast<float>(blockIndex.x) * k_BlockUnits;
	const float columnStart = static_cast<float>(blockIndex.y) * k_BlockUnits;
	const auto texels = static_cast<float>(side);
	// A corner goes to the nearest texel, thus a unit more on each side holds every texel that the footprint paints
	const auto texelOf = [texels](float ground, float start) { return ((ground - start) * texels) * k_UnitPerBlock; };
	const int firstRow = static_cast<int>(std::floor(texelOf(bounds.first.x - 1.0f, rowStart))) - 1;
	const int lastRow = static_cast<int>(std::ceil(texelOf(bounds.second.x + 1.0f, rowStart))) + 1;
	const int firstColumn = static_cast<int>(std::floor(texelOf(bounds.first.y - 1.0f, columnStart))) - 1;
	const int lastColumn = static_cast<int>(std::ceil(texelOf(bounds.second.y + 1.0f, columnStart))) + 1;
	const Region region {.firstRow = std::max(firstRow, 0),
	                     .lastRow = std::min(lastRow, side - 1),
	                     .firstColumn = std::max(firstColumn, 0),
	                     .lastColumn = std::min(lastColumn, side - 1)};
	return region.Empty() ? Region {} : region;
}

Region Join(const Region& a, const Region& b)
{
	if (a.Empty())
	{
		return b;
	}
	if (b.Empty())
	{
		return a;
	}
	return {.firstRow = std::min(a.firstRow, b.firstRow),
	        .lastRow = std::max(a.lastRow, b.lastRow),
	        .firstColumn = std::min(a.firstColumn, b.firstColumn),
	        .lastColumn = std::max(a.lastColumn, b.lastColumn)};
}

bool Overlap(const Region& a, const Region& b)
{
	return !a.Empty() && !b.Empty() && a.firstRow <= b.lastRow && b.firstRow <= a.lastRow && a.firstColumn <= b.lastColumn &&
	       b.firstColumn <= a.lastColumn;
}

void PaintFootprint(std::span<uint16_t> block, glm::ivec2 blockIndex, const Picture& picture,
                    std::span<const Triangle> triangles, const Placement& placement, Mix mix, int side)
{
	PaintFootprint(block, blockIndex, picture, triangles, placement, mix, side, WholeBlock(side));
}

void PaintFootprint(std::span<uint16_t> block, glm::ivec2 blockIndex, const Picture& picture,
                    std::span<const Triangle> triangles, const Placement& placement, Mix mix, int side, const Region& region)
{
	if (side <= 0 || side > k_BlockTexels || block.size() < static_cast<size_t>(side) * static_cast<size_t>(side) ||
	    picture.width <= 0 || picture.height <= 0 ||
	    picture.texels.size() < static_cast<size_t>(picture.width) * static_cast<size_t>(picture.height))
	{
		return;
	}
	// The region is kept in the block
	const Region inBlock {.firstRow = std::max(region.firstRow, 0),
	                      .lastRow = std::min(region.lastRow, side - 1),
	                      .firstColumn = std::max(region.firstColumn, 0),
	                      .lastColumn = std::min(region.lastColumn, side - 1)};
	if (inBlock.Empty())
	{
		return;
	}
	const float rowStart = static_cast<float>(blockIndex.x) * k_BlockUnits;
	const float columnStart = static_cast<float>(blockIndex.y) * k_BlockUnits;
	const auto texels = static_cast<float>(side);
	// The game rounds each step to a float, in this order
	const auto toBlock = [&](glm::vec2 ground, glm::vec2 texel) {
		const float worldX = ((placement.xFromZ * ground.y + placement.xFromX * ground.x) + 0.0f) + placement.origin.x;
		const float worldZ = ((placement.zFromX * ground.x + placement.zFromZ * ground.y) + 0.0f) + placement.origin.y;
		return Corner {
		    .row = ((worldX - rowStart) * texels) * k_UnitPerBlock,
		    .column = ((worldZ - columnStart) * texels) * k_UnitPerBlock,
		    .u = texel.x,
		    .v = texel.y,
		};
	};
	Spans spans {.side = side, .first = side};
	for (const auto& triangle : triangles)
	{
		std::array<Corner, 3> corners {};
		for (size_t i = 0; i < corners.size(); ++i)
		{
			corners.at(i) = toBlock(triangle.corners.at(i), triangle.texels.at(i));
		}
		WalkEdge(spans, corners[0], corners[1]);
		WalkEdge(spans, corners[1], corners[2]);
		WalkEdge(spans, corners[2], corners[0]);
		FillSpans(spans, block, picture, mix, inBlock);
	}
}

bool PaintsEveryBlock(std::span<const size_t> wholeBlocks, size_t blockCount)
{
	if (blockCount == 0 || wholeBlocks.size() < blockCount)
	{
		return false;
	}
	std::vector<bool> painted(blockCount, false);
	size_t count = 0;
	for (const auto block : wholeBlocks)
	{
		if (block < blockCount && !painted[block])
		{
			painted[block] = true;
			++count;
		}
	}
	return count == blockCount;
}

} // namespace openblack::footprint_paint
