/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ground-conforming algorithms of src/3D/LandMorph.h against values worked out by hand from the original's code:
// the triangle cut and its cell planes, the melting deltas, ClampToLandscape and the
// influence circle's curtain.

#include <cmath>

#include <array>
#include <numbers>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/LandMorph.h"

using namespace openblack;

namespace
{
constexpr float k_Epsilon = 1e-5f;

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float epsilon = k_Epsilon)
{
	EXPECT_NEAR(a.x, b.x, epsilon);
	EXPECT_NEAR(a.y, b.y, epsilon);
	EXPECT_NEAR(a.z, b.z, epsilon);
}

/// A flat triangle (4, 0, 0), (14, 0, 0), (4, 0, 10) that crosses the cell line x = 10
land_morph::Primitive KnownTriangle()
{
	land_morph::Primitive primitive;
	primitive.positions = {{4.0f, 0.0f, 0.0f}, {14.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 10.0f}};
	primitive.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
	primitive.diffuse = {0xFF000000u, 0xFFFFFFFFu, 0x80FF0000u};
	primitive.indices = {0, 1, 2};
	return primitive;
}

/// Linear inside every triangle of the cut: its kinks lie on x = 10, x + z = 20 and x - z = 0
float Ridges(glm::vec2 xz)
{
	return std::abs(xz.x - 10.0f) + std::abs(xz.x + xz.y - 20.0f) + 0.5f * std::abs(xz.x - xz.y);
}
} // namespace

TEST(LandMorph, splitByPlaneCutsAKnownTriangle)
{
	auto primitive = KnownTriangle();
	land_morph::SplitByPlane(primitive, glm::vec4(1.0f, 0.0f, 0.0f, -10.0f)); // x = 10
	// d = -6, 4, -6: signs -, +, -, product +, so the lone vertex is A = 1, B = 2, C = 0; t = -4 / (-6 - 4) = 0.4 on both
	// edges, trunc(0.4 x 255) = 102
	ASSERT_EQ(primitive.positions.size(), 5u);
	ExpectNear(primitive.positions[3], glm::vec3(10.0f, 0.0f, 4.0f)); // n1 on A-B
	ExpectNear(primitive.positions[4], glm::vec3(10.0f, 0.0f, 0.0f)); // n2 on A-C
	ASSERT_EQ(primitive.uvs.size(), 5u);
	EXPECT_NEAR(primitive.uvs[3].x, 0.6f, k_Epsilon);
	EXPECT_NEAR(primitive.uvs[3].y, 0.4f, k_Epsilon);
	EXPECT_NEAR(primitive.uvs[4].x, 0.6f, k_Epsilon);
	EXPECT_NEAR(primitive.uvs[4].y, 0.0f, k_Epsilon);
	// alpha 255 + (-127 x 102 >> 8 = -51) = 0xCC, red 0xFF, green and blue 255 + (-255 x 102 >> 8 = -102) = 0x99
	ASSERT_EQ(primitive.diffuse.size(), 5u);
	EXPECT_EQ(primitive.diffuse[3], 0xCCFF9999u);
	EXPECT_EQ(primitive.diffuse[4], 0xFF999999u);
	// no normals and no speculars to cut: they stay empty
	EXPECT_TRUE(primitive.normals.empty());
	EXPECT_TRUE(primitive.specular.empty());
	// (A, n1, n2), then (B, C, n2) and (B, n2, n1): the same winding
	const std::vector<uint32_t> expected = {1, 3, 4, 2, 0, 4, 2, 4, 3};
	EXPECT_EQ(primitive.indices, expected);
}

TEST(LandMorph, splitByPlaneZeroIsNegativeAndSkipsUncutTriangles)
{
	auto primitive = KnownTriangle();
	// x = 4 passes through two vertices: 0, 10, 0 -> -, +, -: one cut, both new vertices at x = 4
	land_morph::SplitByPlane(primitive, glm::vec4(1.0f, 0.0f, 0.0f, -4.0f));
	ASSERT_EQ(primitive.positions.size(), 5u);
	ExpectNear(primitive.positions[3], glm::vec3(4.0f, 0.0f, 10.0f)); // t = -10 / (0 - 10) = 1 on A-B: B itself
	ExpectNear(primitive.positions[4], glm::vec3(4.0f, 0.0f, 0.0f));
	// a plane that misses it leaves it alone
	auto untouched = KnownTriangle();
	land_morph::SplitByPlane(untouched, glm::vec4(1.0f, 0.0f, 0.0f, -20.0f));
	EXPECT_EQ(untouched.positions.size(), 3u);
	EXPECT_EQ(untouched.indices.size(), 3u);
}

TEST(LandMorph, cellPlanesOfABox)
{
	// box (12, 18) x (3, 18): x0 = -1 - trunc(-1.2) = 0, x1 = 1 - trunc(-2.7) = 3, z0 = -1 - trunc(-0.3) = -1,
	// z1 = 1 - trunc(-1.8) = 2, dz = 3
	const auto planes = land_morph::CellPlanes(glm::vec3(12.0f, 0.0f, 3.0f), glm::vec3(27.0f, 0.0f, 18.0f));
	ASSERT_EQ(planes.size(), 4u + 4u + 7u + 7u);
	EXPECT_EQ(planes[0], glm::vec4(1.0f, 0.0f, 0.0f, -0.0f));
	EXPECT_EQ(planes[3], glm::vec4(1.0f, 0.0f, 0.0f, -30.0f));
	EXPECT_EQ(planes[4], glm::vec4(0.0f, 0.0f, 1.0f, 10.0f));  // z = -10
	EXPECT_EQ(planes[7], glm::vec4(0.0f, 0.0f, 1.0f, -20.0f)); // z = 20
	const float s = static_cast<float>(1.0 / std::sqrt(2.0));
	// the first diagonal, i = x0 - dz = -3 through (-30, 0, 20): x + z = -10
	EXPECT_FLOAT_EQ(planes[8].x, s);
	EXPECT_FLOAT_EQ(planes[8].z, s);
	EXPECT_NEAR(planes[8].w / s, 10.0f, k_Epsilon);
	// the second, i = 0 through (0, 0, 20): x - z = -20
	EXPECT_FLOAT_EQ(planes[15].z, -s);
	EXPECT_NEAR(planes[15].w / s, 20.0f, k_Epsilon);
	// the last one, i = x1 + dz = 6: x - z = 40
	EXPECT_NEAR(planes[21].w / s, -40.0f, k_Epsilon);
}

TEST(LandMorph, raiseAboveLandscapeFollowsTheLandExactly)
{
	std::array<land_morph::Primitive, 1> mesh = {KnownTriangle()};
	const land_morph::Ground ground = Ridges;
	land_morph::RaiseAboveLandscape(ground, mesh, glm::vec2(4.0f, 0.0f));
	const auto& primitive = mesh[0];
	const float h0 = Ridges(glm::vec2(4.0f, 0.0f));
	ASSERT_GT(primitive.positions.size(), 3u);
	ASSERT_EQ(primitive.positions.size(), primitive.uvs.size());
	ASSERT_EQ(primitive.positions.size(), primitive.diffuse.size());
	for (const auto& p : primitive.positions)
	{
		EXPECT_NEAR(p.y, Ridges(glm::vec2(p.x, p.z)) - h0, 1e-4f);
	}
	// every triangle lies inside one linear piece of the land: its centre is on the raised land too
	for (size_t t = 0; t + 2 < primitive.indices.size(); t += 3)
	{
		const auto centre = (primitive.positions[primitive.indices[t]] + primitive.positions[primitive.indices[t + 1]] +
		                     primitive.positions[primitive.indices[t + 2]]) /
		                    3.0f;
		EXPECT_NEAR(centre.y, Ridges(glm::vec2(centre.x, centre.z)) - h0, 1e-4f);
	}
	// without the cut the middle of the edge (14, 0) - (4, 10) would miss the ridge
	const glm::vec3 a(14.0f, Ridges(glm::vec2(14.0f, 0.0f)) - h0, 0.0f);
	const glm::vec3 b(4.0f, Ridges(glm::vec2(4.0f, 10.0f)) - h0, 10.0f);
	EXPECT_GT(std::abs((a.y + b.y) * 0.5f - (Ridges(glm::vec2(9.0f, 5.0f)) - h0)), 0.5f);
}

TEST(LandMorph, raiseAboveLandscapeCutsOnlyTheFirstPrimitive)
{
	std::array<land_morph::Primitive, 2> mesh = {KnownTriangle(), KnownTriangle()};
	land_morph::RaiseAboveLandscape(Ridges, mesh, glm::vec2(4.0f, 0.0f));
	EXPECT_GT(mesh[0].positions.size(), 3u);
	ASSERT_EQ(mesh[1].positions.size(), 3u);
	EXPECT_EQ(mesh[1].positions[1].y, 0.0f); // nor raised
}

TEST(LandMorph, meltingDeltaIsInModelUnits)
{
	// H = 0.1 x + 0.2 z; the object at (100, 5, 200), turned a quarter about Y, scale 2
	const land_morph::Ground ground = [](glm::vec2 xz) { return 0.1f * xz.x + 0.2f * xz.y; };
	const auto object = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f)) *
	                    glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)) *
	                    glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	const std::array<glm::vec3, 3> model = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 3.0f, 1.0f)};
	std::array<float, 3> deltas {};
	land_morph::MeltingDeltas(ground, object, 2.0f, model, deltas);
	EXPECT_EQ(deltas[0], 0.0f); // the origin
	// (1, 0, 0) -> world (100, 5, 198): (H - H0) / 2 = (-0.4) / 2
	EXPECT_NEAR(deltas[1], -0.2f, k_Epsilon);
	// (0, 3, 1) -> world (102, 11, 200): the local height does not count, (0.2) / 2
	EXPECT_NEAR(deltas[2], 0.1f, k_Epsilon);
	// drawn: y_world = 5 + 2 (v.y + delta), the land under the vertex + the object's own altitude
	EXPECT_NEAR(5.0f + 2.0f * (model[1].y + deltas[1]),
	            5.0f + ground(glm::vec2(100.0f, 198.0f)) - ground(glm::vec2(100.0f, 200.0f)), k_Epsilon);
}

TEST(LandMorph, meltingSnapshotAgainstLive)
{
	// Snapshot keeps the deltas of the creation; Live (PhysicalShield) takes them again each draw
	float slope = 0.1f;
	const land_morph::Ground ground = [&slope](glm::vec2 xz) { return slope * xz.x; };
	const auto object = glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 0.0f, 50.0f));
	const std::array<glm::vec3, 1> model = {glm::vec3(10.0f, 0.0f, 0.0f)};
	std::array<float, 1> snapshot {};
	land_morph::MeltingDeltas(ground, object, 1.0f, model, snapshot);
	EXPECT_NEAR(snapshot[0], 1.0f, k_Epsilon);
	slope = 0.3f; // the land changes afterwards
	std::array<float, 1> live {};
	land_morph::MeltingDeltas(ground, object, 1.0f, model, live);
	EXPECT_NEAR(snapshot[0], 1.0f, k_Epsilon);
	EXPECT_NEAR(live[0], 3.0f, k_Epsilon);
}

TEST(LandMorph, clampToLandscapeBakesEveryVertexWithoutCutting)
{
	// y = (H(v) - H(M.pos)) + y on every vertex, no new vertex
	std::vector<glm::vec3> vertices = {{4.0f, 1.0f, 0.0f}, {14.0f, 2.0f, 0.0f}, {4.0f, 3.0f, 10.0f}};
	const float h0 = Ridges(glm::vec2(4.0f, 0.0f));
	land_morph::Bake(Ridges, vertices, h0);
	ASSERT_EQ(vertices.size(), 3u);
	EXPECT_EQ(vertices[0].y, 1.0f); // the vertex at the matrix's position keeps its height
	EXPECT_EQ(vertices[1].y, (Ridges(glm::vec2(14.0f, 0.0f)) - h0) + 2.0f);
	EXPECT_EQ(vertices[2].y, (Ridges(glm::vec2(4.0f, 10.0f)) - h0) + 3.0f);
	// the FragMesh order (v.y - (H0 - H)) is the same float
	const glm::vec3 p(14.0f, 2.0f, 0.0f);
	EXPECT_EQ(land_morph::Raised(Ridges, p, h0), p.y - (h0 - Ridges(glm::vec2(p.x, p.z))));
}

TEST(LandMorph, bakeAgainstY)
{
	// melted borders and the citadel: y += H(w) - pos.y, no 1 / scale
	const land_morph::Ground ground = [](glm::vec2 xz) { return 0.5f * xz.x; };
	const auto object =
	    glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 4.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	std::array<glm::vec3, 1> vertices = {glm::vec3(1.0f, 1.0f, 0.0f)}; // world x = 12
	land_morph::BakeAgainstY(ground, object, vertices);
	EXPECT_NEAR(vertices[0].y, 1.0f + 6.0f - 4.0f, k_Epsilon);
}

TEST(LandMorph, groundBuiltHelpers)
{
	const land_morph::Ground ground = [](glm::vec2) { return 3.0f; };
	EXPECT_EQ(land_morph::OnGround(ground, glm::vec2(0.0f), land_morph::k_BlobLift), 3.0f + 0.2f);
	EXPECT_EQ(land_morph::OnGround(ground, glm::vec2(0.0f), land_morph::k_LeashRibbonLift), 3.0f + 0.1f);
	EXPECT_EQ(land_morph::OnGround(ground, glm::vec2(0.0f), land_morph::k_CreatureQuadLift), 3.0f + 0.15f);
}

TEST(LandMorph, influenceCurtain)
{
	const land_morph::Ground flat = [](glm::vec2) { return 3.0f; };
	// r = 50: N = trunc(314.16 x 0.05) = 15, u step (1 - trunc(-2.83)) / 15 = 3 / 15, v step min(trunc(0.83), 6) / 15 = 0
	const auto curtain = land_morph::InfluenceCurtain(flat, glm::vec3(100.0f, 0.0f, 200.0f), 50.0f, 0x80FF0000u);
	ASSERT_EQ(curtain.positions.size(), 3u * 15u + 3u);
	ASSERT_EQ(curtain.uvs.size(), curtain.positions.size());
	ASSERT_EQ(curtain.colours.size(), curtain.positions.size());
	ASSERT_EQ(curtain.indices.size(), 15u * 12u);
	ExpectNear(curtain.positions[0], glm::vec3(150.0f, 3.0f, 200.0f));
	ExpectNear(curtain.positions[1], glm::vec3(150.0f, 23.0f, 200.0f));
	ExpectNear(curtain.positions[2], glm::vec3(150.0f, 43.0f, 200.0f));
	// segment 1 at 2 pi / 15
	const float angle = 2.0f * std::numbers::pi_v<float> / 15.0f;
	ExpectNear(curtain.positions[3], glm::vec3(100.0f + 50.0f * std::cos(angle), 3.0f, 200.0f + 50.0f * std::sin(angle)),
	           1e-3f);
	EXPECT_NEAR(curtain.uvs[3].x, 0.2f, k_Epsilon);
	EXPECT_NEAR(curtain.uvs[4].y, 0.2f, k_Epsilon);
	EXPECT_NEAR(curtain.uvs[5].y, 0.4f, k_Epsilon);
	// the closing column at angle 0, u = 15 steps
	ExpectNear(curtain.positions[45], glm::vec3(150.0f, 3.0f, 200.0f));
	EXPECT_NEAR(curtain.uvs[45].x, 3.0f, 1e-4f);
	EXPECT_EQ(curtain.colours[47], 0x80FF0000u);
	const std::vector<uint32_t> first(curtain.indices.begin(), curtain.indices.begin() + 12);
	EXPECT_EQ(first, (std::vector<uint32_t> {0, 3, 4, 0, 4, 1, 1, 4, 5, 1, 5, 2}));
	// the segment count's clamp: 8 and 250; the v step stops at 6 turns
	EXPECT_EQ(land_morph::InfluenceCurtain(flat, glm::vec3(0.0f), 1.0f, 0).positions.size(), 3u * 8u + 3u);
	const auto big = land_morph::InfluenceCurtain(flat, glm::vec3(0.0f), 2000.0f, 0);
	EXPECT_EQ(big.positions.size(), 3u * 250u + 3u);
	EXPECT_NEAR(big.uvs.back().y - 0.4f, 6.0f, 1e-3f);
}

TEST(LandMorph, objectProgramNames)
{
	EXPECT_EQ(land_morph::ObjectProgramName(false), "ObjectInstanced");
	EXPECT_EQ(land_morph::ObjectProgramName(true), "ObjectHeightMapInstanced");
	EXPECT_EQ(land_morph::ObjectProgramName(false, land_morph::ObjectPass::Shadow), "ObjectShadowInstanced");
	EXPECT_EQ(land_morph::ObjectProgramName(true, land_morph::ObjectPass::Shadow), "ObjectHeightMapShadowInstanced");
}
