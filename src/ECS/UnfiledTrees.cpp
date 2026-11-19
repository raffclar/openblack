/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UnfiledTrees.h"

#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using components::MapCellResident;
using components::Tree;

namespace
{

/// The trees that are not in the map's cells, kept with the registry
struct UnfiledTreeSet
{
	entt::sparse_set trees;
};

/// Puts the tree into the set or takes it out, by its map cells now
void Sort(entt::registry& registry, entt::entity entity)
{
	auto& trees = registry.ctx().get<UnfiledTreeSet>().trees;
	const auto* resident = registry.try_get<const MapCellResident>(entity);
	const bool unfiled = resident == nullptr || resident->cells.empty();
	if (unfiled && !trees.contains(entity))
	{
		trees.push(entity);
	}
	else if (!unfiled && trees.contains(entity))
	{
		trees.remove(entity);
	}
}

/// A tree that is made, or a tree that comes into the cells or goes into other cells
void OnTreeOrCells(entt::registry& registry, entt::entity entity)
{
	if (registry.all_of<Tree>(entity))
	{
		Sort(registry, entity);
	}
}

/// A tree that goes. When the registry tells this function, the component that goes is still there.
void OnTreeGone(entt::registry& registry, entt::entity entity)
{
	auto& trees = registry.ctx().get<UnfiledTreeSet>().trees;
	if (trees.contains(entity))
	{
		trees.remove(entity);
	}
}

/// A tree that comes out of the cells. When the registry tells this function, the cells that go are still there.
void OnCellsGone(entt::registry& registry, entt::entity entity)
{
	auto& trees = registry.ctx().get<UnfiledTreeSet>().trees;
	if (registry.all_of<Tree>(entity) && !trees.contains(entity))
	{
		trees.push(entity);
	}
}

} // namespace

const entt::sparse_set& UnfiledTrees(Registry& registry)
{
	auto& underlying = registry.Underlying();
	auto& context = underlying.ctx();
	if (const auto* found = context.find<UnfiledTreeSet>(); found != nullptr)
	{
		return found->trees;
	}
	// The first time: the set and its signals stay with the registry, also when it is emptied
	context.emplace<UnfiledTreeSet>();
	for (const auto entity : underlying.view<const Tree>())
	{
		Sort(underlying, entity);
	}
	underlying.on_construct<Tree>().connect<&OnTreeOrCells>();
	underlying.on_destroy<Tree>().connect<&OnTreeGone>();
	underlying.on_construct<MapCellResident>().connect<&OnTreeOrCells>();
	underlying.on_update<MapCellResident>().connect<&OnTreeOrCells>();
	underlying.on_destroy<MapCellResident>().connect<&OnCellsGone>();
	return context.get<UnfiledTreeSet>().trees;
}

} // namespace openblack::ecs
