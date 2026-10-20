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

#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "CreatureIdleMind.h"

// What a creature does about a teleport stone that it walks past. When a jump from the stone saves enough of its walk,
// the creature turns aside into the stone with a plan that the creature must do. It walks to the stone with a puzzled
// face and asks the stone for the jump. When the stone refuses the jump, the plan fails. When the stone takes it, a
// second plan that the creature must do continues: the stone carries the creature to the far stone, the creature walks
// on to where it was going, and then the stone no longer controls it. Pure agendas, tested on their own.

namespace openblack::creature_mind
{

/// The action of the game's table that takes the creature to the stone
inline constexpr std::string_view k_GoToTeleportAction = "GoToTeleport";
/// The action of the game's table that the creature does after the jump
inline constexpr std::string_view k_AfterTeleportAction = "MoveToPos";

/// The walk to the stone and the jump from it. The point of the stone is its map position in metres. The jump is the
/// main step.
[[nodiscard]] std::vector<Step> GoToTeleport(uint32_t stone, glm::vec2 stonePoint);

/// The steps after the jump: the stone carries the creature, the creature walks to where it was going (its map position
/// in metres), and the stone no longer controls the creature. With no destination, there is no walk: the area that
/// holds the creature refused the walk.
[[nodiscard]] std::vector<Step> AfterTeleport(std::optional<glm::vec2> destination);

} // namespace openblack::creature_mind
