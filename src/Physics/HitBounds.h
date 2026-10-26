/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

/// A box that holds every hit that the face search can find on a body that keeps its shape. If the line of a point does
/// not touch the box, the search finds no face. Thus a search that stops at this test gives the same result as the full
/// search. The box includes the float errors of the search, of the turn of the body and of the move of the body.
namespace openblack::physics::hit_bounds
{

/// The largest difference between a product of two axes and the product of two axes that are square and of unit length
inline constexpr float k_LargestAxisError = 0x1p-12f;
/// The largest shift that the margins accept. The shift is the largest part of the centre of a body plus the largest
/// part of its look-ahead move.
inline constexpr float k_LargestShift = 0x1p20f;

/// A box in the world. A box with a minimum above its maximum holds no point.
struct Box
{
	glm::vec3 min {0.0f};
	glm::vec3 max {0.0f};
};

/// One face of a body in the body's own space: its normal and its three corners
struct LocalFace
{
	glm::vec3 normal {0.0f};
	glm::vec3 a {0.0f};
	glm::vec3 b {0.0f};
	glm::vec3 c {0.0f};
};

/// The distance that the box of the corners of a face must grow to hold every hit on the face, for each pose that
/// FitsMargin accepts with the same shift. A face with a zero normal never faces a line and needs no distance. A face that is
/// too thin gives an infinite distance.
/// @param largestPart The largest size of a part of a corner of the body in its own space
/// @param shift The largest shift of the poses
[[nodiscard]] float FaceMargin(const LocalFace& face, float largestPart, float shift);

/// The largest margin of the faces of a body
[[nodiscard]] float ShapeMargin(std::span<const LocalFace> faces, float largestPart, float shift);

/// Whether a pose is near enough to a turn and a move for the margins. The pose puts each point at the axes times the
/// point, plus the centre, plus the look-ahead move. It turns each normal with the same axes. The shift of the pose must
/// be less than the shift of the margins.
[[nodiscard]] bool FitsMargin(const glm::mat3& axes, glm::vec3 centre, glm::vec3 ahead, float shift);

/// The box that holds the line from a point back to one direction length behind it, with all hits that the search can
/// find on it. There is no box when the parts are too large.
[[nodiscard]] std::optional<Box> LineBox(glm::vec3 point, glm::vec3 direction);

/// Whether two boxes touch
[[nodiscard]] bool Touches(const Box& first, const Box& second);

} // namespace openblack::physics::hit_bounds
