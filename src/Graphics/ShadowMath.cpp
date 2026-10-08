/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowMath.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include <glm/vec4.hpp>

#include "Common/TruncateToInt.h"

namespace openblack::graphics::shadow_math
{
namespace
{
/// The fade's block test of one cell: exists, (visible), nearer than 100000
BlockState BlockOfCell(const BlockAt& blocks, int cellX, int cellZ, int cellLimit, bool& inRange)
{
	inRange = cellX >= 0 && cellX <= cellLimit && cellZ >= 0 && cellZ <= cellLimit;
	if (!inRange)
	{
		return {};
	}
	return blocks(cellX >> 4, cellZ >> 4); // 16 cells per block side, 32 blocks a row
}

/// The span tables of the rasterizer: the left ends (edges going up), the right ends (edges going down), and the rows
/// the triangle touched. The tables are never cleared, as in the original: each triangle only resets the rows (Begin),
/// and every row of [minRow, maxRow) is written again by its edges
struct Spans
{
	explicit Spans(int rows)
	    : left(static_cast<size_t>(rows), 0)
	    , right(static_cast<size_t>(rows), 0)
	    , minRow(rows)
	{
	}
	/// Empty row range: minRow = grid rows, maxRow = 0
	void Begin(int rows)
	{
		minRow = rows;
		maxRow = 0;
	}
	std::vector<int> left;
	std::vector<int> right;
	int minRow;
	int maxRow {0};
};

/// One edge: rows [y0, y1) half open, clipped to [0, rows); x in 16.16 fixed point from x0 with the step
/// (x1 - x0) / (y1 - y0), stored >> 16
void Edge(int y0, int y1, float x0, float x1, int rows, Spans& spans)
{
	if (y0 == y1)
	{
		return;
	}
	auto* table = &spans.right;
	if (y0 > y1) // swapped, into the left ends
	{
		std::swap(y0, y1);
		std::swap(x0, x1);
		table = &spans.left;
	}
	if (y0 >= rows || y1 <= 0)
	{
		return;
	}
	const float slope = (x1 - x0) / static_cast<float>(y1 - y0);
	if (y1 > rows)
	{
		y1 = rows;
	}
	if (y0 < 0) // x0 - y0 slope, stored as a float
	{
		x0 = x0 - static_cast<float>(y0) * slope;
		y0 = 0;
	}
	spans.minRow = std::min(spans.minRow, y0);
	spans.maxRow = std::max(spans.maxRow, y1);
	const int count = y1 - y0;
	if (count <= 0)
	{
		return;
	}
	const int step = TruncateToInt(slope * 65536.0f);
	int x = TruncateToInt(x0 * 65536.0f);
	for (int row = y0; row < y0 + count; ++row)
	{
		(*table)[static_cast<size_t>(row)] = x >> 16;
		x += step;
	}
}

/// Each row of [minRow, maxRow) fills [max(0, l), min(grid x, r)) of its subrow; the odd subrows set the high nibbles,
/// the even ones the low nibbles unless `halfRows`; two subrows to a texel row
void Fill(const Spans& spans, bool halfRows, Coverage& coverage)
{
	const int gridX = coverage.GridX();
	for (int row = spans.minRow; row < spans.maxRow; ++row)
	{
		int left = spans.left[static_cast<size_t>(row)];
		if (left < 0)
		{
			left = 0;
		}
		int right = spans.right[static_cast<size_t>(row)];
		if (right > gridX)
		{
			right = gridX;
		}
		if (right <= left || left >= gridX || right <= 0)
		{
			continue;
		}
		const bool odd = (row & 1) != 0;
		if (!odd && halfRows)
		{
			continue;
		}
		auto* bytes = coverage.bytes.data() + static_cast<size_t>(row >> 1) * static_cast<size_t>(coverage.texels);
		const int shift = odd ? 4 : 0;
		for (int x = left; x < right; ++x)
		{
			bytes[x >> 2] = static_cast<uint8_t>(bytes[x >> 2] | (1u << ((x & 3) + shift)));
		}
	}
}
} // namespace

float Fade(glm::vec3 position, float ground, glm::vec3 camera, float scale, float meshRadius, const BlockAt& blocks,
           int cellLimit)
{
	// The block under the caster, when there is one, must be nearer than 100000
	bool inRange = false;
	const auto centre = BlockOfCell(blocks, TruncateToInt(position.x * k_CellScale), TruncateToInt(position.z * k_CellScale),
	                                cellLimit, inRange);
	if (inRange && centre.exists && !(centre.distance < k_BlockFar))
	{
		return 0.0f;
	}
	// Any of the 3 x 3 neighbours, x outer and z inner, from -1 to 1
	bool found = false;
	for (int i = -1; i < 2 && !found; ++i)
	{
		for (int j = -1; j < 2 && !found; ++j)
		{
			const int cellX = TruncateToInt((static_cast<float>(i) * k_NeighbourStep + position.x) * k_CellScale);
			const int cellZ = TruncateToInt((static_cast<float>(j) * k_NeighbourStep + position.z) * k_CellScale);
			const auto block = BlockOfCell(blocks, cellX, cellZ, cellLimit, inRange);
			found = inRange && block.exists && block.visible && block.distance < k_BlockFar;
		}
	}
	if (!found)
	{
		return 0.0f;
	}
	// dx, dy, dz stored as floats; ((dz dz + dy dy) + dx dx), sqrt, / (scale x radius)
	const float dx = position.x - camera.x;
	const float dy = ground - camera.y;
	const float dz = position.z - camera.z;
	const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
	const float q = distance / (scale * meshRadius);
	if (q < k_FadeFull)
	{
		return k_FadeMax;
	}
	if (!(q <= k_FadeGone))
	{
		return 0.0f;
	}
	return k_FadeMax - (q - k_FadeFull) * k_FadeMax / (k_FadeGone - k_FadeFull);
}

int AlphaGeneric(float fade, int base)
{
	return TruncateToInt(static_cast<float>(base) * fade * k_InvByte); // base x fade x (1 / 255)
}

int AlphaComplex(float fade, int base)
{
	return fade < k_FadeMax ? AlphaGeneric(fade, base) : 255;
}

glm::vec3 LightGeneric(glm::vec3 position, bool useSun)
{
	if (useSun)
	{
		// The fixed sun (-500000, 500000, -500000)
		return {std::bit_cast<float>(0xC8F42400u), std::bit_cast<float>(0x48F42400u), std::bit_cast<float>(0xC8F42400u)};
	}
	return {position.x, position.y + k_VerticalLight, position.z};
}

glm::vec3 LightHand(glm::vec3 position)
{
	return {position.x, position.y + k_HandLight, position.z};
}

glm::vec3 LightCreature(glm::vec3 position, glm::vec3 light, float meshRadius, float scale)
{
	const float radius = meshRadius * scale * k_CreatureRadii;
	float dx = light.x - position.x;
	float dy = light.y - position.y;
	float dz = light.z - position.z;
	float horizontal = std::sqrt(dz * dz + dx * dx);
	if (horizontal < radius)
	{
		if (static_cast<double>(horizontal) < k_CreatureNear) // compared as a double
		{
			dx = dx + 1.0f;
			dz = dz + 1.0f;
		}
		if (!(dx == 0.0f && dy == 0.0f && dz == 0.0f))
		{
			const float factor = radius / std::sqrt(dy * dy + dz * dz + dx * dx);
			dx = dx * factor;
			dy = factor * dy;
			dz = factor * dz;
		}
		horizontal = std::sqrt(dz * dz + dx * dx);
	}
	if (dy / horizontal < 1.0f)
	{
		dy = horizontal;
	}
	return {position.x + dx, position.y + dy, position.z + dz};
}

Projection MakeProjection(glm::vec3 position, glm::vec3 light)
{
	return {light, position - light, position.y};
}

glm::vec2 Project(const Projection& projection, const glm::mat4& matrix, glm::vec3 local, Box& box)
{
	const float ty = -projection.baseY + matrix[3][1];
	const float wx = matrix[2][0] * local.z + matrix[1][0] * local.y + matrix[0][0] * local.x + matrix[3][0];
	const float wy = matrix[0][1] * local.x + matrix[2][1] * local.z + matrix[1][1] * local.y + ty;
	const float wz = matrix[0][2] * local.x + matrix[2][2] * local.z + matrix[1][2] * local.y + matrix[3][2];
	const float h = wy < 0.0f ? 0.0f : wy;
	const float k = wz * projection.dir.z + wx * projection.dir.x;
	if (k < box.kMin)
	{
		box.kMin = k;
	}
	const float t = -projection.light.y / (h - projection.light.y);
	const glm::vec2 p((wx - projection.light.x) * t + projection.light.x, (wz - projection.light.z) * t + projection.light.z);
	if (p.x < box.x0)
	{
		box.x0 = p.x;
	}
	if (p.x > box.x1)
	{
		box.x1 = p.x;
	}
	if (p.y < box.z0)
	{
		box.z0 = p.y;
	}
	if (p.y > box.z1)
	{
		box.z1 = p.y;
	}
	return p;
}

Coverage::Coverage(int side)
    : texels(side)
    , bytes(static_cast<size_t>(side) * static_cast<size_t>(side), 0)
{
}

void Coverage::Clear()
{
	std::fill(bytes.begin(), bytes.end(), uint8_t {0});
}

void ToGrid(const Box& box, std::span<glm::vec2> points, int texels)
{
	const auto gridX = static_cast<float>(texels * 4); // 128 for 32 texels
	const auto gridZ = static_cast<float>(texels * 2); // 64 for 32 texels
	const float scaleX = gridX / (box.x1 - box.x0);
	const float scaleZ = gridZ / (box.z1 - box.z0);
	for (auto& point : points)
	{
		point.x = (point.x - box.x0) * scaleX;
		if (point.x < 0.0f)
		{
			point.x = 0.0f;
		}
		else if (point.x > gridX - 1.0f)
		{
			point.x = gridX - 1.0f;
		}
		point.y = (point.y - box.z0) * scaleZ;
		if (point.y < 0.0f)
		{
			point.y = 0.0f;
		}
		else if (point.y > gridZ - 1.0f)
		{
			point.y = gridZ - 1.0f;
		}
	}
}

void RasterTriangles(std::span<const glm::vec2> grid, std::span<const uint16_t> indices, bool bothFaces, bool halfRows,
                     Coverage& coverage)
{
	const int rows = coverage.GridZ();
	Spans spans(rows);
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		if (indices[i] >= grid.size() || indices[i + 1] >= grid.size() || indices[i + 2] >= grid.size())
		{
			continue; // (port guard)
		}
		const auto& p0 = grid[indices[i]];
		const auto& p1 = grid[indices[i + 1]];
		const auto& p2 = grid[indices[i + 2]];
		const int r0 = TruncateToInt(p0.y);
		const int r1 = TruncateToInt(p1.y);
		const int r2 = TruncateToInt(p2.y);
		spans.Begin(rows);
		if (!bothFaces)
		{
			// Back faces dropped
			const float a = static_cast<float>(r1 - r2) * (p0.x - p2.x);
			const float b = static_cast<float>(r0 - r2) * (p1.x - p2.x);
			if (!(b >= a))
			{
				continue;
			}
			Edge(r0, r2, p0.x, p2.x, rows, spans);
			Edge(r2, r1, p2.x, p1.x, rows, spans);
			Edge(r1, r0, p1.x, p0.x, rows, spans);
		}
		else
		{
			// The walk that makes the spans of either winding
			const float a = static_cast<float>(r2 - r1) * (p0.x - p1.x);
			const float b = static_cast<float>(r0 - r1) * (p2.x - p1.x);
			if (b < a)
			{
				Edge(r0, r2, p0.x, p2.x, rows, spans);
				Edge(r2, r1, p2.x, p1.x, rows, spans);
				Edge(r1, r0, p1.x, p0.x, rows, spans);
			}
			else
			{
				Edge(r0, r1, p0.x, p1.x, rows, spans);
				Edge(r1, r2, p1.x, p2.x, rows, spans);
				Edge(r2, r0, p2.x, p0.x, rows, spans);
			}
		}
		Fill(spans, halfRows, coverage);
	}
}

void Resolve(const Coverage& coverage, Texels& texels)
{
	const int side = coverage.texels;
	texels.assign(static_cast<size_t>(side) * static_cast<size_t>(side), 0);
	for (int row = 1; row < side - 1; ++row)
	{
		for (int column = 1; column < side - 1; ++column)
		{
			const auto index = static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column);
			texels[index] = static_cast<uint8_t>(std::popcount(coverage.bytes[index])); // subsamples covered
		}
	}
}

void ChromaFilter(std::span<const uint16_t> rendered, Texels& texels)
{
	const auto side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(texels.size()))));
	if (rendered.size() != texels.size())
	{
		return; // (port guard)
	}
	const auto at = [side](int row, int column) {
		return static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column);
	};
	for (int row = 1; row < side - 1; ++row)
	{
		for (int column = 1; column < side - 1; ++column)
		{
			const int sum = rendered[at(row, column)] + rendered[at(row, column + 1)] + rendered[at(row + 1, column)] +
			                rendered[at(row + 1, column + 1)];
			const int average = (sum / 4) & 0xF000; // signed division by 4, alpha nibble only
			texels[at(row, column)] = static_cast<uint8_t>(texels[at(row, column)] | (average >> 12)); // OR-ed in
		}
	}
}

void BakeAlpha(Texels& texels, int alpha)
{
	if (alpha == 255)
	{
		return;
	}
	const auto side = static_cast<int>(std::lround(std::sqrt(static_cast<double>(texels.size()))));
	for (int row = 1; row < side - 1; ++row)
	{
		for (int column = 1; column < side - 1; ++column)
		{
			auto& n = texels[static_cast<size_t>(row) * static_cast<size_t>(side) + static_cast<size_t>(column)];
			const int value = ((static_cast<int>(n) << 12) * alpha / 255) & 0xF000; // signed division by 255
			n = static_cast<uint8_t>(value >> 12);
		}
	}
}

AlphaMap MakeAlphaMap(std::span<const uint16_t> texels, int width, int height)
{
	AlphaMap map {};
	if (width <= 0 || height <= 0 || texels.size() < static_cast<size_t>(width) * static_cast<size_t>(height))
	{
		return map; // (port guard)
	}
	const float rowStep = static_cast<float>(height) * k_SixtyFourth;
	const float columnStep = static_cast<float>(width) * k_SixtyFourth;
	for (int row = 0; row < 64; ++row)
	{
		const int source = TruncateToInt(static_cast<float>(row) * rowStep) * width;
		for (int column = 0; column < 64; ++column)
		{
			const auto texel = texels[static_cast<size_t>(source + TruncateToInt(static_cast<float>(column) * columnStep))];
			map[static_cast<size_t>(row * 64 + column)] = static_cast<uint8_t>((texel >> 8) & 0xF0); // the alpha nibble
		}
	}
	return map;
}

glm::vec4 ChromaVertex(const Projection& projection, const Box& box, const glm::mat4& matrix, glm::vec3 local, glm::vec2 uv)
{
	// 32 / (x1 - x0) and 32 / (z1 - z0), stored as floats
	const float scaleX = k_ChromaSide / (box.x1 - box.x0);
	const float scaleZ = k_ChromaSide / (box.z1 - box.z0);
	const float base = projection.baseY - projection.light.y;
	// The object's matrix: (x m00 + z m20) + y m10 + t
	const float wx = local.x * matrix[0][0] + local.z * matrix[2][0] + local.y * matrix[1][0] + matrix[3][0];
	const float wy = local.x * matrix[0][1] + local.z * matrix[2][1] + local.y * matrix[1][1] + matrix[3][1];
	const float wz = local.x * matrix[0][2] + local.z * matrix[2][2] + local.y * matrix[1][2] + matrix[3][2];
	const float t = base / (wy - projection.light.y);
	float x = ((wx - projection.light.x) * t + projection.light.x - box.x0) * scaleX;
	if (x < 1.0f)
	{
		x = 1.0f;
	}
	else if (!(x <= k_ChromaSide - 1.0f))
	{
		x = k_ChromaSide - 1.0f;
	}
	float z = ((wz - projection.light.z) * t + projection.light.z - box.z0) * scaleZ;
	if (z < 1.0f)
	{
		z = 1.0f;
	}
	else if (!(z <= k_ChromaSide - 1.0f))
	{
		z = k_ChromaSide - 1.0f;
	}
	return {x, z, uv.x * 63.0f, uv.y * 63.0f};
}

namespace
{
/// A vertex of the chroma triangle: x, y (the row), u, v
struct ChromaPoint
{
	int x;
	int y;
	int u;
	int v;
};

/// An edge table entry: x, u, v
struct ChromaEdge
{
	int x {0};
	int u {0};
	int v {0};
};

/// A horizontal edge stores its two ends as they are, the lesser x into `down` (u, v NOT in 16.16, as the original);
/// else from the upper vertex, `down` going down and `up` going up, rows y0..y1 inclusive, x, u, v in 16.16 with the
/// steps d (1 / (y1 - y0 + 1)) 65536, truncated toward zero
void ChromaEdgeOf(ChromaPoint a, ChromaPoint b, std::vector<ChromaEdge>& down, std::vector<ChromaEdge>& up)
{
	if (a.y == b.y)
	{
		const auto& left = a.x < b.x ? a : b;
		const auto& right = a.x < b.x ? b : a;
		down[static_cast<size_t>(a.y)] = {left.x, left.u, left.v};
		up[static_cast<size_t>(a.y)] = {right.x, right.u, right.v};
		return;
	}
	auto* table = &down;
	if (a.y > b.y)
	{
		std::swap(a, b);
		table = &up;
	}
	const int count = b.y - a.y + 1;
	const float inverse = 1.0f / static_cast<float>(count);
	const int stepX = static_cast<int>(static_cast<float>(b.x - a.x) * inverse * 65536.0f);
	const int stepU = static_cast<int>(static_cast<float>(b.u - a.u) * inverse * 65536.0f);
	const int stepV = static_cast<int>(static_cast<float>(b.v - a.v) * inverse * 65536.0f);
	int x = a.x << 16;
	int u = a.u << 16;
	int v = a.v << 16;
	for (int row = a.y; row < a.y + count; ++row)
	{
		(*table)[static_cast<size_t>(row)] = {x >> 16, u, v}; // x as an integer, u and v kept in 16.16
		x += stepX;
		u += stepU;
		v += stepV;
	}
}
} // namespace

void ChromaTriangle(const std::array<glm::vec4, 3>& vertices, const AlphaMap& map, std::span<uint16_t> target, int side)
{
	if (side <= 0 || target.size() < static_cast<size_t>(side) * static_cast<size_t>(side))
	{
		return; // (port guard)
	}
	std::array<ChromaPoint, 3> points {};
	for (size_t i = 0; i < 3; ++i)
	{
		// x, z, u, v truncated toward zero; x and y clamped to [0, width - 1] / [0, height - 1]
		points[i] = {TruncateToInt(vertices[i].x), TruncateToInt(vertices[i].y), TruncateToInt(vertices[i].z),
		             TruncateToInt(vertices[i].w)};
		points[i].x = std::clamp(points[i].x, 0, side - 1);
		points[i].y = std::clamp(points[i].y, 0, side - 1);
	}
	// a becomes the upper vertex
	auto* a = &points[0];
	auto* b = &points[1];
	auto* c = &points[2];
	const bool aFirst = a->y < b->y || (a->y == b->y && a->x >= b->x);
	if (aFirst)
	{
		// c above a (or level and to the right) -> swap a, c
		if (!(a->y < c->y || (a->y == c->y && a->x >= c->x)))
		{
			std::swap(a, c);
		}
	}
	else
	{
		// b below c (or level and to the left) -> swap a, c; else swap a, b
		if (b->y > c->y || (b->y == c->y && b->x < c->x))
		{
			std::swap(a, c);
		}
		else
		{
			std::swap(a, b);
		}
	}
	// Then b and c
	if (b->x > c->x)
	{
		std::swap(b, c);
	}
	else if (b->x == c->x)
	{
		if (a->x <= b->x ? b->y < c->y : b->y > c->y)
		{
			std::swap(b, c);
		}
	}
	const int minRow = a->y;
	const int maxRow = std::max(b->y, c->y);
	// The two edge tables, kept and not cleared as the original's: every row of minRow..maxRow is written by the edges
	// first
	thread_local std::vector<ChromaEdge> down;
	thread_local std::vector<ChromaEdge> up;
	down.resize(static_cast<size_t>(side));
	up.resize(static_cast<size_t>(side));
	ChromaEdgeOf(*a, *b, down, up);
	ChromaEdgeOf(*b, *c, down, up);
	ChromaEdgeOf(*c, *a, down, up);
	// Rows minRow..maxRow inclusive, each span from `down` to `up` (or the other way), inclusive
	for (int row = minRow; row <= maxRow; ++row)
	{
		const auto& left = down[static_cast<size_t>(row)];
		const auto& right = up[static_cast<size_t>(row)];
		int count = right.x - left.x + 1;
		int x = left.x;
		int u = left.u;
		int v = left.v;
		int stepU = 0;
		int stepV = 0;
		if (count < 0)
		{
			count = left.x - right.x + 1;
			x = right.x;
			u = right.u;
			v = right.v;
			stepU = (left.u - right.u) / count; // integer division
			stepV = (left.v - right.v) / count;
		}
		else if (count != 0)
		{
			stepU = (right.u - left.u) / count;
			stepV = (right.v - left.v) / count;
		}
		auto* texel = target.data() + static_cast<size_t>(row) * static_cast<size_t>(side);
		for (int i = 0; i < count; ++i)
		{
			const int index = ((v >> 10) & ~0x3F) + (u >> 16); // (v >> 16) 64 + (u >> 16)
			const int px = x + i;
			if (index >= 0 && index < static_cast<int>(map.size()) && px >= 0 && px < side) // (port guard)
			{
				texel[px] = static_cast<uint16_t>(texel[px] | ((map[static_cast<size_t>(index)] & 0xE0) << 7));
			}
			u += stepU;
			v += stepV;
		}
	}
}

float LandProjectionFactor(float baseY, float lightY, float ground)
{
	return (baseY - lightY) / (ground - lightY);
}

bool TouchesBlock(const Box& box, int blockX, int blockZ)
{
	return static_cast<float>(blockX) * k_BlockSize <= box.x1 && static_cast<float>(blockX + 1) * k_BlockSize >= box.x0 &&
	       static_cast<float>(blockZ) * k_BlockSize <= box.z1 && static_cast<float>(blockZ + 1) * k_BlockSize >= box.z0;
}

bool ReachesMorphable(const Box& box, glm::vec3 meshCentre, const glm::mat4& model, float scale, float halfDiagonal)
{
	const float radius = scale * halfDiagonal;
	const float width = box.x1 - box.x0;
	const float depth = box.z1 - box.z0;
	// Width < depth, equal or unordered takes depth
	const float side = width > depth ? width : depth;
	const float reach = radius + side * k_MorphableBoxFactor;
	const glm::vec3 c = meshCentre;
	const float x = ((c.y * model[1].x + c.z * model[2].x) + c.x * model[0].x) + model[3].x;
	const float z = ((c.y * model[1].z + c.x * model[0].z) + c.z * model[2].z) + model[3].z;
	const float boxX = (box.x0 + box.x1) * 0.5f;
	const float boxZ = (box.z0 + box.z1) * 0.5f;
	const float dz = z - boxZ;
	const float dx = x - boxX;
	const float distance = dx * dx + dz * dz;
	return reach * reach > distance;
}

bool BlockVisible(const std::array<glm::vec3, 8>& corners, const affine::AffineMatrix& worldToClipping, float nearW)
{
	uint32_t nearCodes = 0;
	uint32_t right = 0;
	uint32_t left = 0;
	uint32_t top = 0;
	uint32_t bottom = 0;
	for (uint32_t i = 0; i < corners.size(); ++i)
	{
		const uint32_t bit = 1u << i;
		// X, Y and Z, the depth
		const auto clip = affine::ToClipForShadowBlocks(worldToClipping, corners[i]);
		if (clip.z < nearW)
		{
			nearCodes |= bit;
		}
		if (clip.x > clip.z)
		{
			right |= bit;
		}
		else if (-clip.z > clip.x)
		{
			left |= bit;
		}
		if (clip.y > clip.z)
		{
			top |= bit;
		}
		else if (-clip.z > clip.y)
		{
			bottom |= bit;
		}
	}
	return nearCodes != 0xFF && right != 0xFF && left != 0xFF && top != 0xFF && bottom != 0xFF; // one outcode for all 8
}

} // namespace openblack::graphics::shadow_math
