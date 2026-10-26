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

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <random>
#include <span>
#include <utility>
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
std::vector<WorldFace> ClosedBox(std::mt19937& random)
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
		const auto faces = ClosedBox(random);
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
	EXPECT_FALSE(NearestBehind(std::span<const FaceBlock> {}, glm::vec3(0.0f), glm::vec3(1.0f)).has_value());
	EXPECT_FALSE(NearestBehindOneByOne({}, glm::vec3(0.0f), glm::vec3(1.0f)).has_value());
}

namespace
{

FaceSet Set(const std::vector<WorldFace>& faces)
{
	FaceSet set;
	set.Reset(faces.size());
	for (std::size_t i = 0; i < faces.size(); ++i)
	{
		set.Store(i, faces[i]);
	}
	return set;
}

/// Whether a point is in a box that is wider by a margin
bool InBox(const openblack::physics::face_search::Box& box, glm::vec3 point, float margin)
{
	return box.min.x - margin <= point.x && point.x <= box.max.x + margin && box.min.y - margin <= point.y &&
	       point.y <= box.max.y + margin && box.min.z - margin <= point.z && point.z <= box.max.z + margin;
}

float LargestPart(glm::vec3 v)
{
	return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

/// A triangle of a random shape: some are thin, some are very thin, some have a normal that is a little wrong
WorldFace RandomFace(std::mt19937& random, glm::vec3 centre, float size)
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_int_distribution<int> kind(0, 5);
	WorldFace face;
	face.a = centre + glm::vec3(unit(random), unit(random), unit(random)) * size;
	face.b = centre + glm::vec3(unit(random), unit(random), unit(random)) * size;
	switch (kind(random))
	{
	case 0:
		// Nearly on the line through the first two corners
		face.c = glm::mix(face.a, face.b, unit(random)) + glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-3f;
		break;
	case 1:
		// On the line, or a very small distance from it
		face.c = glm::mix(face.a, face.b, unit(random)) + glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-6f;
		break;
	case 2:
		// Two corners nearly at the same place
		face.c = face.a + glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-4f;
		break;
	default:
		face.c = centre + glm::vec3(unit(random), unit(random), unit(random)) * size;
		break;
	}
	glm::vec3 normal = glm::cross(face.b - face.a, face.c - face.a);
	const float length = glm::length(normal);
	normal = length > 0.0f ? normal / length : glm::vec3(0.0f);
	if (kind(random) == 0)
	{
		// A turned normal, as a body gives after its axes go a little out of square
		normal = glm::normalize(normal + glm::vec3(unit(random), unit(random), unit(random)) * 0.01f);
	}
	face.normal = normal;
	return face;
}

/// A line that meets a face at a corner or on an edge, a very small distance in or out
std::pair<glm::vec3, glm::vec3> LineToEdge(std::mt19937& random, const WorldFace& face, float size)
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> along(0.0f, 1.0f);
	std::uniform_int_distribution<int> edge(0, 3);
	glm::vec3 target;
	switch (edge(random))
	{
	case 0:
		target = glm::mix(face.a, face.b, along(random));
		break;
	case 1:
		target = glm::mix(face.b, face.c, along(random));
		break;
	case 2:
		target = glm::mix(face.c, face.a, along(random));
		break;
	default:
		target = face.a;
		break;
	}
	target += glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-6f;
	const glm::vec3 direction = glm::vec3(unit(random), unit(random), unit(random)) * size * 2.0f;
	// The point is in front of the target, so the search looks back to it
	return {target + direction * along(random), direction};
}

} // namespace

TEST(PhysicsFaceSearch, BoxCullGivesTheSameHitsOnBoxes)
{
	std::mt19937 random(4321u);
	std::uniform_real_distribution<float> offset(-1.0f, 1.0f);
	std::uniform_real_distribution<float> reach(0.0f, 3.0f);
	int hits = 0;
	for (int box = 0; box < 400; ++box)
	{
		const auto faces = ClosedBox(random);
		const auto blocks = Blocks(faces);
		const auto set = Set(faces);
		const auto centre = (faces[0].a + faces[2].c) * 0.5f;
		for (int sample = 0; sample < 200; ++sample)
		{
			const glm::vec3 point = centre + glm::vec3(offset(random), offset(random), offset(random)) * reach(random);
			const glm::vec3 direction = glm::vec3(offset(random), offset(random), offset(random)) * reach(random);
			const auto expected = NearestBehind(blocks, point, direction);
			hits += expected.has_value() ? 1 : 0;
			ExpectSame(expected, NearestBehind(set, point, direction));
		}
	}
	EXPECT_GT(hits, 10000);
}

TEST(PhysicsFaceSearch, BoxCullGivesTheSameHitsOnThinFacesAndEdges)
{
	std::mt19937 random(2468u);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_int_distribution<int> count(1, 40);
	// Small and large faces, near the origin and as far as the land goes
	constexpr std::array<float, 4> k_Sizes {0.01f, 0.3f, 2.0f, 40.0f};
	constexpr std::array<float, 3> k_Places {0.0f, 900.0f, 5000.0f};
	int hits = 0;
	for (int round = 0; round < 6000; ++round)
	{
		const float size = k_Sizes.at(static_cast<std::size_t>(round) % k_Sizes.size());
		const float place = k_Places.at(static_cast<std::size_t>(round / 4) % k_Places.size());
		const glm::vec3 centre = glm::vec3(unit(random), unit(random), unit(random)) * place;
		std::vector<WorldFace> faces(static_cast<std::size_t>(count(random)));
		for (auto& face : faces)
		{
			face = RandomFace(random, centre, size);
		}
		const auto blocks = Blocks(faces);
		const auto set = Set(faces);
		for (int sample = 0; sample < 40; ++sample)
		{
			const auto& face = faces[static_cast<std::size_t>(sample) % faces.size()];
			const auto [point, direction] = LineToEdge(random, face, size);
			const auto expected = NearestBehind(blocks, point, direction);
			hits += expected.has_value() ? 1 : 0;
			ExpectSame(expected, NearestBehind(set, point, direction));
			ExpectSame(expected, NearestBehindOneByOne(blocks, point, direction));
		}
	}
	EXPECT_GT(hits, 50000);
}

TEST(PhysicsFaceSearch, EachHitIsInTheBoxOfItsFace)
{
	std::mt19937 random(13579u);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	int hits = 0;
	for (int round = 0; round < 200000; ++round)
	{
		const float size = round % 2 == 0 ? 0.05f : 5.0f;
		const glm::vec3 centre = glm::vec3(unit(random), unit(random), unit(random)) * 3000.0f;
		const std::vector<WorldFace> faces {RandomFace(random, centre, size)};
		const auto [point, direction] = LineToEdge(random, faces[0], size);
		const auto hit = NearestBehind(Blocks(faces), point, direction);
		if (!hit.has_value())
		{
			continue;
		}
		const auto box = ReachOf(faces[0]);
		hits += std::isfinite(box.min.x) ? 1 : 0;
		// The search widens the box of the line by this much
		const float lineMargin = (LargestPart(point) + LargestPart(direction)) * 0x1p-17f;
		EXPECT_TRUE(InBox(box, hit->point, lineMargin));
	}
	// Most hits are on faces with a finite box
	EXPECT_GT(hits, 15000);
}

TEST(PhysicsFaceSearch, AGoodFaceGetsATightBox)
{
	const WorldFace face {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {1000.0f, 1.0f, 0.0f}, .b = {1000.0f, 3.0f, 0.0f}, .c = {1002.0f, 2.0f, 0.0f}};
	const auto box = ReachOf(face);
	EXPECT_LT(1000.0f - box.min.x, 0.01f);
	EXPECT_LT(box.max.y - 3.0f, 0.01f);
	EXPECT_LT(box.max.z, 0.01f);
	EXPECT_GE(1000.0f, box.min.x);
	EXPECT_GE(box.max.x, 1002.0f);
}

TEST(PhysicsFaceSearch, AThinOrBadFaceGetsNoFiniteBox)
{
	// The corners are on one line
	const WorldFace line {
	    .normal = {0.0f, 0.0f, 1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {2.0f, 0.0f, 0.0f}};
	EXPECT_TRUE(std::isinf(ReachOf(line).min.x));
	// The normal points against the order of the corners
	const WorldFace back {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {0.0f, 1.0f, 0.0f}};
	EXPECT_TRUE(std::isinf(ReachOf(back).min.x));
	// A zero normal never faces a line, so its box is empty
	const WorldFace none {
	    .normal = {0.0f, 0.0f, 0.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {0.0f, 1.0f, 0.0f}};
	EXPECT_GT(ReachOf(none).min.x, ReachOf(none).max.x);
}

TEST(PhysicsFaceSearch, BoxCullKeepsTheFirstFaceOnATie)
{
	const WorldFace face {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {-1.0f, -1.0f, 0.0f}, .b = {-1.0f, 1.0f, 0.0f}, .c = {1.0f, 0.0f, 0.0f}};
	// Far faces before and between the copies, so the cull drops whole blocks between them
	WorldFace far = face;
	far.a.x += 100.0f;
	far.b.x += 100.0f;
	far.c.x += 100.0f;
	std::vector<WorldFace> faces(20, far);
	faces[9] = face;
	faces[17] = face;
	const glm::vec3 point(0.0f, 0.0f, 0.5f);
	const glm::vec3 direction(0.0f, 0.0f, 1.0f);
	const auto hit = NearestBehind(Set(faces), point, direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->face, 9u);
	ExpectSame(NearestBehind(Blocks(faces), point, direction), hit);
}

TEST(PhysicsFaceSearch, BoxCullSearchesAllWithoutABox)
{
	const WorldFace face {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {-1.0f, -1.0f, 0.0f}, .b = {-1.0f, 1.0f, 0.0f}, .c = {1.0f, 0.0f, 0.0f}};
	const std::vector<WorldFace> faces(5, face);
	const auto set = Set(faces);
	// A direction too large for a box of the line
	const glm::vec3 point(0.0f, 0.0f, 0.5f);
	const glm::vec3 direction(0.0f, 0.0f, 0x1p41f);
	ExpectSame(NearestBehind(Blocks(faces), point, direction), NearestBehind(set, point, direction));
	EXPECT_FALSE(NearestBehind(FaceSet {}, point, glm::vec3(0.0f, 0.0f, 1.0f)).has_value());
}
