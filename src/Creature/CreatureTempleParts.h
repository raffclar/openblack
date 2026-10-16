/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/MapCells.h"

/// The parts of a temple that a creature sees, believes in and acts on: the temple's heart, its worship sites and the
/// altar at the head of each worship site. The altar is a part of its own, apart from its site. The shipped lands make
/// no creature pen as a thing of its own: the pen is a place on the temple.
namespace openblack::creature_temple_parts
{
/// Whether a worship site's altar is in a map cell. The altar stands in each cell that its outline covers, as a building
/// does.
[[nodiscard]] bool AltarIn(const ecs::map_cells::Outline& outline, glm::ivec2 cell);

/// The things that the eyes of a creature come across in a map cell, in their order. An altar goes to the front of the
/// things that stay put in its cells when its site is made, so the altars in the cell come first. Then come the things
/// that the map keeps in the cell, in the map's order.
[[nodiscard]] std::vector<entt::entity> ThingsSeenInCell(std::span<const entt::entity> altars,
                                                         std::span<const entt::entity> inCell);
} // namespace openblack::creature_temple_parts
