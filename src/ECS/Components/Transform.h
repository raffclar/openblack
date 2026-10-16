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

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

struct Transform
{
	glm::vec3 position;
	glm::mat3 rotation;
	// TODO(bwrsandman): Change to float. For all intents and purposes, transform scales uniformly.
	glm::vec3 scale;
};

/// The matrix that draws a thing: scaled, then turned, then moved to its position. The origin goes to the position
/// exactly, also when the turn is not an exact rotation, such as the turn of a hand that holds the thing.
[[nodiscard]] inline glm::mat4 PlacedMatrix(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale)
{
	glm::mat4 matrix(rotation);
	matrix[0] *= scale.x;
	matrix[1] *= scale.y;
	matrix[2] *= scale.z;
	matrix[3] = glm::vec4(position, 1.0f);
	return matrix;
}

inline std::optional<glm::vec3> GetTransformPosition(const Transform& transform)
{
	return transform.position;
};

} // namespace openblack::ecs::components
