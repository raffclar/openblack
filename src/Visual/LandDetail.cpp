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
#include <bit>

#include <glm/geometric.hpp>

#include "Common/NearestWhole.h"

namespace openblack::graphics::land_detail
{

namespace
{

/// Below this, a component of the line's direction along the ground counts as nothing
constexpr float k_SmallestDirection = 1e-4f;
/// The map has 32 by 32 blocks, each 160 units a side and 16 cells of 10 units
constexpr int k_BlocksPerSide = 32;
constexpr float k_BlockSize = 160.0f;
constexpr float k_CellSize = 10.0f;

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

GroundLine GroundLineAcross(glm::vec3 cameraPosition, glm::vec3 cameraForward, float depth, float groundHeight,
                            const GroundLine& before)
{
	const glm::vec3& c = cameraPosition;
	const glm::vec3& f = cameraForward;
	// The plane square to the view through the point depth units along it: f . p + d = 0
	const float px = c.x + depth * f.x;
	const float py = c.y + depth * f.y;
	const float pz = depth * f.z + c.z;
	const float d = -((pz * f.z + py * f.y) + px * f.x);
	// The line runs along the cross product of the up direction and the view. Each step is a float step of its own, in
	// the game's order, also the steps with zero. The cross product has no y: its square adds nothing.
	const float alongX = f.z * 1.0f - 0.0f * f.y;
	const float alongZ = 0.0f * f.y - f.x * 1.0f;
	const float squareX = alongX * alongX;
	const float squareZ = alongZ * alongZ;
	GroundLine line {.along = {alongX, alongZ}, .point = before.point};
	const float lowHeight = -groundHeight;
	if (squareZ > 0.0f && squareZ > squareX && squareZ > k_SmallestDirection)
	{
		// The point on the line where z is 0
		const float scale = 1.0f / alongZ;
		line.point = {scale * (d - lowHeight * f.y), scale * 0.0f};
	}
	else if (squareX > k_SmallestDirection)
	{
		// The point on the line where x is 0
		const float scale = 1.0f / alongX;
		line.point = {scale * 0.0f, scale * (lowHeight * f.y - d)};
	}
	else
	{
		return line;
	}
	const float unit = 1.0f / std::sqrt(squareZ + squareX);
	line.along = {unit * alongX, unit * alongZ};
	return line;
}

float SideOf(const GroundLine& line, float x, float z)
{
	return line.along.y * x - line.along.x * z;
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

uint8_t MorphedAltitude(uint8_t own, uint8_t target, float side, float width)
{
	const float scale = 256.0f / width;
	const float half = width * 0.5f;
	if (!(side < half))
	{
		return own;
	}
	if (!(-half < side))
	{
		return target;
	}
	// The share of the target, rounded to the nearest whole number and to the even one at a half
	const float scaled = (half - side) * scale;
	const auto share = static_cast<uint32_t>(NearestWhole(scaled));
	return static_cast<uint8_t>(((256u - share) * own + share * target) >> 8);
}

float MorphCornerSide(const GroundLine& line, Morph morph, glm::vec2 blockCorner, size_t x, size_t z)
{
	// The coarser mesh's step, and the first corner between its corners, in cells
	const size_t step = morph == Morph::ToQuarter ? 4 : 2;
	const size_t first = step / 2;
	const float stepUnits = static_cast<float>(step) * k_CellSize;
	const float firstUnits = static_cast<float>(first) * k_CellSize;
	// A corner on a row of the coarser mesh starts from the block's corner, the others from the first corner between.
	// Then the place steps on by the coarser mesh's step, one float sum at a time.
	const bool onRowX = x % step == 0;
	const bool onRowZ = z % step == 0;
	float alongX = onRowX ? blockCorner.x - line.point.x : (firstUnits + blockCorner.x) - line.point.x;
	float alongZ = onRowZ ? blockCorner.y - line.point.y : (firstUnits + blockCorner.y) - line.point.y;
	for (size_t at = onRowX ? 0 : first; at < x; at += step)
	{
		alongX = stepUnits + alongX;
	}
	for (size_t at = onRowZ ? 0 : first; at < z; at += step)
	{
		alongZ = alongZ + stepUnits;
	}
	return SideOf(line, alongX, alongZ);
}

std::array<uint8_t, k_CornerCount> MorphedCorners(std::span<const uint8_t, k_CornerCount> altitudes, Morph morph,
                                                  const GroundLine& line, float width, glm::vec2 blockCorner)
{
	std::array<uint8_t, k_CornerCount> drawn {};
	std::ranges::copy(altitudes, drawn.begin());
	if (morph == Morph::None)
	{
		return drawn;
	}
	const size_t step = morph == Morph::ToQuarter ? 4 : 2;
	const size_t first = step / 2;
	const float stepUnits = static_cast<float>(step) * k_CellSize;
	const float firstUnits = static_cast<float>(first) * k_CellSize;
	// Only the corners between the coarser mesh's corners morph: on the half mesh for the morph to the quarter mesh. The
	// game takes them in three sets, each row by row: those between along z, those between along both, and those
	// between along x. Each place steps on from the one before by one float sum, as MorphCornerSide finds it.
	const auto morphSet = [&](size_t firstX, size_t firstZ) {
		float alongX = (firstX == 0 ? blockCorner.x : firstUnits + blockCorner.x) - line.point.x;
		const float startZ = (firstZ == 0 ? blockCorner.y : firstUnits + blockCorner.y) - line.point.y;
		for (size_t x = firstX; x < k_CornersPerSide; x += step)
		{
			float alongZ = startZ;
			for (size_t z = firstZ; z < k_CornersPerSide; z += step)
			{
				const auto targets = TargetsAt(altitudes, x, z);
				const uint8_t target = morph == Morph::ToQuarter ? targets.quarter : targets.half;
				const size_t corner = x * k_CornersPerSide + z;
				drawn.at(corner) = MorphedAltitude(altitudes[corner], target, SideOf(line, alongX, alongZ), width);
				alongZ = alongZ + stepUnits;
			}
			alongX = stepUnits + alongX;
		}
	};
	morphSet(0, first);
	morphSet(first, first);
	morphSet(first, 0);
	return drawn;
}

bool SameCornerInputs(const BlockCornerInputs& a, const BlockCornerInputs& b)
{
	const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
	const auto sameLine = [&bits](const GroundLine& x, const GroundLine& y) {
		return bits(x.along.x) == bits(y.along.x) && bits(x.along.y) == bits(y.along.y) && bits(x.point.x) == bits(y.point.x) &&
		       bits(x.point.y) == bits(y.point.y);
	};
	return a.morph == b.morph && a.fades == b.fades && sameLine(a.meshLine, b.meshLine) &&
	       bits(a.meshWidth) == bits(b.meshWidth) && sameLine(a.textureLine, b.textureLine) &&
	       bits(a.textureWidth) == bits(b.textureWidth) && bits(a.textureBand) == bits(b.textureBand) &&
	       bits(a.blockCorner.x) == bits(b.blockCorner.x) && bits(a.blockCorner.y) == bits(b.blockCorner.y) &&
	       a.altitudes == b.altitudes;
}

uint8_t CrossFadeAlpha(float side, float width, float band)
{
	const float scale = 255.0f / width;
	if (side > band)
	{
		return 0;
	}
	if (-band < side)
	{
		// Rounded to the nearest whole number and to the even one at a half
		const float scaled = (band - side) * scale;
		return static_cast<uint8_t>(NearestWhole(scaled));
	}
	return 255;
}

std::array<uint8_t, k_CornerCount> CrossFadeCorners(const GroundLine& line, float width, float band, glm::vec2 blockCorner)
{
	std::array<uint8_t, k_CornerCount> alphas {};
	for (size_t x = 0; x < k_CornersPerSide; ++x)
	{
		for (size_t z = 0; z < k_CornersPerSide; ++z)
		{
			// The game finds the corner's place again from its texture coordinates, in 16ths across the block
			const float acrossX = static_cast<float>(x) * 0.0625f;
			const float acrossZ = static_cast<float>(z) * 0.0625f;
			const float alongX = (acrossX * k_BlockSize + blockCorner.x) - line.point.x;
			const float alongZ = (acrossZ * k_BlockSize + blockCorner.y) - line.point.y;
			const float side = alongX * line.along.y - alongZ * line.along.x;
			alphas.at(x * k_CornersPerSide + z) = CrossFadeAlpha(side, width, band);
		}
	}
	return alphas;
}

uint16_t CoarseSourceTexel(uint8_t coarseTexel, const CoarseSampling& sampling)
{
	// The samples step evenly in 65536ths across the block's texture, each on a texel a multiple of four
	const uint32_t sample = std::clamp<uint32_t>(coarseTexel, 1, sampling.samples) - 1;
	return static_cast<uint16_t>(((sample * sampling.step) >> 16) * 4);
}

float CoarseTexelPosition(float fraction, const CoarseSampling& sampling)
{
	// The page's 255th part in from the coarse texture's edge, then across the block
	return static_cast<float>(sampling.blockTexels) / 255.0f + fraction * sampling.drawnTexels;
}

FrameDetail ClassifyFrame(const FrameInputs& inputs, std::span<const BlockPlace> blocks, const FrameDetail* before)
{
	FrameDetail frame;
	frame.lines = inputs.lines;
	const float textureGround = std::min(inputs.origin.y, k_TextureGroundCap);
	frame.meshNear = LineAcross(inputs.origin, inputs.forward, inputs.lines.meshNear, 0.0f);
	frame.meshFar = LineAcross(inputs.origin, inputs.forward, inputs.lines.meshFar, 0.0f);
	frame.texture = LineAcross(inputs.origin, inputs.forward, inputs.lines.texture, textureGround);
	const FrameDetail start;
	const FrameDetail& kept = before != nullptr ? *before : start;
	frame.meshNearGround = GroundLineAcross(inputs.origin, inputs.forward, inputs.lines.meshNear, 0.0f, kept.meshNearGround);
	frame.meshFarGround = GroundLineAcross(inputs.origin, inputs.forward, inputs.lines.meshFar, 0.0f, kept.meshFarGround);
	frame.textureGround =
	    GroundLineAcross(inputs.origin, inputs.forward, inputs.lines.texture, textureGround, kept.textureGround);

	// The blocks by where they are on the map's grid of blocks
	std::array<int16_t, static_cast<size_t>(k_BlocksPerSide * k_BlocksPerSide)> grid {};
	grid.fill(-1);
	const auto onGrid = [](glm::ivec2 at) {
		return at.x >= 0 && at.x < k_BlocksPerSide && at.y >= 0 && at.y < k_BlocksPerSide;
	};
	const auto gridIndex = [](glm::ivec2 at) { return static_cast<size_t>(at.x * k_BlocksPerSide + at.y); };

	frame.blocks.resize(blocks.size());
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const auto& block = blocks[i];
		if (onGrid(block.at))
		{
			grid.at(gridIndex(block.at)) = static_cast<int16_t>(i);
		}
		const std::array<glm::vec2, 4> corners = {block.corner, block.corner + glm::vec2(0.0f, k_BlockSize),
		                                          block.corner + glm::vec2(k_BlockSize, 0.0f),
		                                          block.corner + glm::vec2(k_BlockSize)};
		CornerOffsets offsets;
		for (size_t corner = 0; corner < corners.size(); ++corner)
		{
			offsets.meshNear.at(corner) = Offset(frame.meshNear, corners.at(corner));
			offsets.meshFar.at(corner) = Offset(frame.meshFar, corners.at(corner));
			offsets.texture.at(corner) = Offset(frame.texture, corners.at(corner));
		}
		frame.blocks[i] = Classify(offsets, inputs.lines, inputs.coarseLand);
	}

	const auto indexAt = [&](glm::ivec2 at) -> int16_t { return onGrid(at) ? grid.at(gridIndex(at)) : int16_t {-1}; };
	frame.coarserSides.resize(blocks.size());
	frame.neighbours.resize(blocks.size());
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const auto at = blocks[i].at;
		const std::array<int16_t, 4> neighbours = {indexAt(at - glm::ivec2(1, 0)), indexAt(at - glm::ivec2(0, 1)),
		                                           indexAt(at + glm::ivec2(0, 1)), indexAt(at + glm::ivec2(1, 0))};
		std::array<std::optional<Mesh>, 4> meshes;
		std::ranges::transform(neighbours, meshes.begin(), [&frame](int16_t index) {
			return index < 0 ? std::nullopt : std::optional(frame.blocks[static_cast<size_t>(index)].mesh);
		});
		frame.coarserSides[i] = CoarserSides(frame.blocks[i].mesh, meshes);
		frame.neighbours[i] = neighbours;
	}
	return frame;
}

uint8_t DrawnCornerAltitude(std::span<const uint8_t, k_CornerCount> altitudes, size_t x, size_t z, Morph morph,
                            const FrameDetail& frame, glm::vec2 blockCorner)
{
	const size_t corner = x * k_CornersPerSide + z;
	const uint8_t own = altitudes[corner];
	const size_t step = morph == Morph::ToQuarter ? 4 : 2;
	const size_t first = step / 2;
	// Only the corners between the coarser mesh's corners morph, as the land draws them (see MorphedCorners)
	if (morph == Morph::None || x % first != 0 || z % first != 0 || (x % step == 0 && z % step == 0))
	{
		return own;
	}
	const auto targets = TargetsAt(altitudes, x, z);
	const bool toHalf = morph == Morph::ToHalf;
	const auto& line = toHalf ? frame.meshNearGround : frame.meshFarGround;
	return MorphedAltitude(own, toHalf ? targets.half : targets.quarter, MorphCornerSide(line, morph, blockCorner, x, z),
	                       2.0f * frame.lines.meshBand);
}

const FrameDetail& FrameMemo::For(const FrameInputs& inputs, std::span<const BlockPlace> blocks)
{
	if (_inputs != inputs || !std::ranges::equal(_blocks, blocks))
	{
		_frame = ClassifyFrame(inputs, blocks, _classified ? &_frame : nullptr);
		_classified = true;
		_inputs = inputs;
		_blocks.assign(blocks.begin(), blocks.end());
		++_classifications;
	}
	return _frame;
}

} // namespace openblack::graphics::land_detail
