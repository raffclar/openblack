/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FaceSearch.h"

#include <cmath>

#include <algorithm>
#include <bit>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#define OPENBLACK_FACE_SEARCH_SSE
#if defined(_MSC_VER)
#define OPENBLACK_FACE_SEARCH_INLINE __forceinline
#else
#define OPENBLACK_FACE_SEARCH_INLINE inline __attribute__((always_inline))
#endif
#endif

using namespace openblack::physics::face_search;

namespace
{

constexpr float k_Infinity = std::numeric_limits<float>::infinity();
/// The box of a face that no line can hit
[[nodiscard]] Box NoReach()
{
	return {.min = glm::vec3(k_Infinity), .max = glm::vec3(-k_Infinity)};
}

/// The box of a face that the search cannot bound
[[nodiscard]] Box AllReach()
{
	return {.min = glm::vec3(-k_Infinity), .max = glm::vec3(k_Infinity)};
}

// The limits below keep all float operations of the search away from overflow and underflow. Thus each operation has a
// relative error of at most half a unit in the last place, and the bounds of the errors below are true.

/// The largest part of a corner, a point or a direction that the boxes accept
constexpr float k_LargestPart = 0x1p40f;
/// The smallest size of a face that gets a finite box
constexpr float k_SmallestSize = 0x1p-30f;
/// The smallest and the largest part of a normal that a face with a finite box can have
constexpr float k_SmallestNormal = 0x1p-10f;
constexpr float k_LargestNormal = 0x1p10f;
/// Four times the largest error of the edge test of the search, for unit sizes of the edge, the offset and the normal
constexpr float k_EdgeError = 0x1p-18f;
/// Four times the largest error of the dot product of a normal and an edge, for unit sizes
constexpr float k_DotError = 0x1p-20f;
/// More than the largest distance between a hit and the plane of its face, for a unit size of the parts of the
/// corners, the point and the direction
constexpr float k_PlaneError = 0x1p-18f;
/// More than the largest error of a hit and of the far end of the line, for a unit size of the point and the direction
constexpr float k_LineError = 0x1p-17f;
/// A small distance that the box of each face gets in addition
constexpr float k_SmallestMargin = 1e-20f;

[[nodiscard]] float LargestPart(glm::vec3 v)
{
	return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

[[nodiscard]] bool IsFinite(glm::vec3 v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/// The box that holds the line from the point back to one direction length behind it, with all hits that the search
/// finds on it. There is no box when the parts are too large.
[[nodiscard]] std::optional<Box> LineBox(glm::vec3 point, glm::vec3 direction)
{
	const float size = LargestPart(point) + LargestPart(direction);
	if (!(size <= k_LargestPart))
	{
		return std::nullopt;
	}
	const float margin = size * k_LineError;
	const glm::vec3 end = point - direction;
	return Box {.min = glm::min(point, end) - margin, .max = glm::max(point, end) + margin};
}

/// The edge test of one side of a triangle: the cross product of the edge and the point's offset from the edge's start,
/// on the side of the normal. The game adds the parts of the dot product from z to x.
[[nodiscard]] bool OnInnerSide(float sx, float sy, float sz, float ex, float ey, float ez, float px, float py, float pz,
                               float nx, float ny, float nz)
{
	// The edge, then the point's offset from its start
	const float dx = ex - sx;
	const float dy = ey - sy;
	const float dz = ez - sz;
	const float ox = px - sx;
	const float oy = py - sy;
	const float oz = pz - sz;
	const float cx = dy * oz - oy * dz;
	const float cy = dz * ox - oz * dx;
	const float cz = dx * oy - ox * dy;
	return (cz * nz + cy * ny) + cx * nx > 0.0f;
}

/// Tests the four faces of a block one at a time. A face that the line meets nearer to the point than the best hit
/// becomes the best hit.
void SearchBlockOneByOne(const FaceBlock& block, std::size_t b, glm::vec3 point, glm::vec3 direction, float& best,
                         std::optional<Hit>& found)
{
	for (std::size_t lane = 0; lane < k_Lanes; ++lane)
	{
		const float nx = block.nx[lane];
		const float ny = block.ny[lane];
		const float nz = block.nz[lane];
		// The facing adds x, z, then y
		const float facing = (nx * direction.x + nz * direction.z) + ny * direction.y;
		if (!(facing < k_FacingThreshold))
		{
			continue;
		}
		// The distance along the line adds z, y, then x
		const float ex = point.x - block.ax[lane];
		const float ey = point.y - block.ay[lane];
		const float ez = point.z - block.az[lane];
		const float t = -((ez * nz + ey * ny) + ex * nx) / facing;
		if (!(t < 0.0f) || !(best < t))
		{
			continue;
		}
		const glm::vec3 hit(point.x + t * direction.x, point.y + t * direction.y, point.z + t * direction.z);
		const float ax = block.ax[lane];
		const float ay = block.ay[lane];
		const float az = block.az[lane];
		const float bx = block.bx[lane];
		const float by = block.by[lane];
		const float bz = block.bz[lane];
		const float cx = block.cx[lane];
		const float cy = block.cy[lane];
		const float cz = block.cz[lane];
		if (!OnInnerSide(ax, ay, az, bx, by, bz, hit.x, hit.y, hit.z, nx, ny, nz) ||
		    !OnInnerSide(bx, by, bz, cx, cy, cz, hit.x, hit.y, hit.z, nx, ny, nz) ||
		    !OnInnerSide(cx, cy, cz, ax, ay, az, hit.x, hit.y, hit.z, nx, ny, nz))
		{
			continue;
		}
		best = t;
		found = Hit {.point = hit, .face = b * k_Lanes + lane};
	}
}

#if !defined(OPENBLACK_FACE_SEARCH_SSE)
/// Whether a box touches the box of a face block
[[nodiscard]] bool Touches(const BoundsBlock& bounds, std::size_t lane, const Box& box)
{
	return bounds.minX[lane] <= box.max.x && box.min.x <= bounds.maxX[lane] && bounds.minY[lane] <= box.max.y &&
	       box.min.y <= bounds.maxY[lane] && bounds.minZ[lane] <= box.max.z && box.min.z <= bounds.maxZ[lane];
}
#endif

#if defined(OPENBLACK_FACE_SEARCH_SSE)
/// The same edge test as OnInnerSide, for four faces at a time
[[nodiscard]] OPENBLACK_FACE_SEARCH_INLINE __m128 OnInnerSide4(__m128 sx, __m128 sy, __m128 sz, __m128 ex, __m128 ey, __m128 ez,
                                                               __m128 px, __m128 py, __m128 pz, __m128 nx, __m128 ny, __m128 nz)
{
	const __m128 dx = _mm_sub_ps(ex, sx);
	const __m128 dy = _mm_sub_ps(ey, sy);
	const __m128 dz = _mm_sub_ps(ez, sz);
	const __m128 ox = _mm_sub_ps(px, sx);
	const __m128 oy = _mm_sub_ps(py, sy);
	const __m128 oz = _mm_sub_ps(pz, sz);
	const __m128 cx = _mm_sub_ps(_mm_mul_ps(dy, oz), _mm_mul_ps(oy, dz));
	const __m128 cy = _mm_sub_ps(_mm_mul_ps(dz, ox), _mm_mul_ps(oz, dx));
	const __m128 cz = _mm_sub_ps(_mm_mul_ps(dx, oy), _mm_mul_ps(ox, dy));
	const __m128 side = _mm_add_ps(_mm_add_ps(_mm_mul_ps(cz, nz), _mm_mul_ps(cy, ny)), _mm_mul_ps(cx, nx));
	return _mm_cmpgt_ps(side, _mm_setzero_ps());
}

/// The point and the direction of one search in all four lanes
struct Line4
{
	__m128 px;
	__m128 py;
	__m128 pz;
	__m128 dx;
	__m128 dy;
	__m128 dz;
};

[[nodiscard]] OPENBLACK_FACE_SEARCH_INLINE Line4 MakeLine4(glm::vec3 point, glm::vec3 direction)
{
	return {.px = _mm_set1_ps(point.x),
	        .py = _mm_set1_ps(point.y),
	        .pz = _mm_set1_ps(point.z),
	        .dx = _mm_set1_ps(direction.x),
	        .dy = _mm_set1_ps(direction.y),
	        .dz = _mm_set1_ps(direction.z)};
}

/// Tests the four faces of a block at a time. The faces that the line meets nearer to the point than the best hit
/// become the best hit in their order.
OPENBLACK_FACE_SEARCH_INLINE void SearchBlock4(const FaceBlock& block, std::size_t b, const Line4& line, float& best,
                                               std::optional<Hit>& found)
{
	const __m128 nx = _mm_load_ps(block.nx.data());
	const __m128 ny = _mm_load_ps(block.ny.data());
	const __m128 nz = _mm_load_ps(block.nz.data());
	const __m128 facing = _mm_add_ps(_mm_add_ps(_mm_mul_ps(nx, line.dx), _mm_mul_ps(nz, line.dz)), _mm_mul_ps(ny, line.dy));
	__m128 mask = _mm_cmplt_ps(facing, _mm_set1_ps(k_FacingThreshold));
	if (_mm_movemask_ps(mask) == 0)
	{
		return;
	}
	const __m128 ax = _mm_load_ps(block.ax.data());
	const __m128 ay = _mm_load_ps(block.ay.data());
	const __m128 az = _mm_load_ps(block.az.data());
	const __m128 ex = _mm_sub_ps(line.px, ax);
	const __m128 ey = _mm_sub_ps(line.py, ay);
	const __m128 ez = _mm_sub_ps(line.pz, az);
	const __m128 along = _mm_add_ps(_mm_add_ps(_mm_mul_ps(ez, nz), _mm_mul_ps(ey, ny)), _mm_mul_ps(ex, nx));
	const __m128 t = _mm_div_ps(_mm_xor_ps(along, _mm_set1_ps(-0.0f)), facing);
	// The largest distance only increases. Thus a face behind it now does not win later in this block.
	mask = _mm_and_ps(mask, _mm_and_ps(_mm_cmplt_ps(t, _mm_setzero_ps()), _mm_cmplt_ps(_mm_set1_ps(best), t)));
	if (_mm_movemask_ps(mask) == 0)
	{
		return;
	}
	const __m128 hx = _mm_add_ps(line.px, _mm_mul_ps(t, line.dx));
	const __m128 hy = _mm_add_ps(line.py, _mm_mul_ps(t, line.dy));
	const __m128 hz = _mm_add_ps(line.pz, _mm_mul_ps(t, line.dz));
	const __m128 bx = _mm_load_ps(block.bx.data());
	const __m128 by = _mm_load_ps(block.by.data());
	const __m128 bz = _mm_load_ps(block.bz.data());
	const __m128 cx = _mm_load_ps(block.cx.data());
	const __m128 cy = _mm_load_ps(block.cy.data());
	const __m128 cz = _mm_load_ps(block.cz.data());
	mask = _mm_and_ps(mask, OnInnerSide4(ax, ay, az, bx, by, bz, hx, hy, hz, nx, ny, nz));
	mask = _mm_and_ps(mask, OnInnerSide4(bx, by, bz, cx, cy, cz, hx, hy, hz, nx, ny, nz));
	mask = _mm_and_ps(mask, OnInnerSide4(cx, cy, cz, ax, ay, az, hx, hy, hz, nx, ny, nz));
	auto lanes = static_cast<unsigned>(_mm_movemask_ps(mask));
	if (lanes == 0)
	{
		return;
	}
	alignas(16) std::array<float, k_Lanes> ts {};
	alignas(16) std::array<float, k_Lanes> xs {};
	alignas(16) std::array<float, k_Lanes> ys {};
	alignas(16) std::array<float, k_Lanes> zs {};
	_mm_store_ps(ts.data(), t);
	_mm_store_ps(xs.data(), hx);
	_mm_store_ps(ys.data(), hy);
	_mm_store_ps(zs.data(), hz);
	// Take the faces of the block in their order, as the search one face at a time does
	while (lanes != 0)
	{
		const auto lane = static_cast<std::size_t>(std::countr_zero(lanes));
		lanes &= lanes - 1;
		if (best < ts.at(lane))
		{
			best = ts.at(lane);
			found = Hit {.point = {xs.at(lane), ys.at(lane), zs.at(lane)}, .face = b * k_Lanes + lane};
		}
	}
}
#endif

} // namespace

void openblack::physics::face_search::Store(std::span<FaceBlock> blocks, std::size_t face, const WorldFace& world)
{
	auto& block = blocks[face / k_Lanes];
	const auto lane = face % k_Lanes;
	block.nx.at(lane) = world.normal.x;
	block.ny.at(lane) = world.normal.y;
	block.nz.at(lane) = world.normal.z;
	block.ax.at(lane) = world.a.x;
	block.ay.at(lane) = world.a.y;
	block.az.at(lane) = world.a.z;
	block.bx.at(lane) = world.b.x;
	block.by.at(lane) = world.b.y;
	block.bz.at(lane) = world.b.z;
	block.cx.at(lane) = world.c.x;
	block.cy.at(lane) = world.c.y;
	block.cz.at(lane) = world.c.z;
}

std::optional<Hit> openblack::physics::face_search::NearestBehindOneByOne(std::span<const FaceBlock> blocks, glm::vec3 point,
                                                                          glm::vec3 direction)
{
	std::optional<Hit> found;
	// The search keeps the largest distance along the line, which starts one direction length behind the point
	float best = -1.0f;
	for (std::size_t b = 0; b < blocks.size(); ++b)
	{
		SearchBlockOneByOne(blocks[b], b, point, direction, best, found);
	}
	return found;
}

std::optional<Hit> openblack::physics::face_search::NearestBehind(std::span<const FaceBlock> blocks, glm::vec3 point,
                                                                  glm::vec3 direction)
{
#if defined(OPENBLACK_FACE_SEARCH_SSE)
	std::optional<Hit> found;
	float best = -1.0f;
	const auto line = MakeLine4(point, direction);
	for (std::size_t b = 0; b < blocks.size(); ++b)
	{
		SearchBlock4(blocks[b], b, line, best, found);
	}
	return found;
#else
	return NearestBehindOneByOne(blocks, point, direction);
#endif
}

Box openblack::physics::face_search::ReachOf(const WorldFace& face)
{
	// The three edge tests of a hit add to one value for the face. When each test passes, the share of each test is
	// more than minus its error divided by that value. Thus the hit is a mix of the corners with weights that are almost
	// positive, and it stays near the box of the corners.
	const auto& n = face.normal;
	// A normal with a part that is not a number, or a zero normal, never faces a line
	if (std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z) || (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f))
	{
		return NoReach();
	}
	if (!IsFinite(n) || !IsFinite(face.a) || !IsFinite(face.b) || !IsFinite(face.c))
	{
		return AllReach();
	}
	const glm::vec3 low = glm::min(glm::min(face.a, face.b), face.c);
	const glm::vec3 high = glm::max(glm::max(face.a, face.b), face.c);
	const float size = LargestPart(high - low);
	const float largest = std::max(LargestPart(low), LargestPart(high));
	const float normalLargest = LargestPart(n);
	if (!(largest <= k_LargestPart) || !(size >= k_SmallestSize) || !(normalLargest >= k_SmallestNormal) ||
	    !(normalLargest <= k_LargestNormal))
	{
		return AllReach();
	}
	const float normalSum = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
	const glm::vec3 ab = face.b - face.a;
	const glm::vec3 ac = face.c - face.a;
	// The three edge tests add to this value. A hit passes all three only if each test is above its error.
	const glm::vec3 across = glm::cross(ab, ac);
	const float area = (across.x * n.x + across.y * n.y) + across.z * n.z;
	const float sizeSquared = size * size * normalSum;
	const float lowArea = area - k_EdgeError * sizeSquared;
	if (!(lowArea > 0.0f))
	{
		return AllReach();
	}
	// How much a passing hit can go out of the triangle, for each unit of its distance from the corners
	const float spill = k_EdgeError * size * normalSum / lowArea;
	// How far a corner can be from the plane through the first corner with the normal
	const float bOff = std::abs((ab.x * n.x + ab.y * n.y) + ab.z * n.z);
	const float cOff = std::abs((ac.x * n.x + ac.y * n.y) + ac.z * n.z);
	const float offPlane = (std::max(bOff, cOff) + k_DotError * size * normalSum) / normalLargest;
	if (!(2.0f * spill * size + 4.0f * spill * offPlane <= 0.25f))
	{
		return AllReach();
	}
	const float margin = 3.0f * spill * size * size + 3.0f * offPlane + k_PlaneError * largest + k_SmallestMargin;
	return {.min = low - margin, .max = high + margin};
}

void FaceSet::Reset(std::size_t faces)
{
	_blocks.resize(BlocksFor(faces));
	// An empty place in the last block keeps a zero normal, so it never faces a line
	if (!_blocks.empty())
	{
		_blocks.back() = {};
	}
	_bounds.resize((_blocks.size() + k_BlocksPerBounds - 1) / k_BlocksPerBounds);
	for (auto& bounds : _bounds)
	{
		bounds.minX.fill(k_Infinity);
		bounds.minY.fill(k_Infinity);
		bounds.minZ.fill(k_Infinity);
		bounds.maxX.fill(-k_Infinity);
		bounds.maxY.fill(-k_Infinity);
		bounds.maxZ.fill(-k_Infinity);
	}
}

void FaceSet::Store(std::size_t face, const WorldFace& world)
{
	openblack::physics::face_search::Store(_blocks, face, world);
	const auto reach = ReachOf(world);
	const auto block = face / k_Lanes;
	auto& bounds = _bounds[block / k_BlocksPerBounds];
	const auto lane = block % k_BlocksPerBounds;
	bounds.minX.at(lane) = std::min(bounds.minX.at(lane), reach.min.x);
	bounds.minY.at(lane) = std::min(bounds.minY.at(lane), reach.min.y);
	bounds.minZ.at(lane) = std::min(bounds.minZ.at(lane), reach.min.z);
	bounds.maxX.at(lane) = std::max(bounds.maxX.at(lane), reach.max.x);
	bounds.maxY.at(lane) = std::max(bounds.maxY.at(lane), reach.max.y);
	bounds.maxZ.at(lane) = std::max(bounds.maxZ.at(lane), reach.max.z);
}

std::optional<Hit> openblack::physics::face_search::NearestBehind(const FaceSet& faces, glm::vec3 point, glm::vec3 direction)
{
	const auto blocks = faces.Blocks();
	const auto box = LineBox(point, direction);
	if (!box.has_value())
	{
		return NearestBehind(blocks, point, direction);
	}
	std::optional<Hit> found;
	float best = -1.0f;
	const auto bounds = faces.Bounds();
#if defined(OPENBLACK_FACE_SEARCH_SSE)
	const auto line = MakeLine4(point, direction);
	const __m128 lowX = _mm_set1_ps(box->min.x);
	const __m128 lowY = _mm_set1_ps(box->min.y);
	const __m128 lowZ = _mm_set1_ps(box->min.z);
	const __m128 highX = _mm_set1_ps(box->max.x);
	const __m128 highY = _mm_set1_ps(box->max.y);
	const __m128 highZ = _mm_set1_ps(box->max.z);
	for (std::size_t g = 0; g < bounds.size(); ++g)
	{
		const auto& group = bounds[g];
		__m128 touch =
		    _mm_and_ps(_mm_cmple_ps(_mm_load_ps(group.minX.data()), highX), _mm_cmple_ps(lowX, _mm_load_ps(group.maxX.data())));
		touch = _mm_and_ps(touch, _mm_cmple_ps(_mm_load_ps(group.minY.data()), highY));
		touch = _mm_and_ps(touch, _mm_cmple_ps(lowY, _mm_load_ps(group.maxY.data())));
		touch = _mm_and_ps(touch, _mm_cmple_ps(_mm_load_ps(group.minZ.data()), highZ));
		touch = _mm_and_ps(touch, _mm_cmple_ps(lowZ, _mm_load_ps(group.maxZ.data())));
		// Test the blocks that the line touches in their order
		auto lanes = static_cast<unsigned>(_mm_movemask_ps(touch));
		while (lanes != 0)
		{
			const auto b = g * k_BlocksPerBounds + static_cast<std::size_t>(std::countr_zero(lanes));
			lanes &= lanes - 1;
			SearchBlock4(blocks[b], b, line, best, found);
		}
	}
#else
	for (std::size_t b = 0; b < blocks.size(); ++b)
	{
		if (Touches(bounds[b / k_BlocksPerBounds], b % k_BlocksPerBounds, *box))
		{
			SearchBlockOneByOne(blocks[b], b, point, direction, best, found);
		}
	}
#endif
	return found;
}
