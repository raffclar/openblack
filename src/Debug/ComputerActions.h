/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string_view>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::computer_mind
{
struct Mind;
}

/// A debug tool that makes a computer god do the action of a node of its mind with two things. The tool puts the action
/// in the queue of the god, as a land script does, so the god starts it in its next turn. Only the testbed and the
/// debug tools use this tool. A normal game never does.
namespace openblack::debug::computer_actions
{

/// Makes the mind ready for a script action of the node that the name finds first:
/// - A mind that has no tree yet gets its tree, as the first turn of the mind makes it.
/// - A node with an action and no children chooses itself, as if the god worked it out before.
/// A script can then start the action of the node, also when the needs above the node never let the god work it out.
/// False when no node has the name.
bool MakeReady(computer_mind::Mind& mind, std::string_view name);

/// Puts the action of the named node, with two things, in the queue of the computer god of the player. The god starts it
/// in its next turn when its hand is free. False when the player has no computer mind or no node has the name.
bool Force(PlayerNames player, std::string_view name, entt::entity first, entt::entity second);

/// The mind counts the sends of its creature for the kind of miracle (0 to impress, 1 kind, 2 angry). False when the
/// kind is not one of these.
bool SetUntaughtSends(computer_mind::Mind& mind, size_t kind, uint32_t count);

/// The computer god of the player counts the sends of its creature for the kind of miracle (0 to impress, 1 kind, 2
/// angry), as if it sent the creature that many times while the creature knew no miracle of that kind. False when the
/// player has no computer mind or the kind is not one of these.
bool SetUntaughtSends(PlayerNames player, size_t kind, uint32_t count);

} // namespace openblack::debug::computer_actions
