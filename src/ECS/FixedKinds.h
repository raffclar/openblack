/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <span>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The kinds of things that the game marks as fixed to the land. The mark is on each thing that covers the cells of its
/// outline: the buildings, the fields, the worship sites with their altars and icons, the features and flowers, the
/// rocks, the dead trees, the big forests, the fish farms and the vortices. A tree has no mark, and a thing that moves
/// has no mark.
///
/// Some tests of the game ask for this mark:
/// - A tree or a grazing animal asks only the first thing that stays put in a cell.
/// - The search for a clear place for a temporary pile of a town asks each thing near the place.
/// - A creature that found no way to a thing waits longer before it tries a thing with the mark again.
/// - A villager that lies down by a tree keeps away from a thing with the mark.
namespace openblack::ecs::fixed_kinds
{

/// Whether the game marks a thing as fixed to the land
[[nodiscard]] bool IsFixedKind(const Registry& registry, entt::entity thing);

/// Whether the first of the things that stay put in a cell has the mark. The game does not look at the other things of
/// the cell. A cell with no thing that stays put has no mark.
[[nodiscard]] bool CellIsFixed(std::span<const entt::entity> fixedInCell, const std::function<bool(entt::entity)>& isFixedKind);

} // namespace openblack::ecs::fixed_kinds
