/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>

#include <array>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a rigid town building settles on uneven land. A building whose model does not bend with the land keeps its
/// shape and is lowered, once, when it is made, so that the low side of its foundations reaches the land: by how much
/// lower the land is under the lowest corner of its box than under its origin, but never by more than a fifth of its
/// size across or 0.8, whichever is deeper. Buildings that bend with the land are not lowered. Pure, for the abode
/// archetype and its tests.
namespace openblack::ecs::abode_foundation
{

/// The model's box seen from above, in its own space. It always reaches the model's origin.
struct GroundBox
{
	glm::vec2 minima;
	glm::vec2 maxima;
};

/// The box of a model from the corners of its vertices' box, stretched to reach its origin
[[nodiscard]] GroundBox GroundBoxOf(glm::vec3 minima, glm::vec3 maxima);

/// How far the building reaches across: the larger of its box's half width and half depth, at its size
[[nodiscard]] float GroundRadius(const GroundBox& box, float scale);

/// The four corners of the box on the land, turned and sized as the building stands at `position` (x, z)
[[nodiscard]] std::array<glm::vec2, 4> GroundCorners(const GroundBox& box, const glm::mat3& rotation, float scale,
                                                     glm::vec2 position);

/// How much lower the land is under the lowest of the corners than under the origin; 0 when none is lower
[[nodiscard]] float DeepestDip(float originHeight, std::span<const float, 4> cornerHeights);

/// The deepest a building of this reach is ever lowered (a negative height)
[[nodiscard]] float SinkLimit(float groundRadius);

/// How far the building is lowered (a negative height, or 0) for the land's dip under its corners
[[nodiscard]] float Sink(float deepestDip, float groundRadius);

/// How far a rigid building is lowered where it stands, with `landHeight(x, z)` giving the land's height
template <typename LandHeight>
    requires std::invocable<LandHeight, glm::vec2>
[[nodiscard]] float FoundationSink(const GroundBox& box, const glm::mat3& rotation, float scale, glm::vec2 position,
                                   LandHeight&& landHeight)
{
	const auto corners = GroundCorners(box, rotation, scale, position);
	std::array<float, 4> heights {};
	for (size_t i = 0; i < corners.size(); ++i)
	{
		heights.at(i) = landHeight(corners.at(i));
	}
	return Sink(DeepestDip(landHeight(position), heights), GroundRadius(box, scale));
}

} // namespace openblack::ecs::abode_foundation
