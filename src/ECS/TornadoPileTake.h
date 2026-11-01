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

#include "Enums.h"

namespace openblack::ecs::systems
{
class ResourceStoreSystemInterface;
}

namespace openblack::ecs::tornado_pile
{

/// A tornado or a vortex takes a part of a pile into a new pot. The pile says how much it holds, as a script asks it: a
/// pile of a store gives the count of its store. The tornado takes the smaller of the amount that it asks and that
/// count. The pile then gives that amount, as a villager takes from it, with nobody's taking counted. Thus a pile of a
/// store stays when it is empty, and a pile on its own goes. The new pot holds the amount that the tornado asked of the
/// pile, also when the pile gave less. The amount for the pot, 0 when the pile holds nothing.
[[nodiscard]] uint32_t Take(systems::ResourceStoreSystemInterface& stores, entt::entity pile, ResourceType resource,
                            uint32_t asked);

} // namespace openblack::ecs::tornado_pile
