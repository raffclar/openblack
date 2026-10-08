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
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The data of the map cells' object lists (ECS/MapCells.h), kept by the map cells system
namespace openblack::ecs::map_cells::lists
{
/// A map cell: the first mobile object and the first fixed one
struct Cell
{
	entt::entity mobile {entt::null};
	entt::entity fixed {entt::null};
};

/// A MultiMapFixed's next object in one cell, and that cell's x and z
struct MultiCellLink
{
	entt::entity next {entt::null};
	int16_t x {0};
	int16_t z {0};
};

/// An object's map fields: the next, whether it is in the fixed list, the mobile list's previous, and for a
/// MultiMapFixed one next per cell
struct Link
{
	InsertKind kind {InsertKind::None};
	ObjectType type {ObjectType::Invalid};
	bool fixedList {false};
	entt::entity next {entt::null};
	entt::entity prev {entt::null};
	glm::ivec2 cell {0};
	std::vector<MultiCellLink> children;
	// what the cells were worked out from (openblack: Sync's move test)
	int32_t x {0};
	int32_t z {0};
	glm::mat3 rotation {1.0f};
	glm::vec3 scale {1.0f};
	entt::id_type mesh {0};
	/// The collide data (none for the Object class), built on insert, so kept here from the same insert
	std::optional<map_collide::Shape> collide;
};

/// What the readers skip: what the original has already taken out of the map (on starting physics, from the hand or
/// not, on the hand's pick-up, on deletion). Taken when it is made (MapCells.cpp)
struct ReadFilter
{
	const Registry* registry {nullptr};
	std::optional<entt::entity> held;
	std::vector<entt::entity> thrown;
	std::vector<entt::entity> flying;

	ReadFilter();
	[[nodiscard]] bool Out(entt::entity object) const;
};
} // namespace openblack::ecs::map_cells::lists
