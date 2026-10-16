/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DMesh.h"

/// The things a building puts round itself: a tip sign at the sign point of its model, when its row has a tip. The
/// building makes them when it is made, built or not, and they go when the building goes.
namespace openblack::ecs::abode_surroundings
{

/// The kind of effect point where a building puts its tip sign
inline constexpr uint32_t k_TipSignPoint = 1;

/// The first of the model's points of a kind, in the model's space; none when the model has no such point
[[nodiscard]] std::optional<glm::vec3> FindPoint(std::span<const graphics::L3DMesh::EffectPoint> points, uint32_t kind);

/// A building puts a tip sign up only when its row has a tip text
[[nodiscard]] constexpr bool HasTipSign(uint32_t tipText)
{
	return tipText != 0;
}

/// Where a point of the model stands in the world, by the building's placement (its position, turn and scale)
[[nodiscard]] glm::vec3 PlacePoint(const glm::mat4& placement, glm::vec3 point);

/// Whether two places are in the same cell of the land's map, across the land
[[nodiscard]] bool InSameCell(glm::vec3 a, glm::vec3 b);

/// A building is made: it puts up its tip sign, when it has one
void Create(entt::entity abode);

/// A building goes: every tip sign in the map cell of its sign point goes too, when its row has a tip
void Delete(entt::entity abode);

} // namespace openblack::ecs::abode_surroundings
