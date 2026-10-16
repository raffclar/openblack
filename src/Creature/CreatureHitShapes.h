/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The shapes that a fight blow strikes. Each bone of a creature's mesh has a box round the vertices that it places, in
/// the space of the bone. A line meets the ellipsoid that fills the box. The game makes these boxes once, from
/// the base mesh, and the boxes do not change when the body changes its shape.
namespace openblack::creature_hit_shapes
{

/// A number of vertices of a primitive, one after the other, that one bone places
struct VertexGroup
{
	uint16_t vertexCount;
	uint16_t bone;
};

/// The vertices of a primitive and the groups of vertices that its bones place
struct Primitive
{
	std::span<const glm::vec3> positions;
	std::span<const VertexGroup> groups;
};

/// The box round the vertices that a bone places, in the space of the bone. The box starts at the origin of the bone,
/// thus it always holds that origin.
struct Shape
{
	glm::vec3 min {0.0f};
	glm::vec3 max {0.0f};
};

/// The box of each bone, over all the primitives. A mesh without bones has one box round all its vertices. A group
/// with a bone out of the count adds to no box.
[[nodiscard]] std::vector<Shape> ShapesOf(size_t count, bool boned, std::span<const Primitive> primitives);

/// A shape as the body is posed now, with the matrix from the bone to the world and the matrix back
struct PlacedShape
{
	Shape shape;
	glm::mat4 toWorld;
	glm::mat4 toShape;
};

/// Each shape placed by the matrix of its bone in the world. Shapes without a matrix are left out.
[[nodiscard]] std::vector<PlacedShape> Place(std::span<const Shape> shapes, std::span<const glm::mat4> toWorld);

/// Where a line meets a shape: the point in the world, and how far along the line in lengths of its direction
struct Hit
{
	glm::vec3 point;
	float along;
};

/// A box or a line this short in a direction of the box tests nothing
constexpr float k_LeastExtent = 0.0001f;

/// Where the line from the origin along the direction goes into the ellipsoid of the shape. The line has no ends: the
/// point can be behind the origin, and then along is less than 0. None when the line misses the ellipsoid or the box is
/// too thin.
[[nodiscard]] std::optional<Hit> LineMeets(const PlacedShape& placed, const glm::vec3& origin, const glm::vec3& direction);

/// Of all the shapes that the line meets, the one with the smallest along. A shape behind the origin thus comes before
/// a shape ahead of it.
[[nodiscard]] std::optional<Hit> NearestHit(std::span<const PlacedShape> shapes, const glm::vec3& origin,
                                            const glm::vec3& direction);

/// Whether a point lies ahead of the origin, along the direction, and nearer than the end of the direction
[[nodiscard]] bool WithinLine(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& direction);

/// Before a creature chooses a blow, it finds how far the blow reaches into the opponent. Lines go from the point
/// where the blow lands, towards the opponent, in a row across. The row is 4.5 opponent sizes wide (15 x 0.15 each
/// side) and has eleven lines, each 0.1 of the width from the last.
constexpr int k_DepthLines = 11;
constexpr float k_DepthWidthPerSize = 15.0f;
constexpr double k_DepthWidthShare = 0.15;
constexpr float k_DepthSpacingShare = 0.1f;
/// The depth before a line meets the body: the most a blow can be from the body
constexpr float k_NoDepth = 1000.0f;

/// How far ahead of the point the body starts, along the direction to the opponent (a unit vector), as the nearest of
/// the lines that meet it. A negative value is a point inside the body. None when no line meets the body.
[[nodiscard]] std::optional<float> DepthToBody(std::span<const PlacedShape> shapes, const glm::vec3& point,
                                               const glm::vec3& towards, float opponentSize);

} // namespace openblack::creature_hit_shapes
