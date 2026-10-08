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
#include <unordered_map>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/MapCellsLists.h"

namespace openblack::ecs::systems
{
/// The game's map cells: every cell's two lists, every object's map fields, and the snapshot a read batch shares
/// (ecs::map_cells walks and keeps them through this)
class MapCellsSystemInterface
{
public:
	virtual ~MapCellsSystemInterface() = default;

	/// The cell's lists, every cell made on the first use; null off the map
	[[nodiscard]] virtual map_cells::lists::Cell* CellAt(glm::ivec2 cell) = 0;
	/// The cell's lists; null off the map or while no cell has been made
	[[nodiscard]] virtual const map_cells::lists::Cell* CellIfAny(glm::ivec2 cell) const = 0;
	/// Every cell, x outer and z inner; empty while none has been made
	[[nodiscard]] virtual std::span<const map_cells::lists::Cell> Cells() const = 0;

	/// The object's map fields; null when it is not in the map
	[[nodiscard]] virtual map_cells::lists::Link* LinkOf(entt::entity object) = 0;
	/// Keeps the object's map fields and returns the kept ones (an object already linked keeps its own)
	virtual map_cells::lists::Link& AddLink(entt::entity object, map_cells::lists::Link link) = 0;
	/// The object is no longer in the map
	virtual void RemoveLink(entt::entity object) = 0;
	/// Every object in the map with its fields
	[[nodiscard]] virtual const std::unordered_map<entt::entity, map_cells::lists::Link>& Links() const = 0;

	/// Empties every cell: no objects, no links
	virtual void Clear() = 0;
	/// The registry the lists are read with; another one (or none) empties them first, as its entities are not these
	virtual void UseRegistry(const Registry* registry) = 0;

	/// A read batch opens / closes; they nest, the outermost takes the snapshot the readers share while it lives
	virtual void BeginReadBatch() = 0;
	virtual void EndReadBatch() = 0;
	/// The live batch's snapshot; null without one
	[[nodiscard]] virtual const map_cells::lists::ReadFilter* BatchFilter() const = 0;
};
} // namespace openblack::ecs::systems
