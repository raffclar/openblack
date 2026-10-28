/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FaceSearch.h"

#include <bit>

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
		const auto& block = blocks[b];
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
	return found;
}

std::optional<Hit> openblack::physics::face_search::NearestBehind(std::span<const FaceBlock> blocks, glm::vec3 point,
                                                                  glm::vec3 direction)
{
#if defined(OPENBLACK_FACE_SEARCH_SSE)
	std::optional<Hit> found;
	float best = -1.0f;
	const __m128 dx = _mm_set1_ps(direction.x);
	const __m128 dy = _mm_set1_ps(direction.y);
	const __m128 dz = _mm_set1_ps(direction.z);
	const __m128 px = _mm_set1_ps(point.x);
	const __m128 py = _mm_set1_ps(point.y);
	const __m128 pz = _mm_set1_ps(point.z);
	const __m128 threshold = _mm_set1_ps(k_FacingThreshold);
	const __m128 zero = _mm_setzero_ps();
	const __m128 sign = _mm_set1_ps(-0.0f);
	for (std::size_t b = 0; b < blocks.size(); ++b)
	{
		const auto& block = blocks[b];
		const __m128 nx = _mm_load_ps(block.nx.data());
		const __m128 ny = _mm_load_ps(block.ny.data());
		const __m128 nz = _mm_load_ps(block.nz.data());
		const __m128 facing = _mm_add_ps(_mm_add_ps(_mm_mul_ps(nx, dx), _mm_mul_ps(nz, dz)), _mm_mul_ps(ny, dy));
		__m128 mask = _mm_cmplt_ps(facing, threshold);
		if (_mm_movemask_ps(mask) == 0)
		{
			continue;
		}
		const __m128 ax = _mm_load_ps(block.ax.data());
		const __m128 ay = _mm_load_ps(block.ay.data());
		const __m128 az = _mm_load_ps(block.az.data());
		const __m128 ex = _mm_sub_ps(px, ax);
		const __m128 ey = _mm_sub_ps(py, ay);
		const __m128 ez = _mm_sub_ps(pz, az);
		const __m128 along = _mm_add_ps(_mm_add_ps(_mm_mul_ps(ez, nz), _mm_mul_ps(ey, ny)), _mm_mul_ps(ex, nx));
		const __m128 t = _mm_div_ps(_mm_xor_ps(along, sign), facing);
		// The largest distance only increases. Thus a face behind it now does not win later in this block.
		mask = _mm_and_ps(mask, _mm_and_ps(_mm_cmplt_ps(t, zero), _mm_cmplt_ps(_mm_set1_ps(best), t)));
		if (_mm_movemask_ps(mask) == 0)
		{
			continue;
		}
		const __m128 hx = _mm_add_ps(px, _mm_mul_ps(t, dx));
		const __m128 hy = _mm_add_ps(py, _mm_mul_ps(t, dy));
		const __m128 hz = _mm_add_ps(pz, _mm_mul_ps(t, dz));
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
			continue;
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
	return found;
#else
	return NearestBehindOneByOne(blocks, point, direction);
#endif
}
