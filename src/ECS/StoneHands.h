/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
namespace systems
{
class ObjectDrawListSystemInterface;
}

namespace stone_hands
{

/// Whether a village centre shows its stone hands in this frame, with their seeds and their charge looks. The centre
/// must stand in a town, the land's draw list must draw it, and it must have life. Without a draw list, the list does
/// not hide the centre.
[[nodiscard]] bool CentreShowsHands(const Registry& registry, const systems::ObjectDrawListSystemInterface* drawList,
                                    entt::entity centre);

} // namespace stone_hands
} // namespace openblack::ecs
