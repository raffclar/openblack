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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// The special points of an object's mesh (the L3D extra metrics), in the world: the metric's matrix times the
// object's, and a variant that also gives that matrix's angles. The worship site (its B_WORSHIP mesh), the town
// centre (its icon slots) and the icon mesh use them.

namespace openblack::worship
{
struct SpecialPoint
{
	glm::vec3 position;  ///< world point
	glm::mat3 rotation;  ///< the object's rotation times the metric's
	float yAngle {0.0f}; ///< the Y angle of `rotation` (the original's angle convention: openblack's eulerAngleY(-angle))
};

/// The entity's mesh's extra metric `index` through its Transform, or nullopt without that metric
[[nodiscard]] std::optional<SpecialPoint> GetSpecialPoint(entt::entity entity, int index);

/// How many extra metrics the entity's mesh has (for the worship traces)
[[nodiscard]] size_t ExtraMetricCount(entt::entity entity);

/// The Y angle of a rotation made with eulerAngleY(-angle) (the openblack convention of the script angles)
[[nodiscard]] float YAngleOf(const glm::mat3& rotation);

/// The land's height at a world point (0 without a landscape)
[[nodiscard]] float GroundAt(const glm::vec3& point);
} // namespace openblack::worship
