/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>

/// Villagers turned into walking skeletons by scripts (the sculptor's death on the first land, the singing stones
/// bringing the dead back, the undead town): a skeleton wears the skeleton's model at every distance, keeps it when it
/// grows up or is made young, and gives birth to skeletons. It otherwise lives, walks and works as any villager.
namespace openblack::villager_skeleton
{

/// Makes the villager a skeleton, or its own self again, wearing the models that go with it
void Set(entt::entity villager, bool skeleton);

/// Whether the villager is a skeleton
[[nodiscard]] bool Is(entt::entity villager);

/// The models the villager wears at its three distances for its age: the skeleton's while it is one
[[nodiscard]] std::array<entt::id_type, 3> ModelsFor(entt::entity villager, bool child);

} // namespace openblack::villager_skeleton
