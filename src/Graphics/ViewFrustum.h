/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// What a camera can see: the four sides of its view, which meet at the eye, so that whatever lies inside all four is in
/// front of it and on screen however near or far. Near and far are left out, as the depth range differs between
/// projections and the land is drawn out to the horizon.
namespace openblack::graphics::view_frustum
{

struct Frustum
{
	/// Each side's plane, its normal pointing into the view: a point is inside where dot(xyz, point) + w >= 0
	std::array<glm::vec4, 4> sides;
};

/// The sides of the view a view-projection matrix draws
[[nodiscard]] Frustum FromViewProjection(const glm::mat4& viewProjection) noexcept;

/// Whether any of a sphere is inside the view
[[nodiscard]] bool SeesSphere(const Frustum& frustum, glm::vec3 centre, float radius) noexcept;

/// Whether any of a sphere, or of its reflection in the sea (its height turned over at 0), is inside the view: what the
/// main and the reflection passes draw between them
[[nodiscard]] bool SeesSphereOrReflection(const Frustum& frustum, glm::vec3 centre, float radius) noexcept;

} // namespace openblack::graphics::view_frustum
