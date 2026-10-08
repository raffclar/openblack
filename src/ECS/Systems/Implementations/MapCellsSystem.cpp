/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MapCellsSystem.h"

#include <cstddef>

#include <utility>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;
namespace map_coords = openblack::map_coords;
using map_cells::lists::Cell;
using map_cells::lists::Link;
using map_cells::lists::ReadFilter;

namespace
{
/// The place of a cell on the map (InBounds) in the cells, x outer and z inner
std::size_t IndexOf(glm::ivec2 cell)
{
	return static_cast<std::size_t>(cell.x) * map_coords::k_MapCells + static_cast<std::size_t>(cell.y);
}
} // namespace

Cell* MapCellsSystem::CellAt(glm::ivec2 cell)
{
	if (!map_coords::InBounds(cell))
	{
		return nullptr;
	}
	if (_cells.empty())
	{
		_cells.resize(static_cast<std::size_t>(map_coords::k_MapCells) * map_coords::k_MapCells);
	}
	return &_cells[IndexOf(cell)];
}

const Cell* MapCellsSystem::CellIfAny(glm::ivec2 cell) const
{
	if (!map_coords::InBounds(cell) || _cells.empty())
	{
		return nullptr;
	}
	return &_cells[IndexOf(cell)];
}

std::span<const Cell> MapCellsSystem::Cells() const
{
	return _cells;
}

Link* MapCellsSystem::LinkOf(entt::entity object)
{
	const auto index = static_cast<std::size_t>(entt::to_entity(object));
	if (index >= _index.size())
	{
		return nullptr; // no entity of this index has been linked
	}
	const auto& slot = _index[index];
	if (slot.link != nullptr && slot.entity == object)
	{
		return slot.link;
	}
	if (!slot.shared)
	{
		return nullptr; // the slot's entity, if any, is the only linked one of this index
	}
	const auto found = _links.find(object);
	return found != _links.end() ? &found->second : nullptr;
}

Link& MapCellsSystem::AddLink(entt::entity object, Link link)
{
	const auto [found, inserted] = _links.emplace(object, std::move(link));
	if (inserted)
	{
		const auto index = static_cast<std::size_t>(entt::to_entity(object));
		if (index >= _index.size())
		{
			_index.resize(index + 1);
		}
		auto& slot = _index[index];
		if (slot.link == nullptr)
		{
			slot.entity = object;
			slot.link = &found->second;
		}
		else
		{
			slot.shared = true; // another version of this index is still linked: the map answers for this one
		}
	}
	return found->second;
}

void MapCellsSystem::RemoveLink(entt::entity object)
{
	_links.erase(object);
	const auto index = static_cast<std::size_t>(entt::to_entity(object));
	if (index < _index.size() && _index[index].link != nullptr && _index[index].entity == object)
	{
		_index[index].entity = entt::null;
		_index[index].link = nullptr; // a shared slot stays shared: another version may still be in the map
	}
}

const std::unordered_map<entt::entity, Link>& MapCellsSystem::Links() const
{
	return _links;
}

void MapCellsSystem::Clear()
{
	_cells.clear();
	_links.clear();
	_index.clear();
}

void MapCellsSystem::UseRegistry(const Registry* registry)
{
	if (registry != _registry)
	{
		Clear();
		_registry = registry;
	}
}

void MapCellsSystem::BeginReadBatch()
{
	if (_batchDepth++ == 0)
	{
		_batchFilter.emplace();
	}
}

void MapCellsSystem::EndReadBatch()
{
	if (--_batchDepth == 0)
	{
		_batchFilter.reset();
	}
}

const ReadFilter* MapCellsSystem::BatchFilter() const
{
	return _batchFilter ? &*_batchFilter : nullptr;
}
