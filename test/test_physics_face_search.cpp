/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The search for the face behind a point. The search with four lanes, the search one face at a time and a copy of the
// first search that the physics used must give the same face and the same bits of the hit for many random cases.

#include <cstdint>
#include <cstring>

#include <optional>
#include <random>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/X87Vector.h"
#include "Physics/FaceSearch.h"

using namespace openblack::physics::face_search;

namespace
{

/// The first search of the physics, one face after the other with glm vectors. The test uses it as the reference.
std::optional<Hit> Reference(const std::vector<WorldFace>& faces, glm::vec3 point, glm::vec3 direction)
{
	using openblack::x87::DotZyx;
	const auto inside = [](glm::vec3 p, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 normal) {
		return DotZyx(glm::cross(v1 - v0, p - v0), normal) > 0.0f && DotZyx(glm::cross(v2 - v1, p - v1), normal) > 0.0f &&
		       DotZyx(glm::cross(v0 - v2, p - v2), normal) > 0.0f;
	};
	std::optional<Hit> found;
	float best = -1.0f;
	for (std::size_t i = 0; i < faces.size(); ++i)
	{
		const auto& face = faces[i];
		const float facing = openblack::x87::Dot<openblack::x87::SumOrder::Xzy>(face.normal, direction);
		if (!(facing < k_FacingThreshold))
		{
			continue;
		}
		const float t = -DotZyx(point - face.a, face.normal) / facing;
		if (!(t < 0.0f) || !(best < t))
		{
			continue;
		}
		const auto hit = point + t * direction;
		if (!inside(hit, face.a, face.b, face.c, face.normal))
		{
			continue;
		}
		best = t;
		found = Hit {.point = hit, .face = i};
	}
	return found;
}

std::vector<FaceBlock> Blocks(const std::vector<WorldFace>& faces)
{
	std::vector<FaceBlock> blocks(BlocksFor(faces.size()));
	for (std::size_t i = 0; i < faces.size(); ++i)
	{
		Store(blocks, i, faces[i]);
	}
	return blocks;
}

uint32_t Bits(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

void ExpectSame(const std::optional<Hit>& expected, const std::optional<Hit>& actual)
{
	ASSERT_EQ(expected.has_value(), actual.has_value());
	if (!expected.has_value())
	{
		return;
	}
	EXPECT_EQ(expected->face, actual->face);
	EXPECT_EQ(Bits(expected->point.x), Bits(actual->point.x));
	EXPECT_EQ(Bits(expected->point.y), Bits(actual->point.y));
	EXPECT_EQ(Bits(expected->point.z), Bits(actual->point.z));
}

/// A closed box of twelve faces about the origin, its normals out, turned and moved as a body is
std::vector<WorldFace> Box(std::mt19937& random)
{
	std::uniform_real_distribution<float> size(0.2f, 3.0f);
	std::uniform_real_distribution<float> spread(-50.0f, 50.0f);
	std::uniform_real_distribution<float> angle(-3.1f, 3.1f);
	const glm::vec3 half(size(random), size(random), size(random));
	const glm::vec3 centre(spread(random), spread(random), spread(random));
	const float a = angle(random);
	const auto turn = [&](glm::vec3 v) {
		return glm::vec3(v.x * std::cos(a) - v.z * std::sin(a), v.y, v.x * std::sin(a) + v.z * std::cos(a));
	};
	std::array<glm::vec3, 8> corners {};
	for (std::size_t i = 0; i < corners.size(); ++i)
	{
		const glm::vec3 local((i & 1) != 0 ? half.x : -half.x, (i & 2) != 0 ? half.y : -half.y,
		                      (i & 4) != 0 ? half.z : -half.z);
		corners.at(i) = turn(local) + centre;
	}
	constexpr std::array<std::array<int, 3>, 12> k_Triangles {{{0, 2, 1},
	                                                           {1, 2, 3},
	                                                           {4, 5, 6},
	                                                           {5, 7, 6},
	                                                           {0, 1, 4},
	                                                           {1, 5, 4},
	                                                           {2, 6, 3},
	                                                           {3, 6, 7},
	                                                           {0, 4, 2},
	                                                           {2, 4, 6},
	                                                           {1, 3, 5},
	                                                           {3, 7, 5}}};
	std::vector<WorldFace> faces;
	for (const auto& triangle : k_Triangles)
	{
		const auto& p0 = corners.at(triangle[0]);
		const auto& p1 = corners.at(triangle[1]);
		const auto& p2 = corners.at(triangle[2]);
		faces.push_back({.normal = glm::normalize(glm::cross(p1 - p0, p2 - p0)), .a = p0, .b = p1, .c = p2});
	}
	return faces;
}

} // namespace

TEST(PhysicsFaceSearch, BoxesGiveTheSameHitsAsTheFirstSearch)
{
	std::mt19937 random(1234u);
	std::uniform_real_distribution<float> offset(-1.0f, 1.0f);
	std::uniform_real_distribution<float> reach(0.0f, 3.0f);
	int hits = 0;
	for (int box = 0; box < 400; ++box)
	{
		const auto faces = Box(random);
		const auto blocks = Blocks(faces);
		const auto centre = (faces[0].a + faces[2].c) * 0.5f;
		for (int sample = 0; sample < 200; ++sample)
		{
			// Points near the box, with lines from another body's centre through them
			const glm::vec3 point = centre + glm::vec3(offset(random), offset(random), offset(random)) * reach(random);
			const glm::vec3 direction = glm::vec3(offset(random), offset(random), offset(random)) * reach(random);
			const auto expected = Reference(faces, point, direction);
			hits += expected.has_value() ? 1 : 0;
			ExpectSame(expected, NearestBehindOneByOne(blocks, point, direction));
			ExpectSame(expected, NearestBehind(blocks, point, direction));
		}
	}
	EXPECT_GT(hits, 10000);
}

TEST(PhysicsFaceSearch, RandomTrianglesGiveTheSameHitsAsTheFirstSearch)
{
	std::mt19937 random(98765u);
	std::uniform_real_distribution<float> place(-2.0f, 2.0f);
	std::uniform_int_distribution<int> count(1, 23);
	for (int round = 0; round < 3000; ++round)
	{
		std::vector<WorldFace> faces(static_cast<std::size_t>(count(random)));
		for (auto& face : faces)
		{
			face.a = {place(random), place(random), place(random)};
			face.b = {place(random), place(random), place(random)};
			face.c = {place(random), place(random), place(random)};
			// Some normals point the wrong way or are not unit, as the search must not depend on them
			face.normal = glm::cross(face.b - face.a, face.c - face.a) * place(random);
		}
		const auto blocks = Blocks(faces);
		for (int sample = 0; sample < 20; ++sample)
		{
			const glm::vec3 point(place(random), place(random), place(random));
			const glm::vec3 direction(place(random), place(random), place(random));
			const auto expected = Reference(faces, point, direction);
			ExpectSame(expected, NearestBehindOneByOne(blocks, point, direction));
			ExpectSame(expected, NearestBehind(blocks, point, direction));
		}
	}
}

TEST(PhysicsFaceSearch, OnATieTheFirstFaceWins)
{
	const WorldFace face {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {-1.0f, -1.0f, 0.0f}, .b = {-1.0f, 1.0f, 0.0f}, .c = {1.0f, 0.0f, 0.0f}};
	// Six copies of the same face, so the tie is in the first block and between the blocks
	const std::vector<WorldFace> faces(6, face);
	const auto blocks = Blocks(faces);
	const glm::vec3 point(0.0f, 0.0f, 0.5f);
	const glm::vec3 direction(0.0f, 0.0f, 1.0f);
	const auto hit = NearestBehind(blocks, point, direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->face, 0u);
	EXPECT_FLOAT_EQ(hit->point.z, 0.0f);
	ExpectSame(Reference(faces, point, direction), hit);
}

TEST(PhysicsFaceSearch, NoFacesGiveNoHit)
{
	EXPECT_FALSE(NearestBehind({}, glm::vec3(0.0f), glm::vec3(1.0f)).has_value());
	EXPECT_FALSE(NearestBehindOneByOne({}, glm::vec3(0.0f), glm::vec3(1.0f)).has_value());
}
