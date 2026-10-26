/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The box of all hits of a body that keeps its shape. Each hit that the face search finds must be in the box, for
// bodies with thin faces, turned axes that are a little out of square and places far from the origin.

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Physics/FaceSearch.h"
#include "Physics/HitBounds.h"

using namespace openblack::physics;
using namespace openblack::physics::hit_bounds;

namespace
{

float LargestPart(glm::vec3 v)
{
	return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

/// A unit normal, or zero for corners on one line, as a body makes its normals
glm::vec3 NormalOf(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
	const auto across = glm::cross(b - a, c - a);
	const float length = glm::length(across);
	return length > 0.0f ? across * (1.0f / length) : glm::vec3(0.0f);
}

/// A body's shape: random triangles near its origin, some of them thin or very thin
std::vector<LocalFace> RandomShape(std::mt19937& random, float size)
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_int_distribution<int> kind(0, 5);
	std::uniform_int_distribution<int> count(1, 30);
	std::vector<LocalFace> faces(static_cast<std::size_t>(count(random)));
	for (auto& face : faces)
	{
		face.a = glm::vec3(unit(random), unit(random), unit(random)) * size;
		face.b = glm::vec3(unit(random), unit(random), unit(random)) * size;
		switch (kind(random))
		{
		case 0:
			face.c =
			    face.a + (face.b - face.a) * unit(random) + glm::vec3(unit(random), unit(random), unit(random)) * size * 0.01f;
			break;
		case 1:
			face.c =
			    face.a + (face.b - face.a) * unit(random) + glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-6f;
			break;
		default:
			face.c = glm::vec3(unit(random), unit(random), unit(random)) * size;
			break;
		}
		face.normal = NormalOf(face.a, face.b, face.c);
	}
	return faces;
}

/// Axes of a random turn, with each part moved a little, so they are a little out of square
glm::mat3 RandomAxes(std::mt19937& random)
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	const auto x = glm::normalize(glm::vec3(unit(random), unit(random), unit(random)));
	const auto y = glm::normalize(glm::cross(x, glm::vec3(unit(random), unit(random), unit(random))));
	const auto z = glm::cross(x, y);
	glm::mat3 axes(x, y, z);
	for (int i = 0; i < 3; ++i)
	{
		axes[i] += glm::vec3(unit(random), unit(random), unit(random)) * 0x1p-15f;
	}
	return axes;
}

/// The axes times a vector, with the parts of each row added in the order that the body uses for a step
glm::vec3 Turned(const glm::mat3& axes, glm::vec3 v)
{
	return (axes[1] * v.y + axes[0] * v.x) + axes[2] * v.z;
}

/// The shift of the margin for a pose, as the body chooses it
float ShiftFor(float poseShift)
{
	float shift = 16.0f;
	while (0.99f * shift < poseShift && shift < k_LargestShift)
	{
		shift *= 2.0f;
	}
	return shift;
}

} // namespace

TEST(PhysicsHitBounds, EveryHitIsInTheBoxOfTheBody)
{
	std::mt19937 random(97531u);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> along(0.0f, 1.0f);
	constexpr std::array<float, 3> k_Sizes {0.2f, 1.5f, 6.0f};
	constexpr std::array<float, 3> k_Places {10.0f, 2500.0f, 6000.0f};
	int hits = 0;
	int boxed = 0;
	for (int round = 0; round < 3000; ++round)
	{
		const float size = k_Sizes.at(static_cast<std::size_t>(round) % k_Sizes.size());
		const auto shape = RandomShape(random, size);
		const auto axes = RandomAxes(random);
		const auto centre =
		    glm::vec3(unit(random), unit(random), unit(random)) * k_Places.at(static_cast<std::size_t>(round / 3) % 3);
		const auto ahead = glm::vec3(unit(random), unit(random), unit(random)) * 0.3f;
		ASSERT_TRUE(FitsMargin(axes, centre, ahead, k_LargestShift));
		const float shift = ShiftFor(LargestPart(centre) + LargestPart(ahead));
		ASSERT_TRUE(FitsMargin(axes, centre, ahead, shift));
		float largestPart = 0.0f;
		std::vector<face_search::WorldFace> world;
		for (const auto& face : shape)
		{
			largestPart = std::max({largestPart, LargestPart(face.a), LargestPart(face.b), LargestPart(face.c)});
			world.push_back({.normal = axes * face.normal,
			                 .a = Turned(axes, face.a) + centre + ahead,
			                 .b = Turned(axes, face.b) + centre + ahead,
			                 .c = Turned(axes, face.c) + centre + ahead});
		}
		const float margin = ShapeMargin(shape, largestPart, shift);
		glm::vec3 low(std::numeric_limits<float>::infinity());
		glm::vec3 high(-std::numeric_limits<float>::infinity());
		for (const auto& face : world)
		{
			low = glm::min(glm::min(glm::min(low, face.a), face.b), face.c);
			high = glm::max(glm::max(glm::max(high, face.a), face.b), face.c);
		}
		const Box box {.min = low - margin, .max = high + margin};
		std::vector<face_search::FaceBlock> blocks(face_search::BlocksFor(world.size()));
		for (std::size_t i = 0; i < world.size(); ++i)
		{
			face_search::Store(blocks, i, world[i]);
		}
		for (int sample = 0; sample < 60; ++sample)
		{
			// A line aimed at an edge or a corner of a face, a very small distance in or out
			const auto& face = world[static_cast<std::size_t>(sample) % world.size()];
			const float t = along(random);
			const std::array<glm::vec3, 4> targets {face.a + (face.b - face.a) * t, face.b + (face.c - face.b) * t,
			                                        face.c + (face.a - face.c) * t, face.a};
			const auto target = targets.at(static_cast<std::size_t>(sample) % targets.size()) +
			                    glm::vec3(unit(random), unit(random), unit(random)) * size * 1e-6f;
			const auto direction = glm::vec3(unit(random), unit(random), unit(random)) * size * 2.0f;
			const auto point = target + direction * along(random);
			const auto hit = face_search::NearestBehind(blocks, point, direction);
			if (!hit.has_value())
			{
				continue;
			}
			++hits;
			boxed += std::isfinite(margin) ? 1 : 0;
			EXPECT_TRUE(LineTouches(box, point, direction));
		}
	}
	EXPECT_GT(hits, 50000);
	// Many bodies have a finite box, so the test checks real margins
	EXPECT_GT(boxed, 5000);
}

TEST(PhysicsHitBounds, AGoodFaceGetsASmallMargin)
{
	const LocalFace face {
	    .normal = {0.0f, 0.0f, 1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {0.0f, 1.0f, 0.0f}};
	const float margin = FaceMargin(face, 1.0f, 4096.0f);
	EXPECT_GT(margin, 0.0f);
	EXPECT_LT(margin, 0.05f);
}

TEST(PhysicsHitBounds, ABadFaceGetsAnInfiniteMargin)
{
	// The corners are on one line
	const LocalFace line {
	    .normal = {0.0f, 0.0f, 1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {2.0f, 0.0f, 0.0f}};
	EXPECT_TRUE(std::isinf(FaceMargin(line, 2.0f, 16.0f)));
	// The normal points against the order of the corners
	const LocalFace back {
	    .normal = {0.0f, 0.0f, -1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {0.0f, 1.0f, 0.0f}};
	EXPECT_TRUE(std::isinf(FaceMargin(back, 1.0f, 16.0f)));
	// A face that is small against the errors of a far place
	const LocalFace small {
	    .normal = {0.0f, 0.0f, 1.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {0.001f, 0.0f, 0.0f}, .c = {0.0f, 0.001f, 0.0f}};
	EXPECT_TRUE(std::isinf(FaceMargin(small, 1.0f, k_LargestShift)));
	// A zero normal never faces a line
	const LocalFace none {
	    .normal = {0.0f, 0.0f, 0.0f}, .a = {0.0f, 0.0f, 0.0f}, .b = {1.0f, 0.0f, 0.0f}, .c = {0.0f, 1.0f, 0.0f}};
	EXPECT_EQ(FaceMargin(none, 1.0f, 16.0f), 0.0f);
	const std::array<LocalFace, 2> faces {none, line};
	EXPECT_TRUE(std::isinf(ShapeMargin(faces, 2.0f, 16.0f)));
}

TEST(PhysicsHitBounds, OnlyANearTurnFitsTheMargin)
{
	const glm::mat3 square(1.0f);
	EXPECT_TRUE(FitsMargin(square, glm::vec3(100.0f), glm::vec3(1.0f), 128.0f));
	// The shift is too large for the margin
	EXPECT_FALSE(FitsMargin(square, glm::vec3(100.0f), glm::vec3(30.0f), 128.0f));
	// A mirror
	EXPECT_FALSE(FitsMargin(glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	                        glm::vec3(0.0f), glm::vec3(0.0f), 16.0f));
	// Axes out of square or not of unit length
	EXPECT_FALSE(FitsMargin(glm::mat3(glm::vec3(1.0f, 0.01f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	                        glm::vec3(0.0f), glm::vec3(0.0f), 16.0f));
	EXPECT_FALSE(FitsMargin(square * 1.01f, glm::vec3(0.0f), glm::vec3(0.0f), 16.0f));
}

TEST(PhysicsHitBounds, TheLineTestHoldsTheLine)
{
	const glm::vec3 point(1.0f, 2.0f, 3.0f);
	const glm::vec3 direction(2.0f, -1.0f, 0.0f);
	// The line goes from (1, 2, 3) back to (-1, 3, 3)
	EXPECT_TRUE(LineTouches({.min = glm::vec3(0.0f, 2.5f, 3.0f), .max = glm::vec3(0.5f, 2.6f, 3.0f)}, point, direction));
	EXPECT_TRUE(LineTouches({.min = glm::vec3(-3.0f, 3.0f, 2.0f), .max = glm::vec3(-1.0f, 4.0f, 3.0f)}, point, direction));
	EXPECT_FALSE(LineTouches({.min = glm::vec3(0.0f, 2.5f, 3.1f), .max = glm::vec3(0.5f, 2.6f, 4.0f)}, point, direction));
	EXPECT_FALSE(LineTouches({.min = glm::vec3(1.1f, 0.0f, 0.0f), .max = glm::vec3(2.0f, 5.0f, 5.0f)}, point, direction));
	// A line with parts that are too large touches every box
	EXPECT_TRUE(LineTouches({.min = glm::vec3(5.0f), .max = glm::vec3(6.0f)}, glm::vec3(0.0f), glm::vec3(0x1p41f)));
}
