/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec2.hpp>

// The shape a fixed thing takes up on the ground, as the walkers and the map see it, and the map cells a building is
// filed in.
//
// A thing's shape comes from its mesh's box, scaled and laid flat: a near-square box (one side at most 1.4 times the
// other) is a single circle as wide as the box's longer half side; a longer box is a row of circles along its long side,
// each as wide as its short half side, enough of them to cover it, inside a bounding circle that reaches the box's
// corners. Each half side is at least a metre.
//
// A building is filed in every map cell that a circle a little wider than half a cell, at the cell's middle, touches
// the building's shape in. The cells looked at are those within the mesh's half diagonal (scaled, plus a metre) of the
// shape's middle. A building that touches none of them is filed in the middle one.
//
// Pure, tested on made-up shapes.

namespace openblack::ecs::collide
{

/// A circle on the ground, in metres
struct Circle
{
	glm::vec2 centre;
	float radius;
};

/// A thing's shape on the ground: a bounding circle and, for a long thing, the row of circles inside it that it is made
/// of
struct Shape
{
	Circle bounds;
	/// The bounding circle's radius squared as the shape holds it (for a long box, worked out from its half sides)
	float boundsSquared;
	std::vector<Circle> row;
};

/// The radius of the circle round a map cell's middle that a building must touch to be filed in that cell
constexpr float k_CellReach = 7.1f;
/// How much longer than it is wide a box can be and still be taken as one circle
constexpr float k_SquareEnough = 1.4f;

/// The shape of a box lying on the ground at `centre`, with half sides `halfSize` (already scaled) along its own x and
/// z, which point along `xAxis` and `zAxis`
[[nodiscard]] Shape ShapeOfBox(glm::vec2 centre, glm::vec2 halfSize, glm::vec2 xAxis, glm::vec2 zAxis);

/// Whether a circle touches a shape: its bounding circle, then, for a long shape, one of its row of circles
[[nodiscard]] bool Touches(const Circle& circle, const Shape& shape);

/// The map cells a building of this shape is filed in, x then z, each at most `cells` along a side. `reach` is how far
/// from the shape's middle cells are looked at, in metres. The list stops short at the map's edge.
[[nodiscard]] std::vector<glm::ivec2> FootprintCells(const Shape& shape, float reach, glm::ivec2 cells);

} // namespace openblack::ecs::collide
