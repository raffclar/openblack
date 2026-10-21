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

#include <span>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::ecs
{

/// What stands in each of the map's cells, kept in the game's order as things come, move and go (see MapCells.h): the
/// things that stay put, then the things that move. Searches of a cell meet them in that order.
class MapInterface: public systems::SavedStateInterface
{
public:
	using CellId = glm::u16vec2;

	static constexpr float k_PositionToGridFactor = static_cast<float>(0x10000) * 0.1f;
	static constexpr glm::u16vec2 k_GridSize = {0x200, 0x200};

	static CellId GetGridCell(const glm::vec2& pos);
	static CellId GetGridCell(const glm::vec3& pos);
	static glm::vec2 GetCellCenter(const CellId& cellId);

	virtual ~MapInterface() = default;

	/// The things that stay put in a cell, newest building first and carried things behind
	[[nodiscard]] virtual std::span<const entt::entity> GetFixedInGridCell(const CellId& cellId) const = 0;
	[[nodiscard]] virtual std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& pos) const = 0;
	/// The things that move in a cell, the last to come into it first
	[[nodiscard]] virtual std::span<const entt::entity> GetMobileInGridCell(const CellId& cellId) const = 0;
	[[nodiscard]] virtual std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& pos) const = 0;
	/// The things filed in a cell so far, for readers outside the game's turn, such as the renderer: these leave the
	/// things made since the last look where they are, so that the game files them when it looks
	[[nodiscard]] virtual std::span<const entt::entity> GetFiledFixedInGridCell(const CellId& cellId) const
	{
		return GetFixedInGridCell(cellId);
	}
	[[nodiscard]] virtual std::span<const entt::entity> GetFiledMobileInGridCell(const CellId& cellId) const
	{
		return GetMobileInGridCell(cellId);
	}
	/// Everything in a cell as a search meets it, none for a cell off the map
	[[nodiscard]] virtual std::vector<entt::entity> GetAllInCell(glm::ivec2 cell) const = 0;
	/// What stands on a land block (by its column and row), in the order the game keeps it for drawing; none for a
	/// block the land hasn't
	[[nodiscard]] virtual std::span<const entt::entity> GetOnBlock(glm::ivec2 block) const = 0;
	/// What stands in cells of no land block, in the same order
	[[nodiscard]] virtual std::span<const entt::entity> GetOffBlocks() const = 0;

	/// Once a game turn, and after the turn's movement: whatever moved to another cell goes into it, as the game moves
	/// things into their new cells
	virtual void Sync() = 0;
	/// A thing moved by a hand, a tool or a miracle goes into the cells it now stands in
	virtual void Refile(entt::entity entity) = 0;
	/// After the load of a saved world: each thing goes back into the cells that it keeps, in the order of the things
	virtual void FileResidents() = 0;
};

} // namespace openblack::ecs
