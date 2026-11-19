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

/// The trees that are not in the map's cells: those with no MapCellResident, and those whose MapCellResident holds no
/// cell. The land's draw list does not decide whether such a tree is drawn, thus the draw looks at each of them. The
/// set stays current from the registry's signals, so that the draw does not look at every tree in each frame. The set
/// and its signals stay with the registry, also when it is emptied. The first call finds the trees that are already
/// there.
[[nodiscard]] const entt::sparse_set& UnfiledTrees(Registry& registry);

} // namespace openblack::ecs
