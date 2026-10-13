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

namespace openblack::ecs::components
{
struct LivingAction;
}

/// The states of a villager that a hunter brought down. First it falls. Then the hunter eats it for a number of turns,
/// and then it dies. The villager keeps the count of these turns only while it stays in the state "being eaten".
namespace openblack::ecs::villager_eaten
{

/// Brought down: the villager plays its fall to the end, then the hunter eats it
uint32_t Downed(components::LivingAction& action);
/// Being eaten: the villager lies still and counts the turns. At the end it dies, or gets up if nothing can kill it
uint32_t BeingEaten(components::LivingAction& action);

} // namespace openblack::ecs::villager_eaten
