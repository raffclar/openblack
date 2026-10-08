/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::shadow_math (src/Graphics/ShadowMath.h) against the original behaviour it ports: the fade, the alphas, the
// lights, the projection, the grid, the rasterizer, the resolve, the chroma blur, the baked fade and the land tests

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "EngineConfig.h"
#include "Graphics/ShadowMath.h"
#include "Locator.h"
#include "support/BgfxShutdown.h"
#include "support/RestoreService.h"

using namespace openblack::graphics::shadow_math;

namespace
{
BlockState Near()
{
	return {true, true, 0.0f};
}

/// The fade with every block there, visible and near, the camera at q radii along x at the ground's height
float FadeAt(float q)
{
	const glm::vec3 position(1000.0f, 30.0f, 1000.0f);
	return Fade(position, 10.0f, glm::vec3(1000.0f + q, 10.0f, 1000.0f), 1.0f, 1.0f, [](int, int) { return Near(); });
}

Texels Full(uint8_t n, int side = k_Texels)
{
	Texels texels(static_cast<size_t>(side * side), 0);
	for (int r = 1; r < side - 1; ++r)
	{
		for (int c = 1; c < side - 1; ++c)
		{
			texels[static_cast<size_t>(r * side + c)] = n;
		}
	}
	return texels;
}

uint8_t ByteAt(const Coverage& coverage, int row, int column)
{
	return coverage.bytes[static_cast<size_t>(row * coverage.texels + column)];
}
} // namespace

TEST(ShadowMath, FadeByDistance)
{
	EXPECT_EQ(FadeAt(49.9f), 255.0f);
	EXPECT_EQ(FadeAt(50.0f), 255.0f); // not < 50, but 255 - 0
	EXPECT_EQ(FadeAt(65.0f), 127.5f);
	EXPECT_EQ(FadeAt(80.0f), 0.0f);
	EXPECT_EQ(FadeAt(80.01f), 0.0f);
	// the scale and the mesh radius divide the distance
	EXPECT_EQ(Fade({1000.0f, 0.0f, 1000.0f}, 0.0f, {1130.0f, 0.0f, 1000.0f}, 2.0f, 1.0f, [](int, int) { return Near(); }),
	          127.5f);
}

TEST(ShadowMath, FadeBlocks)
{
	const glm::vec3 position(955.0f, 0.0f, 1765.0f); // cells (95, 176): block (5, 11)
	const glm::vec3 camera(955.0f, 0.0f, 1766.0f);
	// the block under the caster at 100000 -> 0, even with visible neighbours
	EXPECT_EQ(Fade(position, 0.0f, camera, 1.0f, 1.0f,
	               [](int x, int z) {
		               return x == 5 && z == 11 ? BlockState {true, true, 100000.0f} : Near();
	               }),
	          0.0f);
	// near but nothing visible around -> 0
	EXPECT_EQ(Fade(position, 0.0f, camera, 1.0f, 1.0f, [](int, int) { return BlockState {true, false, 99999.0f}; }), 0.0f);
	// only the neighbour at (+60, -60) visible: cells (101, 170) = block (6, 10)
	EXPECT_EQ(Fade(position, 0.0f, camera, 1.0f, 1.0f,
	               [](int x, int z) {
		               return x == 6 && z == 10 ? Near() : BlockState {true, false, 10.0f};
	               }),
	          255.0f);
	// a missing block does not count, even "visible"
	EXPECT_EQ(Fade(position, 0.0f, camera, 1.0f, 1.0f, [](int, int) { return BlockState {false, true, 0.0f}; }), 0.0f);
	// a cell outside 0..0x1FF skips the first test: x = -20 is cell -2, its +60 neighbour cell 4 = block 0
	EXPECT_EQ(Fade({-20.0f, 0.0f, 1765.0f}, 0.0f, {-20.0f, 0.0f, 1766.0f}, 1.0f, 1.0f,
	               [](int x, int) { return x == 0 ? Near() : BlockState {}; }),
	          255.0f);
	// ... and only the cells in range count: past 0x1FF nothing is found
	EXPECT_EQ(Fade({5200.0f, 0.0f, 1765.0f}, 0.0f, {5200.0f, 0.0f, 1766.0f}, 1.0f, 1.0f, [](int, int) { return Near(); }),
	          0.0f);
}

TEST(ShadowMath, Alphas)
{
	EXPECT_EQ(AlphaGeneric(200.0f, 255), 200);
	EXPECT_EQ(AlphaComplex(200.0f, 255), 200);
	EXPECT_EQ(AlphaGeneric(254.9f, 255), 254);
	EXPECT_EQ(AlphaComplex(254.9f, 255), 254);
	EXPECT_EQ(AlphaGeneric(255.0f, 128), 128);
	EXPECT_EQ(AlphaComplex(255.0f, 128), 255); // a full fade ignores the base
	EXPECT_EQ(AlphaGeneric(0.5f, 255), 0);
	EXPECT_EQ(AlphaComplex(0.5f, 255), 0);
}

TEST(ShadowMath, Lights)
{
	EXPECT_EQ(LightGeneric({10.0f, 20.0f, 30.0f}, true), glm::vec3(-500000.0f, 500000.0f, -500000.0f));
	EXPECT_EQ(LightGeneric({10.0f, 20.0f, 30.0f}, false), glm::vec3(10.0f, 15020.0f, 30.0f));
	EXPECT_EQ(LightHand({10.0f, 20.0f, 30.0f}), glm::vec3(10.0f, 220.0f, 30.0f));

	// the creature: R = 3 x radius x scale = 6
	const glm::vec3 body(100.0f, 0.0f, 100.0f);
	// far and high enough: kept
	EXPECT_EQ(LightCreature(body, {160.0f, 200.0f, 100.0f}, 1.0f, 2.0f), glm::vec3(160.0f, 200.0f, 100.0f));
	// far but low: dy raised to the horizontal distance (45 degrees)
	EXPECT_EQ(LightCreature(body, {160.0f, 10.0f, 100.0f}, 1.0f, 2.0f), glm::vec3(160.0f, 60.0f, 100.0f));
	// near: d (3D) scaled to R = 6, then dy = 3.6 against the horizontal 4.8 -> raised to 4.8
	const auto scaled = LightCreature(body, {104.0f, 3.0f, 100.0f}, 1.0f, 2.0f);
	EXPECT_NEAR(scaled.x, 104.8f, 1e-4f);
	EXPECT_NEAR(scaled.y, 4.8f, 1e-4f);
	EXPECT_NEAR(scaled.z, 100.0f, 1e-4f);
	// straight above (horizontal < 0.1): dx and dz + 1, scaled to R, then up to 45 degrees at least
	const auto above = LightCreature(body, {100.0f, 50.0f, 100.0f}, 1.0f, 2.0f);
	const float length = std::sqrt(50.0f * 50.0f + 2.0f);
	EXPECT_NEAR(above.x - 100.0f, 6.0f / length, 1e-5f);
	EXPECT_NEAR(above.z - 100.0f, 6.0f / length, 1e-5f);
	EXPECT_NEAR(above.y, 50.0f * 6.0f / length, 1e-4f);
}

TEST(ShadowMath, ProjectAbsoluteLightY)
{
	// the hand at y = 50 with its light at 250: t = 250 / (250 - h), not the plane's intersection
	const glm::vec3 hand(100.0f, 50.0f, 200.0f);
	const auto projection = MakeProjection(hand, LightHand(hand));
	EXPECT_EQ(projection.dir, glm::vec3(0.0f, -200.0f, 0.0f));
	const auto matrix = glm::translate(glm::mat4(1.0f), hand);
	Box box;
	const auto p = Project(projection, matrix, {2.0f, 10.0f, 3.0f}, box);
	const float t = -250.0f / (10.0f - 250.0f);
	EXPECT_FLOAT_EQ(p.x, (102.0f - 100.0f) * t + 100.0f);
	EXPECT_FLOAT_EQ(p.y, (203.0f - 200.0f) * t + 200.0f);
	EXPECT_EQ(box.x0, p.x);
	EXPECT_EQ(box.x1, p.x);
	EXPECT_EQ(box.kMin, 0.0f); // vertical light: d.x = d.z = 0
	// under the base: h = 0, t = 1
	const auto under = Project(projection, matrix, {-2.0f, -5.0f, 1.0f}, box);
	EXPECT_EQ(under, glm::vec2(98.0f, 201.0f));
	EXPECT_EQ(box.x0, 98.0f);
	EXPECT_EQ(box.z0, 201.0f);
	EXPECT_EQ(box.z1, p.y);

	// the boat with the sun: shifted by about h along +x and +z (Ly = 500000)
	const glm::vec3 boat(1900.0f, 0.0f, 3100.0f);
	const auto sun = MakeProjection(boat, LightGeneric(boat, true));
	Box sunBox;
	const auto shifted = Project(sun, glm::translate(glm::mat4(1.0f), boat), {0.0f, 10.0f, 0.0f}, sunBox);
	EXPECT_NEAR(shifted.x, 1900.0f + 10.0f * (1900.0f + 500000.0f) / 500000.0f, 0.1f);
	EXPECT_NEAR(shifted.y, 3100.0f + 10.0f * (3100.0f + 500000.0f) / 500000.0f, 0.1f);
	EXPECT_GT(sunBox.kMin, 0.0f); // k = W.z d.z + W.x d.x, both d components positive away from the sun
}

TEST(ShadowMath, ToGrid)
{
	Box box;
	box.x0 = 10.0f;
	box.x1 = 20.0f;
	box.z0 = 0.0f;
	box.z1 = 5.0f;
	std::array<glm::vec2, 4> points {glm::vec2(20.0f, 5.0f), glm::vec2(5.0f, -1.0f), glm::vec2(15.0f, 2.5f),
	                                 glm::vec2(10.0f, 0.0f)};
	ToGrid(box, points);
	EXPECT_EQ(points[0], glm::vec2(127.0f, 63.0f));
	EXPECT_EQ(points[1], glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(points[2], glm::vec2(64.0f, 32.0f));
	EXPECT_EQ(points[3], glm::vec2(0.0f, 0.0f));
}

TEST(ShadowMath, RasterQuad)
{
	// the whole grid as two front-facing triangles
	const std::array<glm::vec2, 4> grid {glm::vec2(0.0f, 0.0f), glm::vec2(127.0f, 0.0f), glm::vec2(127.0f, 63.0f),
	                                     glm::vec2(0.0f, 63.0f)};
	const std::array<uint16_t, 6> front {0, 2, 1, 0, 3, 2};
	for (const bool halfRows : {false, true})
	{
		Coverage coverage;
		RasterTriangles(grid, front, false, halfRows, coverage);
		for (int r = 1; r < 31; ++r)
		{
			for (int c = 1; c < 31; ++c)
			{
				ASSERT_EQ(ByteAt(coverage, r, c), halfRows ? 0xF0 : 0xFF) << r << ", " << c;
			}
		}
		// subrow 63 and subsample 127 are never filled: ToGrid stops at 63 / 127 and both are half open
		EXPECT_EQ(ByteAt(coverage, 31, 10), halfRows ? 0x00 : 0x0F);
		EXPECT_EQ(ByteAt(coverage, 10, 31), halfRows ? 0x70 : 0x77);
		Texels texels;
		Resolve(coverage, texels);
		EXPECT_EQ(texels[static_cast<size_t>(5 * 32 + 5)], halfRows ? 4 : 8);
		EXPECT_EQ(*std::max_element(texels.begin(), texels.end()), halfRows ? 4 : 8);
		for (int i = 0; i < 32; ++i)
		{
			EXPECT_EQ(texels[static_cast<size_t>(i)], 0);
			EXPECT_EQ(texels[static_cast<size_t>(31 * 32 + i)], 0);
			EXPECT_EQ(texels[static_cast<size_t>(i * 32)], 0);
			EXPECT_EQ(texels[static_cast<size_t>(i * 32 + 31)], 0);
		}
	}
}

TEST(ShadowMath, RasterFaces)
{
	const std::array<glm::vec2, 3> grid {glm::vec2(0.0f, 0.0f), glm::vec2(127.0f, 0.0f), glm::vec2(127.0f, 63.0f)};
	const std::array<uint16_t, 3> back {0, 1, 2};
	Coverage coverage;
	RasterTriangles(grid, back, false, false, coverage);
	EXPECT_TRUE(std::all_of(coverage.bytes.begin(), coverage.bytes.end(), [](uint8_t b) { return b == 0; }));
	RasterTriangles(grid, back, true, false, coverage);
	EXPECT_EQ(ByteAt(coverage, 2, 30), 0xFF); // right of the diagonal
	EXPECT_EQ(ByteAt(coverage, 30, 2), 0x00);
	const std::array<uint16_t, 3> front {0, 2, 1};
	Coverage other;
	RasterTriangles(grid, front, false, false, other);
	EXPECT_EQ(other.bytes, coverage.bytes);
}

TEST(ShadowMath, RasterEdges)
{
	// x from 4.9 to 12.9 (>> 16: columns 4..11), rows 2..6 half open (subrows 2..5)
	const std::array<glm::vec2, 4> grid {glm::vec2(4.9f, 2.0f), glm::vec2(12.9f, 2.0f), glm::vec2(12.9f, 6.0f),
	                                     glm::vec2(4.9f, 6.0f)};
	const std::array<uint16_t, 6> quad {0, 2, 1, 0, 3, 2};
	Coverage coverage;
	RasterTriangles(grid, quad, true, false, coverage);
	EXPECT_EQ(ByteAt(coverage, 1, 1), 0xFF);
	EXPECT_EQ(ByteAt(coverage, 1, 2), 0xFF);
	EXPECT_EQ(ByteAt(coverage, 2, 1), 0xFF);
	EXPECT_EQ(ByteAt(coverage, 2, 2), 0xFF);
	EXPECT_EQ(ByteAt(coverage, 3, 1), 0x00);
	EXPECT_EQ(ByteAt(coverage, 1, 3), 0x00);
	EXPECT_EQ(ByteAt(coverage, 0, 1), 0x00);
	EXPECT_EQ(ByteAt(coverage, 1, 0), 0x00);
	// a slanted edge: from (0, 0) to (8, 4), x = 2 y per row, truncated
	const std::array<glm::vec2, 3> slant {glm::vec2(0.0f, 0.0f), glm::vec2(8.0f, 4.0f), glm::vec2(0.0f, 4.0f)};
	const std::array<uint16_t, 3> triangle {0, 1, 2};
	Coverage left;
	RasterTriangles(slant, triangle, true, false, left);
	// row 1: [0, 2) -> subsamples 0, 1 of the high nibble of texel (0, 0); row 3: [0, 6)
	EXPECT_EQ(ByteAt(left, 0, 0), 0x30);
	EXPECT_EQ(ByteAt(left, 1, 0), 0xFF & (0x0F | 0xF0));
	EXPECT_EQ(ByteAt(left, 1, 1), 0x30);
}

TEST(ShadowMath, ResolveBakeChroma)
{
	Coverage coverage;
	std::fill(coverage.bytes.begin(), coverage.bytes.end(), uint8_t {0xFF});
	coverage.bytes[static_cast<size_t>(3 * 32 + 3)] = 0xF0;
	Texels texels;
	Resolve(coverage, texels);
	EXPECT_EQ(texels[static_cast<size_t>(2 * 32 + 2)], 8);
	EXPECT_EQ(texels[static_cast<size_t>(3 * 32 + 3)], 4);
	EXPECT_EQ(texels[0], 0);
	EXPECT_EQ(texels[31], 0);

	auto eight = Full(8);
	BakeAlpha(eight, 200);
	EXPECT_EQ(eight[static_cast<size_t>(1 * 32 + 1)], 6);
	auto four = Full(4);
	BakeAlpha(four, 128);
	EXPECT_EQ(four[static_cast<size_t>(30 * 32 + 30)], 2);
	auto same = Full(8);
	BakeAlpha(same, 255);
	EXPECT_EQ(same, Full(8));
	auto ring = Full(8);
	ring[0] = 5; // never touched
	BakeAlpha(ring, 10);
	EXPECT_EQ(ring[0], 5);
	EXPECT_EQ(ring[static_cast<size_t>(1 * 32 + 1)], 0);

	std::vector<uint16_t> rendered(32 * 32, 0);
	rendered[static_cast<size_t>(4 * 32 + 4)] = 0xF000;
	rendered[static_cast<size_t>(4 * 32 + 5)] = 0xF000;
	Texels chroma(32 * 32, 0);
	chroma[static_cast<size_t>(4 * 32 + 4)] = 8;
	ChromaFilter(rendered, chroma);
	EXPECT_EQ(chroma[static_cast<size_t>(4 * 32 + 4)], 8 | 7); // OR, not max
	EXPECT_EQ(chroma[static_cast<size_t>(3 * 32 + 4)], 7);     // (r + 1, c), (r + 1, c + 1)
	EXPECT_EQ(chroma[static_cast<size_t>(4 * 32 + 3)], 3);     // 15 / 4
	EXPECT_EQ(chroma[static_cast<size_t>(4 * 32 + 5)], 3);
}

// The chroma casters' render: the 64 x 64 alpha map, the vertex into the
// 32 x 32 target and the textured triangle that ORs (a & 0xE0) << 7
TEST(ShadowMath, Chroma)
{
	// the alpha map: byte (r, c) = high byte & 0xF0 of the texel (r h / 64, c w / 64); 128 x 128 -> every other one
	std::vector<uint16_t> source(128 * 128, 0x0FFF);
	source[static_cast<size_t>(2 * 128 + 4)] = 0xA123;
	source[static_cast<size_t>(3 * 128 + 4)] = 0xF000; // an odd row: skipped
	const auto map = MakeAlphaMap(source, 128, 128);
	EXPECT_EQ(map[static_cast<size_t>(1 * 64 + 2)], 0xA0);
	EXPECT_EQ(map[0], 0x00); // 0x0FFF: alpha 0
	EXPECT_EQ(std::count(map.begin(), map.end(), uint8_t {0xF0}), 0);

	// the vertex: t = (base - Ly) / (W.y - Ly), x = ((W.x - Lx) t + Lx - x0) 32 / (x1 - x0) in [1, 31], uv x 63
	Projection projection {glm::vec3(16.0f, 100.0f, 16.0f), glm::vec3(0.0f, -100.0f, 0.0f), 0.0f};
	Box box;
	box.x0 = 0.0f;
	box.x1 = 64.0f;
	box.z0 = 0.0f;
	box.z1 = 64.0f;
	const glm::mat4 identity(1.0f);
	const auto vertex = ChromaVertex(projection, box, identity, glm::vec3(16.0f, 0.0f, 32.0f), glm::vec2(0.5f, 1.0f));
	EXPECT_FLOAT_EQ(vertex.x, 8.0f);
	EXPECT_FLOAT_EQ(vertex.y, 16.0f);
	EXPECT_FLOAT_EQ(vertex.z, 31.5f);
	EXPECT_FLOAT_EQ(vertex.w, 63.0f);
	// h above the base magnifies from the light (no clamp of h): y = 50 halfway to Ly = 100 -> t = 2
	const auto raised = ChromaVertex(projection, box, identity, glm::vec3(20.0f, 50.0f, 16.0f), glm::vec2(0.0f));
	EXPECT_FLOAT_EQ(raised.x, 12.0f); // (20 - 16) 2 + 16 = 24, x 0.5
	EXPECT_FLOAT_EQ(ChromaVertex(projection, box, identity, glm::vec3(-40.0f, 0.0f, 0.0f), glm::vec2(0.0f)).x, 1.0f);
	EXPECT_FLOAT_EQ(ChromaVertex(projection, box, identity, glm::vec3(200.0f, 0.0f, 0.0f), glm::vec2(0.0f)).x, 31.0f);

	// the triangle: inclusive rows and spans, (map & 0xE0) << 7 ORed in: 0xF0 -> nibble 7
	AlphaMap opaque {};
	opaque.fill(0xF0);
	std::vector<uint16_t> target(32 * 32, 0);
	ChromaTriangle(
	    {glm::vec4(2.0f, 2.0f, 0.0f, 0.0f), glm::vec4(10.0f, 2.0f, 63.0f, 0.0f), glm::vec4(2.0f, 10.0f, 0.0f, 63.0f)}, opaque,
	    target, 32);
	EXPECT_EQ(target[static_cast<size_t>(2 * 32 + 2)], 0x7000);
	EXPECT_EQ(target[static_cast<size_t>(3 * 32 + 3)], 0x7000);
	EXPECT_EQ(target[static_cast<size_t>(12 * 32 + 12)], 0);
	EXPECT_EQ(target[static_cast<size_t>(1 * 32 + 1)], 0);
	EXPECT_EQ(std::count(target.begin(), target.end(), uint16_t {0}) +
	              std::count(target.begin(), target.end(), uint16_t {0x7000}),
	          32 * 32);
}

TEST(ShadowMath, Land)
{
	EXPECT_EQ(LandProjectionFactor(50.0f, 250.0f, 50.0f), 1.0f); // no caster: H is the base y
	EXPECT_FLOAT_EQ(LandProjectionFactor(30.0f, 15030.0f, 10.0f), 15000.0f / 15020.0f);

	Box box;
	box.x0 = 320.0f;
	box.x1 = 320.0f;
	box.z0 = 160.0f;
	box.z1 = 200.0f;
	EXPECT_TRUE(TouchesBlock(box, 2, 1)); // bx 160 <= x1 with equality
	EXPECT_TRUE(TouchesBlock(box, 1, 1)); // (bx + 1) 160 >= x0 with equality
	EXPECT_FALSE(TouchesBlock(box, 3, 1));
	EXPECT_TRUE(TouchesBlock(box, 2, 0)); // (bz + 1) 160 >= z0
	EXPECT_FALSE(TouchesBlock(box, 2, 2));
}

TEST(ShadowMath, ReachesMorphable)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_MorphableBoxFactor), 0x3FB50481u); // 1.41419995, not the float nearest sqrt 2
	Box box;
	box.x0 = -5.0f;
	box.x1 = 5.0f; // width 10
	box.z0 = -2.0f;
	box.z1 = 2.0f; // depth 4: the width is the side; the centre (0, 0)
	// an object at (d, 0, 0) with no mesh offset, scale 1, half diagonal 2: R = 2 + 10 x 1.4142
	const float reach = 2.0f + 10.0f * k_MorphableBoxFactor;
	glm::mat4 model(1.0f);
	model[3] = glm::vec4(reach, 0.0f, 0.0f, 1.0f);
	EXPECT_FALSE(ReachesMorphable(box, glm::vec3(0.0f), model, 1.0f, 2.0f)); // d == R: not drawn (strict)
	model[3].x = reach - 0.01f;
	EXPECT_TRUE(ReachesMorphable(box, glm::vec3(0.0f), model, 1.0f, 2.0f));
	// the mesh centre goes through the matrix (90 degrees about y: local x -> world -z); the scale multiplies the half
	// diagonal
	model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	model[3] = glm::vec4(0.0f, 0.0f, 30.0f, 1.0f);
	EXPECT_FALSE(ReachesMorphable(box, glm::vec3(0.0f), model, 1.0f, 2.0f));             // 30 > 16.142
	EXPECT_TRUE(ReachesMorphable(box, glm::vec3(20.0f, 0.0f, 0.0f), model, 1.0f, 2.0f)); // the centre at z 10
	EXPECT_TRUE(ReachesMorphable(box, glm::vec3(0.0f), model, 8.0f, 2.0f));              // R = 16 + 14.142 > 30
	// the depth is the side when the width is not greater
	box.z0 = -15.0f;
	box.z1 = 15.0f; // depth 30
	model = glm::mat4(1.0f);
	model[3] = glm::vec4(0.0f, 0.0f, 40.0f, 1.0f);
	EXPECT_TRUE(ReachesMorphable(box, glm::vec3(0.0f), model, 1.0f, 2.0f)); // 40 < 2 + 42.43
	model[3].z = 45.0f;
	EXPECT_FALSE(ReachesMorphable(box, glm::vec3(0.0f), model, 1.0f, 2.0f));
}

TEST(ShadowMath, BlockVisible)
{
	// the world-to-clip matrix of a camera at (0, 50, 0) looking down +z, a 60-degree lens on a square screen
	// (affine::FrameMatrices)
	const auto worldToClip = openblack::affine::FrameMatrices(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f, 50.0f, 100.0f),
	                                                          glm::radians(60.0f), 1.0f)
	                             .worldToClipping;
	const auto block = [](glm::vec2 corner) {
		std::array<glm::vec3, 8> corners;
		for (int i = 0; i < 8; ++i)
		{
			corners[static_cast<size_t>(i)] = {corner.x + ((i & 1) != 0 ? 160.0f : 0.0f), (i & 2) != 0 ? 60.0f : 0.0f,
			                                   corner.y + ((i & 4) != 0 ? 160.0f : 0.0f)};
		}
		return corners;
	};
	EXPECT_TRUE(BlockVisible(block({-80.0f, 200.0f}), worldToClip, 1.0f));
	EXPECT_FALSE(BlockVisible(block({-80.0f, -400.0f}), worldToClip, 1.0f)); // behind: w < near for all 8
	EXPECT_TRUE(BlockVisible(block({-260.0f, 300.0f}), worldToClip, 1.0f));  // across one side
	EXPECT_FALSE(BlockVisible(block({2000.0f, 300.0f}), worldToClip, 1.0f)); // wholly to one side
}

TEST(ShadowMath, PipelineCube)
{
	// a 2 x 2 x 2 cube 30 above its base under the vertical light: the texture peaks at 8 (4 with halfRows), ring empty
	const std::array<glm::vec3, 8> cube {glm::vec3(-1, 0, -1), glm::vec3(1, 0, -1), glm::vec3(1, 0, 1), glm::vec3(-1, 0, 1),
	                                     glm::vec3(-1, 2, -1), glm::vec3(1, 2, -1), glm::vec3(1, 2, 1), glm::vec3(-1, 2, 1)};
	const std::array<uint16_t, 36> faces {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
	                                      1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0};
	const glm::vec3 position(500.0f, 30.0f, 700.0f);
	const auto projection = MakeProjection(position, LightGeneric(position, false));
	const auto matrix = glm::translate(glm::mat4(1.0f), position);
	for (const bool halfRows : {false, true})
	{
		Box box;
		std::vector<glm::vec2> points;
		for (const auto& v : cube)
		{
			points.push_back(Project(projection, matrix, v, box));
		}
		EXPECT_NEAR(box.x0, 499.0f, 1e-3f);
		EXPECT_NEAR(box.x1, 501.0f, 1e-3f);
		ToGrid(box, points);
		Coverage coverage;
		RasterTriangles(points, faces, true, halfRows, coverage);
		Texels texels;
		Resolve(coverage, texels);
		EXPECT_EQ(*std::max_element(texels.begin(), texels.end()), halfRows ? 4 : 8);
		EXPECT_EQ(texels[0], 0);
		EXPECT_EQ(texels[static_cast<size_t>(31 * 32 + 31)], 0);
	}
}

// L3DSubMesh's skin: the rest pose bone matrices on the local positions give the collision positions, and the hand
// in its rest pose casts a texture of at most 4 with halfRows, 8 with a held object
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(ShadowMath, HandRestPose)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	// the config is the test's only when it had none: then it goes with the test
	const openblack::test::RestoreService<openblack::Locator::config> config;
	if (!openblack::Locator::config::has_value())
	{
		openblack::Locator::config::emplace();
	}
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const openblack::test::BgfxShutdown bgfxShutdown;
	{
		openblack::graphics::L3DMesh mesh("Hand_Boned_Base2");
		std::ifstream file(std::filesystem::path(game) / "Data" / "CreatureMesh" / "Hand_Boned_Base2.l3d", std::ios::binary);
		ASSERT_TRUE(file.good());
		const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		ASSERT_TRUE(mesh.LoadFromBuffer(data));
		ASSERT_TRUE(mesh.IsBoned());
		const auto& bones = mesh.GetBoneMatrices();
		const glm::vec3 position(1000.0f, 40.0f, 1200.0f);
		const auto instance = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		const auto projection = MakeProjection(position, LightHand(position));
		Box box;
		std::vector<glm::vec2> points;
		std::vector<uint16_t> indices;
		for (const auto& subMesh : mesh.GetSubMeshes())
		{
			if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
			{
				continue;
			}
			const auto& local = subMesh->GetSkinLocalPositions();
			const auto& skin = subMesh->GetSkinBones();
			const auto& rest = subMesh->GetCollisionPositions();
			ASSERT_EQ(local.size(), rest.size());
			ASSERT_EQ(skin.size(), rest.size());
			const auto base = static_cast<uint16_t>(points.size());
			for (size_t i = 0; i < local.size(); ++i)
			{
				ASSERT_LT(skin[i], bones.size());
				const auto posed = glm::vec3(bones[skin[i]] * glm::vec4(local[i], 1.0f));
				EXPECT_NEAR(glm::distance(posed, rest[i]), 0.0f, 1e-2f * std::max(1.0f, glm::length(rest[i])));
				points.push_back(Project(projection, instance * bones[skin[i]], local[i], box));
			}
			for (const auto index : subMesh->GetCollisionIndices())
			{
				indices.push_back(static_cast<uint16_t>(base + index));
			}
		}
		ASSERT_FALSE(points.empty());
		ASSERT_LT(box.x0, box.x1);
		ASSERT_LT(box.z0, box.z1);
		auto grid = points;
		ToGrid(box, grid);
		for (const bool halfRows : {true, false})
		{
			Coverage coverage;
			RasterTriangles(grid, indices, true, halfRows, coverage);
			Texels texels;
			Resolve(coverage, texels);
			EXPECT_EQ(*std::max_element(texels.begin(), texels.end()), halfRows ? 4 : 8);
			for (int i = 0; i < k_Texels; ++i)
			{
				EXPECT_EQ(texels[static_cast<size_t>(i)], 0);
				EXPECT_EQ(texels[static_cast<size_t>(i * k_Texels)], 0);
			}
		}
	}
}
