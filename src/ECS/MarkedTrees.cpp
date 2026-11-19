/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MarkedTrees.h"

#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using components::Tree;

namespace
{

/// The trees that can hold a slot, kept with the registry
struct MarkedTreeSet
{
	entt::sparse_set trees;
};

/// A tree that is made or replaced can come with a slot, as after a load
void OnTreeMade(entt::registry& registry, entt::entity entity)
{
	auto& trees = registry.ctx().get<MarkedTreeSet>().trees;
	if (!trees.contains(entity))
	{
		trees.push(entity);
	}
}

/// A tree that goes. When the registry tells this function, the component that goes is still there.
void OnTreeGone(entt::registry& registry, entt::entity entity)
{
	auto& trees = registry.ctx().get<MarkedTreeSet>().trees;
	if (trees.contains(entity))
	{
		trees.remove(entity);
	}
}

} // namespace

entt::sparse_set& MarkedTrees(Registry& registry)
{
	auto& underlying = registry.Underlying();
	auto& context = underlying.ctx();
	if (auto* found = context.find<MarkedTreeSet>(); found != nullptr)
	{
		return found->trees;
	}
	// The first time: the set and its signals stay with the registry, also when it is emptied
	auto& trees = context.emplace<MarkedTreeSet>().trees;
	for (const auto entity : underlying.view<const Tree>())
	{
		trees.push(entity);
	}
	underlying.on_construct<Tree>().connect<&OnTreeMade>();
	underlying.on_update<Tree>().connect<&OnTreeMade>();
	underlying.on_destroy<Tree>().connect<&OnTreeGone>();
	return trees;
}

} // namespace openblack::ecs
