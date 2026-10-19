/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The outlines on the ground of the things that stay put: buildings, features, rocks, trees, the temple and the like.
// The game makes the outline of a thing each time the thing goes into the map's cells. The outline is a circle, or a
// circle with a list of smaller circles in it.
//
// A place is free of the fixed things when a circle of half a unit at the place touches no outline of a thing in the
// place's cell. The land's script makes trees, pots and objects only at such places (or on water). A forest plants its
// new trees only at such places too.
//
// Pure functions, tested on their own. The maths is in floats, in the order that the game does it.

namespace openblack::ecs::fixed_outlines
{

/// A circle on the ground. The square of the radius is kept as the game keeps it: for a long box's bounding circle it
/// is the sum of the squares of the two half sides, and the radius is its square root.
struct Circle
{
	glm::vec2 centre {0.0f};
	float radius {0.0f};
	float radiusSquared {0.0f};
};

/// A circle of a radius, with the square of that radius
[[nodiscard]] Circle CircleOf(glm::vec2 centre, float radius);

/// The outline of a thing. A circle with no parts is all there is. When there are parts, a circle must touch the
/// bounding circle and then one of the parts.
struct Outline
{
	Circle bounds;
	std::vector<Circle> parts;
};

/// Neither half side of a model's box counts as less than a unit
inline constexpr float k_LeastHalfSide = 1.0f;
/// A box with one side more than this many times the other is a row of circles; any other box is one circle
inline constexpr float k_RoundBoxRatio = 1.4f;
/// The radius of a tree's outline, at its trunk
inline constexpr float k_TrunkRadius = 0.3f;
/// The radius of the circle that the free place test puts at the place
inline constexpr float k_ProbeRadius = 0.5f;

/// The three rows of a thing's turn and scale: where the model's x, y and z axes go
using Rows = std::array<glm::vec3, 3>;

/// The outline of a model's box, for a thing turned and scaled by the rows, at a place on the ground, with the angle
/// about the upright axis and the scale of the thing:
/// - The middle of the box goes through the rows to the place.
/// - The half sides are scaled, and each is at least a unit.
/// - A box with one side more than 1.4 times the other is a row of circles along its length. Each circle is as wide
///   as the short half side. The bounding circle reaches the corners.
/// - Any other box is one circle with the radius of the long half side.
[[nodiscard]] Outline OfModelBox(const Rows& rows, glm::vec2 place, float yAngle, float scale, glm::vec3 boxCentre,
                                 glm::vec3 boxHalfSize);

/// The row of circles of a long box, centred at a point, with the half sides along the model's x and z, turned by an
/// angle about the upright axis. There is one circle for each whole time that the short side goes into the long side,
/// and one more. The circles are spread evenly along the length of the box.
[[nodiscard]] std::vector<Circle> RowOfBox(glm::vec2 centre, float halfX, float halfZ, float yAngle);

/// The outline of a tree: a small circle at its trunk
[[nodiscard]] Outline OfTrunk(glm::vec2 place);

/// The circles of the temple, in the place of the circles of its model's box. These are the same for every temple
/// model and size:
/// - A wide circle over the middle.
/// - Seven spokes of small circles, each spoke a seventh of a turn from the last.
/// - The first two spokes have three straight circles and then three that bend away from each other.
/// - The other five spokes have four straight circles.
[[nodiscard]] std::vector<Circle> TempleRing(glm::vec2 place, float yAngle);

/// The circle that the free place test puts at a place
[[nodiscard]] Circle Probe(glm::vec2 place);

/// Whether a circle of an outline touches another circle. Circles that only touch count. The order of the two circles
/// changes the last bit of the sums, so it is the order that the game uses.
[[nodiscard]] bool Meets(const Circle& own, const Circle& other);

/// Whether a circle touches an outline: its bounding circle and, when the outline has parts, one of the parts
[[nodiscard]] bool Collides(const Outline& outline, const Circle& other);

/// Whether the game makes a thing at a place: the place is free of the fixed things, or it is water
[[nodiscard]] constexpr bool OkToCreate(bool collidesWithFixed, bool water)
{
	return !collidesWithFixed || water;
}

} // namespace openblack::ecs::fixed_outlines
