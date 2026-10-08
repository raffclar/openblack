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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{
/// What a town's clear area check reads of the map: the objects of a map cell and the 2D radius of each
class TownCellObjectsInterface
{
public:
	virtual ~TownCellObjectsInterface() = default;

	/// The objects of one map cell, the fixed list first and then the mobile one
	[[nodiscard]] virtual std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell) const = 0;
	/// The object's 2D radius
	[[nodiscard]] virtual float Get2DRadius(entt::entity object) const = 0;
};
} // namespace openblack::ecs::systems
