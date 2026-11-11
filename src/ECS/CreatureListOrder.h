/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The order of the game's list of creatures. Each new creature goes to the front of the list. A creature that goes
/// away leaves the others in their order. Thus a walk of the list takes the newest creature first, also after a creature
/// has gone. The store of the creature components does not keep this order when a creature goes, so each walk that
/// must follow the game's list uses this order.
namespace openblack::ecs::creature_list
{

/// All the creatures, the newest first. The place of each creature in the list of the living gives its age. A creature
/// with no place in that list (as the body that the tattoo menu shows) comes last.
[[nodiscard]] std::vector<entt::entity> NewestFirst(const Registry& registry);

} // namespace openblack::ecs::creature_list
