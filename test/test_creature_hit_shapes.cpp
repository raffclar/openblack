/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHitShapes.h"

using namespace openblack::creature_hit_shapes;

namespace
{
/// An ellipsoid with these half widths round a middle in the world
PlacedShape Ellipsoid(const glm::vec3& middle, const glm::vec3& half)
{
	const auto toWorld = glm::translate(glm::mat4(1.0f), middle);
	return {.shape = {.min = -half, .max = half}, .toWorld = toWorld, .toShape = glm::inverse(toWorld)};
}

void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1e-4f);
	EXPECT_NEAR(actual.y, expected.y, 1e-4f);
	EXPECT_NEAR(actual.z, expected.z, 1e-4f);
}
} // namespace

TEST(CreatureHitShapes, EachBoneHasABoxRoundItsVerticesAndItsOrigin)
{
	const std::array<glm::vec3, 5> positions {{{1, 2, 3}, {2, 1, 1}, {-1, -1, -1}, {4, 4, 4}, {9, 9, 9}}};
	// Two vertices for bone 1, one for bone 0, one for a bone out of the count; the last vertex has no group
	const std::array<VertexGroup, 3> groups {
	    {{.vertexCount = 2, .bone = 1}, {.vertexCount = 1, .bone = 0}, {.vertexCount = 1, .bone = 7}}};
	const std::array<Primitive, 1> primitives {{{.positions = positions, .groups = groups}}};
	const auto shapes = ShapesOf(3, true, primitives);
	ASSERT_EQ(shapes.size(), 3u);
	EXPECT_EQ(shapes[1].min, glm::vec3(0.0f));
	EXPECT_EQ(shapes[1].max, glm::vec3(2.0f, 2.0f, 3.0f));
	EXPECT_EQ(shapes[0].min, glm::vec3(-1.0f));
	EXPECT_EQ(shapes[0].max, glm::vec3(0.0f));
	// A bone with no vertices keeps an empty box at its origin
	EXPECT_EQ(shapes[2].max, glm::vec3(0.0f));
	// A mesh without bones has one box round all its vertices
	const auto whole = ShapesOf(3, false, primitives);
	ASSERT_EQ(whole.size(), 1u);
	EXPECT_EQ(whole[0].min, glm::vec3(-1.0f));
	EXPECT_EQ(whole[0].max, glm::vec3(9.0f));
}

TEST(CreatureHitShapes, ALineGoesIntoTheEllipsoidOfTheBox)
{
	const auto shape = Ellipsoid({5.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f});
	const auto hit = LineMeets(shape, {-5.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f});
	ASSERT_TRUE(hit.has_value());
	ExpectNear(hit->point, {4.0f, 0.0f, 0.0f});
	EXPECT_NEAR(hit->along, 0.45f, 1e-5f);
	// The ellipsoid is 2 high: a line 1.5 up meets it, a line 2.5 up does not
	EXPECT_TRUE(LineMeets(shape, {-5.0f, 1.5f, 0.0f}, {1.0f, 0.0f, 0.0f}).has_value());
	EXPECT_FALSE(LineMeets(shape, {-5.0f, 2.5f, 0.0f}, {1.0f, 0.0f, 0.0f}).has_value());
	// From inside, the point where the line goes in is behind the origin
	const auto inside = LineMeets(shape, {5.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
	ASSERT_TRUE(inside.has_value());
	EXPECT_NEAR(inside->along, -1.0f, 1e-5f);
	// A flat box and a line with no length test nothing
	EXPECT_FALSE(LineMeets(Ellipsoid({5.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}), {-5.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}));
	EXPECT_FALSE(LineMeets(shape, {-5.0f, 0.0f, 0.0f}, glm::vec3(0.0f)).has_value());
}

TEST(CreatureHitShapes, TheShapeWithTheSmallestAlongWinsEvenBehindTheOrigin)
{
	const std::array shapes {Ellipsoid({10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)), Ellipsoid({-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f))};
	const glm::vec3 origin(0.0f);
	const glm::vec3 direction(20.0f, 0.0f, 0.0f);
	const auto hit = NearestHit(shapes, origin, direction);
	ASSERT_TRUE(hit.has_value());
	ExpectNear(hit->point, {-11.0f, 0.0f, 0.0f});
	// That point is behind, so the blow line does not take it
	EXPECT_FALSE(WithinLine(hit->point, origin, direction));
	EXPECT_TRUE(WithinLine({9.0f, 0.0f, 0.0f}, origin, direction));
	EXPECT_FALSE(WithinLine({21.0f, 0.0f, 0.0f}, origin, direction));
	EXPECT_FALSE(NearestHit({}, origin, direction).has_value());
}

TEST(CreatureHitShapes, TheDepthIsTheNearestOfARowOfLines)
{
	// A ball of radius 1, 10 ahead, and a ball of radius 1 2 to the side and 8 ahead: the row 4.5 wide meets both
	const std::array shapes {Ellipsoid({10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)), Ellipsoid({8.0f, 0.0f, 2.0f}, glm::vec3(1.0f))};
	const auto depth = DepthToBody(shapes, glm::vec3(0.0f), {1.0f, 0.0f, 0.0f}, 1.0f);
	ASSERT_TRUE(depth.has_value());
	// The line 1.8 to the side meets the side ball first, 0.2 from its middle
	EXPECT_NEAR(*depth, 8.0f - std::sqrt(0.96f), 1e-3f);
	// A ball outside the row is not found
	const std::array wide {Ellipsoid({10.0f, 0.0f, 5.0f}, glm::vec3(1.0f))};
	EXPECT_FALSE(DepthToBody(wide, glm::vec3(0.0f), {1.0f, 0.0f, 0.0f}, 1.0f).has_value());
}
