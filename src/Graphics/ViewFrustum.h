/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include <glm/common.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
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

/// Whether any of a box, which a model matrix puts in the world, can be inside the view. The answer is false only when
/// all eight corners are outside one side by more than k_BoxMargin. The margin is larger than the rounding of floats,
/// so a box that touches the screen is always in view.
[[nodiscard]] bool SeesBox(const Frustum& frustum, const glm::mat4& model, glm::vec3 minima, glm::vec3 maxima) noexcept;

/// Whether any of a box, which a model matrix puts in the world and which can then move along `sweep` by any amount from
/// minus one to one times it, can be inside the view. The test is the same as SeesBox, with sixteen corners: the eight
/// corners moved to each end of the sweep.
[[nodiscard]] bool SeesSweptBox(const Frustum& frustum, const glm::mat4& model, glm::vec3 minima, glm::vec3 maxima,
                                glm::vec3 sweep) noexcept;

/// How far a model that goes with the land can move each of its points, as a world vector: the vertex shader moves each
/// point along the model's up by the difference of two land heights, divided by the model's size. `highestRise` is the
/// highest land height on the map, which no such difference can be more than. There is no limit (no value) for a tilted
/// model of no size.
[[nodiscard]] std::optional<glm::vec3> LandRiseSweep(const glm::mat4& model, float highestRise) noexcept;

/// How far a corner of a square from -1 to 1 across and up goes from the square's middle, after a model's turn and size.
/// A sprite is such a square. The turn that makes it face the camera does not change the distance.
[[nodiscard]] float QuadRadius(const glm::mat3& turnAndSize) noexcept;

/// The height of a land corner per unit of its altitude, in world units
inline constexpr float k_AltitudeUnit = 0.67f;
/// The highest height that the land can have, at the highest altitude of a corner
inline constexpr float k_HighestLand = static_cast<float>(std::numeric_limits<uint8_t>::max()) * k_AltitudeUnit;

/// The highest land height that the vertex shader can find under a model that goes with the land: under the box that
/// the model matrix puts in the world, and under the model's origin. The shader finds a height from the four corners of
/// its cell of 10 units. The function also reads the corners of one more cell on each side, because the shader rounds the
/// place. `altitudeAt(corner)` gives the altitude of one of the 512 by 512 corners of the map.
template <typename AltitudeAt>
[[nodiscard]] float HighestLandUnder(const glm::mat4& model, glm::vec3 minima, glm::vec3 maxima, AltitudeAt&& altitudeAt)
{
	constexpr float k_CellSize = 10.0f;
	constexpr int k_LastCorner = 511;
	// The place on the map of the origin and of each corner of the box
	glm::vec2 low {model[3].x, model[3].z};
	glm::vec2 high = low;
	for (uint32_t i = 0; i < 8; ++i)
	{
		const glm::vec4 corner {(i & 1u) != 0 ? maxima.x : minima.x, (i & 2u) != 0 ? maxima.y : minima.y,
		                        (i & 4u) != 0 ? maxima.z : minima.z, 1.0f};
		const auto world = model * corner;
		low = glm::min(low, glm::vec2(world.x, world.z));
		high = glm::max(high, glm::vec2(world.x, world.z));
	}
	// Clamped before the change to whole numbers, so that a place far off the map cannot overflow
	constexpr float k_Off = 2.0f * k_CellSize;
	constexpr float k_Far = (static_cast<float>(k_LastCorner) + 2.0f) * k_CellSize;
	const auto cornerOf = [](glm::vec2 place) {
		return glm::ivec2(glm::floor(glm::clamp(place, glm::vec2(-k_Off), glm::vec2(k_Far)) / k_CellSize));
	};
	const auto first = glm::max(cornerOf(low) - 1, glm::ivec2(0));
	const auto last = glm::min(cornerOf(high) + 2, glm::ivec2(k_LastCorner));
	uint8_t highest = 0;
	for (int x = first.x; x <= last.x; ++x)
	{
		for (int z = first.y; z <= last.y; ++z)
		{
			highest = std::max(highest, static_cast<uint8_t>(altitudeAt(glm::ivec2(x, z))));
		}
	}
	return static_cast<float>(highest) * k_AltitudeUnit;
}

/// The distance outside a side that SeesBox still counts as in view, in world units
inline constexpr float k_BoxMargin = 1.0f;

} // namespace openblack::graphics::view_frustum
