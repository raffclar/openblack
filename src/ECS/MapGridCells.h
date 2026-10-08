/*******************************************************************************
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

#include <array>
#include <unordered_set>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{

/// One set of entities per map cell. The cells that received an entity since the last clear are listed, so a clear
/// touches only those: the others are already empty, and clearing an empty set does nothing (it keeps its buckets, so
/// the next inserts iterate in the same order as after a clear of every cell)
template <size_t CellCount>
class MapGridCells
{
public:
	void Insert(size_t cell, entt::entity entity)
	{
		auto& set = _cells.at(cell);
		if (set.empty())
		{
			_occupied.push_back(static_cast<uint32_t>(cell));
		}
		set.insert(entity);
	}

	void Clear() noexcept
	{
		for (const auto cell : _occupied)
		{
			_cells[cell].clear();
		}
		_occupied.clear();
	}

	[[nodiscard]] const std::unordered_set<entt::entity>& At(size_t cell) const { return _cells.at(cell); }

	/// The cells listed for the next clear, in the order they were first filled
	[[nodiscard]] const std::vector<uint32_t>& Occupied() const noexcept { return _occupied; }

private:
	std::array<std::unordered_set<entt::entity>, CellCount> _cells;
	std::vector<uint32_t> _occupied;
};

} // namespace openblack::ecs
