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

#include <functional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics::partial_build_cap
{

/// A corner of the cap, in the model's own space
struct CapVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

/// The most segments one primitive's cut may have and still be capped; a cut through more shows no cap at all
inline constexpr size_t k_MostSegments = 500;

/// The cap over a building's model cut at a height, where its walls are cut through: each triangle with corners both at
/// or above the height and below it is cut along the two of its edges that cross it, and that segment on the outer wall
/// is joined to the same segment on the inner wall, which stands in from it across the ground along the corners'
/// normals by the inset. Each segment gives two triangles, outer first then inner, with the texture coordinates and
/// normals taken along the edges. Nothing when nothing is cut, or when more than the most segments are.
[[nodiscard]] std::vector<CapVertex> Build(std::span<const glm::vec3> positions, std::span<const glm::vec2> uvs,
                                           std::span<const glm::vec3> normals, std::span<const uint16_t> indices, float height,
                                           float inset);

/// Whether the cut draws anything of a primitive: any triangle with a corner below the height, which is drawn cut down to
/// the part below it. Only such a primitive has inner walls and a cap. (The game also leaves them out when all it would
/// draw below the cut is off screen; that changes nothing that can be seen.)
[[nodiscard]] bool DrawsAnythingBelow(std::span<const glm::vec3> positions, std::span<const uint16_t> indices, float height);

/// The corners of a model that follows the land, in the model's own space, each moved up by how much higher the land is
/// under it than under the origin of the model, in the units of the model. The outer walls of the model rise this way when
/// it is drawn, and the inner walls keep the rise of their outer corners. The land gives its height at a point.
[[nodiscard]] std::vector<glm::vec3> RisenWithLand(std::span<const glm::vec3> positions, const glm::mat4& placement,
                                                   const std::function<float(glm::vec2)>& landHeight);
} // namespace openblack::graphics::partial_build_cap
