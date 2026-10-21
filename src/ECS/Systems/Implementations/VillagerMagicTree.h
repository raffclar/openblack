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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager bewildered by the trees of a forest miracle (see magic_tree_watch). It walks to a tree, turns to the next
/// tree and looks at it, then walks on, until it has seen enough trees. The state functions are the living action
/// system's.
namespace openblack::ecs::villager_magic_tree
{

/// A villager takes notice of a tree of a forest miracle
void Setup(components::LivingAction& action, entt::entity tree);

uint32_t InitialiseBewilderedByMagicTreeReaction(components::LivingAction& action);
uint32_t PerformBewilderedByMagicTreeReaction(components::LivingAction& action);
uint32_t TurnToFaceMagicTree(components::LivingAction& action);
uint32_t LookAtMagicTree(components::LivingAction& action);

} // namespace openblack::ecs::villager_magic_tree
