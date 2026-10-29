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

namespace openblack::ecs
{
class Registry;
}

/// What the scripts' poison functions do to a single thing, and what they read from it
namespace openblack::ecs::script_poison
{

/// Whether a thing is poisoned. A villager, an animal or a creature has the mark. A pile or a handful holds poisoned
/// food. A storage pit has poison in its food pile or in any of its wood piles. A toadstool is always poisoned. Any other
/// thing is not.
[[nodiscard]] bool IsPoisoned(const Registry& registry, entt::entity thing);

/// What a script's value does to a thing. A living thing, a pile and a handful take the lowest bit of the value, so they
/// can be cured. A storage pit poisons its food pile when the value is not 0, if it has the pile, and it never cures
/// anything: the pile keeps the poison it has. Any other thing does nothing.
void SetPoisoned(Registry& registry, entt::entity thing, uint32_t value);

} // namespace openblack::ecs::script_poison
