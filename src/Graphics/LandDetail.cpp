/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandDetail.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::graphics::land_detail
{

namespace
{

/// Below this, a component of the line's direction along the ground counts as nothing
constexpr float k_SmallestDirection = 1e-4f;

enum class Place : uint8_t
{
	Before,
	InBand,
	Beyond,
};

Place PlaceOf(float offset, float band)
{
	if (offset > band)
	{
		return Place::Before;
	}
	return offset < -band ? Place::Beyond : Place::InBand;
}

/// Where a block's four corners lie about one line
struct Corners
{
	bool before {false};
	bool inBand {false};
	bool beyond {false};
};

Corners CornersOf(const std::array<float, 4>& offsets, float band)
{
	Corners corners;
	for (const float offset : offsets)
	{
		switch (PlaceOf(offset, band))
		{
		case Place::Before:
			corners.before = true;
			break;
		case Place::InBand:
			corners.inBand = true;
			break;
		case Place::Beyond:
			corners.beyond = true;
			break;
		}
	}
	return corners;
}

uint8_t Average(uint8_t a, uint8_t b)
{
	return static_cast<uint8_t>((a + b) / 2);
}

uint16_t Corner(size_t x, size_t z)
{
	return static_cast<uint16_t>(x * k_CornersPerSide + z);
}

/// Adds the triangle turned the same way as every other: anticlockwise in x and z
void AddTriangle(std::vector<uint16_t>& indices, uint16_t a, uint16_t b, uint16_t c)
{
	const auto xz = [](uint16_t corner) { return glm::ivec2(corner / k_CornersPerSide, corner % k_CornersPerSide); };
	const glm::ivec2 ab = xz(b) - xz(a);
	const glm::ivec2 ac = xz(c) - xz(a);
	if (ab.x * ac.y - ab.y * ac.x >= 0)
	{
		indices.insert(indices.end(), {a, b, c});
	}
	else
	{
		indices.insert(indices.end(), {a, c, b});
	}
}

/// A side of a block, as the corners along its edge and along the row of corners a step in from it
struct Side
{
	uint8_t bit;
	/// The edge's corner at a place along the side, and the inner row's
	uint16_t (*edge)(size_t along, size_t step);
	uint16_t (*inner)(size_t along, size_t step);
	/// The sides at the start and the end of this one
	uint8_t startSide;
	uint8_t endSide;
};

constexpr size_t k_Last = k_CornersPerSide - 1;

constexpr std::array<Side, 4> k_Sides = {{
    {CoarserLowX, [](size_t along, size_t) { return Corner(0, along); },
     [](size_t along, size_t step) { return Corner(step, along); }, CoarserLowZ, CoarserHighZ},
    {CoarserHighX, [](size_t along, size_t) { return Corner(k_Last, along); },
     [](size_t along, size_t step) { return Corner(k_Last - step, along); }, CoarserLowZ, CoarserHighZ},
    {CoarserLowZ, [](size_t along, size_t) { return Corner(along, 0); },
     [](size_t along, size_t step) { return Corner(along, step); }, CoarserLowX, CoarserHighX},
    {CoarserHighZ, [](size_t along, size_t) { return Corner(along, k_Last); },
     [](size_t along, size_t step) { return Corner(along, k_Last - step); }, CoarserLowX, CoarserHighX},
}};

} // namespace

Lines For(uint8_t level)
{
	return k_Lines.at(std::min<size_t>(level, k_Lines.size() - 1));
}

ViewLine LineAcross(glm::vec3 cameraPosition, glm::vec3 cameraForward, float depth, float groundHeight)
{
	const glm::vec2 alongGround {cameraForward.x, cameraForward.z};
	ViewLine line;
	line.camera = {cameraPosition.x, cameraPosition.z};
	line.valid = std::max(alongGround.x * alongGround.x, alongGround.y * alongGround.y) > k_SmallestDirection;
	if (!line.valid)
	{
		return line;
	}
	const float length = glm::length(alongGround);
	line.forward = alongGround / length;
	// The plane holds the points whose depth along the view is depth; on the ground at groundHeight that is where the
	// distance along the ground makes up what the drop from the camera's height doesn't
	line.ahead = (depth + cameraForward.y * (cameraPosition.y - groundHeight)) / length;
	return line;
}

float Offset(const ViewLine& line, glm::vec2 xz)
{
	return line.valid ? line.ahead - glm::dot(xz - line.camera, line.forward) : 0.0f;
}

BlockDetail Classify(const CornerOffsets& offsets, const Lines& lines, bool coarseLand)
{
	BlockDetail detail;

	const auto nearLine = CornersOf(offsets.meshNear, lines.meshBand);
	if (nearLine.inBand || (nearLine.before && nearLine.beyond))
	{
		detail.morph = Morph::ToHalf;
	}
	else if (!nearLine.before)
	{
		const auto farLine = CornersOf(offsets.meshFar, lines.meshBand);
		if (!farLine.inBand && !farLine.before)
		{
			detail.mesh = Mesh::Quarter;
		}
		else
		{
			detail.mesh = Mesh::Half;
			if (farLine.inBand || farLine.beyond)
			{
				detail.morph = Morph::ToQuarter;
			}
		}
	}

	if (coarseLand)
	{
		detail.texture = Texture::Coarse;
	}
	else
	{
		const auto textureLine = CornersOf(offsets.texture, lines.textureBand);
		if (textureLine.inBand || (textureLine.before && textureLine.beyond))
		{
			detail.texture = Texture::CrossFade;
		}
		else if (!textureLine.before)
		{
			detail.texture = Texture::Coarse;
		}
	}
	return detail;
}

uint8_t CoarserSides(Mesh mesh, std::span<const std::optional<Mesh>, 4> lowXLowZHighZHighX)
{
	constexpr std::array<uint8_t, 4> k_Bits = {CoarserLowX, CoarserLowZ, CoarserHighZ, CoarserHighX};
	uint8_t sides = 0;
	for (size_t i = 0; i < k_Bits.size(); ++i)
	{
		const auto& neighbour = lowXLowZHighZHighX[i];
		if (neighbour.has_value() && *neighbour > mesh)
		{
			sides = static_cast<uint8_t>(sides | k_Bits.at(i));
		}
	}
	return sides;
}

std::vector<uint16_t> BlockTriangles(Mesh mesh, uint8_t coarserSides, bool dropOpenSea,
                                     std::span<const CellFlags, k_CornerCount> cells)
{
	const size_t step = size_t {1} << static_cast<size_t>(mesh);
	const size_t quads = (k_CornersPerSide - 1) / step;
	// The quarter mesh is the coarsest: nothing borders it with a coarser one
	const uint8_t stitched = mesh == Mesh::Quarter ? 0 : coarserSides;

	std::vector<uint16_t> indices;
	indices.reserve(quads * quads * 6);

	// The quads the stitched strips leave
	const size_t firstRow = (stitched & CoarserLowX) != 0 ? 1 : 0;
	const size_t endRow = quads - ((stitched & CoarserHighX) != 0 ? 1 : 0);
	const size_t firstColumn = (stitched & CoarserLowZ) != 0 ? 1 : 0;
	const size_t endColumn = quads - ((stitched & CoarserHighZ) != 0 ? 1 : 0);
	const size_t cellsPerRow = endColumn - firstColumn + 1;
	for (size_t row = firstRow; row < endRow; ++row)
	{
		for (size_t column = firstColumn; column < endColumn; ++column)
		{
			const auto& flags = cells[(row - firstRow) * cellsPerRow + (column - firstColumn)];
			if (dropOpenSea && flags.openSea)
			{
				continue;
			}
			const uint16_t lowLow = Corner(row * step, column * step);
			const uint16_t lowHigh = Corner(row * step, (column + 1) * step);
			const uint16_t highLow = Corner((row + 1) * step, column * step);
			const uint16_t highHigh = Corner((row + 1) * step, (column + 1) * step);
			if (flags.split)
			{
				AddTriangle(indices, lowHigh, highHigh, highLow);
				AddTriangle(indices, lowHigh, highLow, lowLow);
			}
			else
			{
				AddTriangle(indices, lowLow, lowHigh, highHigh);
				AddTriangle(indices, lowLow, highHigh, highLow);
			}
		}
	}

	// Each stitched strip: along the edge every second corner of the mesh, a fan of three triangles to each pair of
	// them from the row of the mesh's corners a step in. Where the strip meets another stitched strip, the triangle
	// reaching the other strip's edge is left to that strip.
	for (const auto& side : k_Sides)
	{
		if ((stitched & side.bit) == 0)
		{
			continue;
		}
		for (size_t along = 0; along + 2 * step <= k_Last; along += 2 * step)
		{
			const uint16_t edgeStart = side.edge(along, step);
			const uint16_t edgeEnd = side.edge(along + 2 * step, step);
			const uint16_t innerStart = side.inner(along, step);
			const uint16_t innerMiddle = side.inner(along + step, step);
			const uint16_t innerEnd = side.inner(along + 2 * step, step);
			AddTriangle(indices, edgeStart, edgeEnd, innerMiddle);
			if (along != 0 || (stitched & side.startSide) == 0)
			{
				AddTriangle(indices, edgeStart, innerMiddle, innerStart);
			}
			if (along + 2 * step != k_Last || (stitched & side.endSide) == 0)
			{
				AddTriangle(indices, edgeEnd, innerEnd, innerMiddle);
			}
		}
	}
	return indices;
}

MorphTargets TargetsAt(std::span<const uint8_t, k_CornerCount> altitudes, size_t x, size_t z)
{
	const auto at = [&](size_t cornerX, size_t cornerZ) { return altitudes[cornerX * k_CornersPerSide + cornerZ]; };
	const auto towards = [&](size_t step) {
		const bool oddX = (x / step) % 2 == 1;
		const bool oddZ = (z / step) % 2 == 1;
		if (oddX && oddZ)
		{
			return Average(at(x - step, z - step), at(x + step, z + step));
		}
		if (oddX)
		{
			return Average(at(x - step, z), at(x + step, z));
		}
		if (oddZ)
		{
			return Average(at(x, z - step), at(x, z + step));
		}
		return at(x, z);
	};
	MorphTargets targets {.half = towards(1), .quarter = at(x, z)};
	// Only the half mesh's corners morph to the quarter mesh
	if (x % 2 == 0 && z % 2 == 0)
	{
		targets.quarter = towards(2);
	}
	return targets;
}

uint8_t MorphedAltitude(uint8_t own, uint8_t target, float offset, float band)
{
	if (offset >= band)
	{
		return own;
	}
	if (offset <= -band)
	{
		return target;
	}
	const auto share = static_cast<int>(std::nearbyint((band - offset) * 256.0f / (2.0f * band)));
	return static_cast<uint8_t>(((256 - share) * own + share * target) >> 8);
}

uint8_t CrossFadeAlpha(float offset, float band)
{
	if (offset > band)
	{
		return 0;
	}
	if (offset > -band)
	{
		return static_cast<uint8_t>(std::nearbyint((band - offset) * 255.0f / (2.0f * band)));
	}
	return 255;
}

uint16_t CoarseSourceTexel(uint8_t coarseTexel)
{
	// 62 samples across 256 texels, a step of 1.0323 in 65536ths, each on a texel a multiple of four
	constexpr uint32_t k_Step = 0x10842;
	const uint32_t sample = std::clamp<uint32_t>(coarseTexel, 1, 62) - 1;
	return static_cast<uint16_t>(((sample * k_Step) >> 16) * 4);
}

float CoarseTexelPosition(float fraction)
{
	// A texel in from the coarse texture's edge, then 62 texels across the block
	return 256.0f / 255.0f + fraction * 62.0f;
}

} // namespace openblack::graphics::land_detail
