/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/sparse_set.hpp>

namespace openblack::ecs
{

class Registry;

/// The trees that can hold the slot of a bend point: each tree that a bend point marks, and each tree that is made or
/// replaced. The set can hold a tree with no slot. The reader removes such a tree when it sees it. Thus the trees with
/// a slot are always in the set, and the frames look at only these trees, not at every tree. The set and its signals
/// stay with the registry, also when it is emptied. The first call puts every tree that is already there into the set.
[[nodiscard]] entt::sparse_set& MarkedTrees(Registry& registry);

} // namespace openblack::ecs
