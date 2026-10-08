/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <3D/LandNormal.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

// The island's normal of a point in a cell (land_normal::OfCell) and its tables. The expected bits are those of
// the original's float operations in its order; the plane of the triangle's three corners checks the geometry.

namespace land_normal = openblack::land_normal;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The unit normal of the plane through three corners (cell units: 10 m, heights x 0.67), pointing up
glm::vec3 Plane(glm::ivec2 a, int ha, glm::ivec2 b, int hb, glm::ivec2 c, int hc)
{
	const auto point = [](glm::ivec2 corner, int height) {
		return glm::dvec3(10.0 * corner.x, 0.67 * height, 10.0 * corner.y);
	};
	auto n = glm::normalize(glm::cross(point(b, hb) - point(a, ha), point(c, hc) - point(a, ha)));
	return glm::vec3(n.y < 0.0 ? -n : n);
}

void ExpectNear(const glm::vec3& n, const glm::vec3& expected, float tolerance)
{
	EXPECT_NEAR(n.x, expected.x, tolerance);
	EXPECT_NEAR(n.y, expected.y, tolerance);
	EXPECT_NEAR(n.z, expected.z, tolerance);
}
} // namespace

TEST(TestLandNormal, Tables)
{
	EXPECT_EQ(Bits(land_normal::EdgeScale(0)), 0x3DCCCCCDu);   // 1 / sqrt(100) = 0.1
	EXPECT_EQ(Bits(land_normal::EdgeScale(255)), 0x3BBF775Eu); // 1 / sqrt((0.67 255)^2 + 100)
	EXPECT_EQ(Bits(land_normal::EdgeScale(300)), 0x3BA2D2D1u); // (port guard) past the table, the same formula
	EXPECT_EQ(land_normal::LengthScale(0), 1.0f);
	EXPECT_EQ(Bits(land_normal::LengthScale(307)), 0x3FE9A82Au); // |n|^2 = 0.3: k = 307, 1 / sqrt(307 / 1023)
	EXPECT_EQ(land_normal::LengthScale(1023), 1.0f);
}

TEST(TestLandNormal, FlatCell)
{
	const auto n = land_normal::OfCell(0x8000, 0x4000, true, 10, 10, 10, 10);
	EXPECT_EQ(n.x, 0.0f);
	EXPECT_EQ(Bits(n.y), 0x3F800001u); // 0.01 * 100 through T2[1023]: one ulp above 1, as the original
	EXPECT_EQ(n.z, 0.0f);
}

TEST(TestLandNormal, SlopeAlongX)
{
	// 6.7 m up over the 10 m of x: (-0.67, 1, 0) normalised
	const auto n = land_normal::OfCell(0x8000, 0x4000, false, 0, 0, 10, 10);
	EXPECT_EQ(Bits(n.x), 0xBF0E7E61u);
	EXPECT_EQ(Bits(n.y), 0x3F54AD58u);
	EXPECT_EQ(n.z, 0.0f);
	ExpectNear(n, glm::normalize(glm::vec3(-6.7f, 10.0f, 0.0f)), 1e-6f);
}

TEST(TestLandNormal, TheFourTriangles)
{
	// heights h00 = 0, h01 = 20, h10 = 40, h11 = 5: the split bit and the fractions choose the triangle
	const int h00 = 0;
	const int h01 = 20;
	const int h10 = 40;
	const int h11 = 5;
	// split, fz > 0xFFFF - fx: B = h11, P = h10, Q = h01
	auto n = land_normal::OfCell(0xC000, 0xC000, true, h00, h01, h10, h11);
	EXPECT_EQ(Bits(n.x), 0x3EBBC8E7u);
	EXPECT_EQ(Bits(n.y), 0x3EBAD9BBu);
	EXPECT_EQ(Bits(n.z), 0x3F5B150Du);
	ExpectNear(n, Plane({1, 1}, h11, {1, 0}, h10, {0, 1}, h01), 1e-3f);
	// split, the other half: B = h00
	n = land_normal::OfCell(0x2000, 0x2000, true, h00, h01, h10, h11);
	EXPECT_EQ(Bits(n.x), 0xBF5940F5u);
	EXPECT_EQ(Bits(n.y), 0x3EA22132u);
	EXPECT_EQ(Bits(n.z), 0xBED940F5u);
	ExpectNear(n, Plane({0, 0}, h00, {1, 0}, h10, {0, 1}, h01), 1e-3f);
	// no split, fx > fz: B = h10, P = h11, Q = h00
	n = land_normal::OfCell(0xC000, 0x2000, false, h00, h01, h10, h11);
	EXPECT_EQ(Bits(n.x), 0xBF398DD1u);
	EXPECT_EQ(Bits(n.y), 0x3E8A791Eu);
	EXPECT_EQ(Bits(n.z), 0x3F225C17u);
	ExpectNear(n, Plane({1, 0}, h10, {1, 1}, h11, {0, 0}, h00), 1e-3f);
	// no split, fx <= fz: B = h01
	n = land_normal::OfCell(0x2000, 0xC000, false, h00, h01, h10, h11);
	EXPECT_EQ(Bits(n.x), 0x3F03DE2Du);
	EXPECT_EQ(Bits(n.y), 0x3F033639u);
	EXPECT_EQ(Bits(n.z), 0xBF2FD2E7u);
	ExpectNear(n, Plane({0, 1}, h01, {1, 1}, h11, {0, 0}, h00), 1e-3f);
}

TEST(TestLandNormal, TableQuantisationOnASteepCell)
{
	// |n|^2 1023 = 7: T2[7] leaves the length 0.2 % off 1 (the original's normal is not unit length)
	const auto n = land_normal::OfCell(0x1000, 0x8000, false, 255, 0, 0, 255);
	EXPECT_EQ(Bits(n.x), 0xBF348574u);
	EXPECT_EQ(Bits(n.y), 0x3D290E9Fu);
	EXPECT_EQ(Bits(n.z), 0x3F348574u);
	EXPECT_GT(std::abs(glm::length(n) - 1.0f), 1e-3f);
	ExpectNear(n, Plane({0, 1}, 0, {1, 1}, 255, {0, 0}, 255), 2e-3f);
}
