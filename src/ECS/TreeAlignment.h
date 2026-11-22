/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::systems
{
class AlignmentSystemInterface;
}

/// The change of alignment that a player gets when a tree goes into the land or comes out of it
namespace openblack::ecs::tree_alignment
{

/// The player plants a tree or pulls one up. Each player, also the neutral player, gets the change. A planted tree adds
/// the change and a pulled tree removes it. The alignment of the player damps the change. The history and the waiting
/// alignment of the player then get the damped change.
void TreeMoved(systems::AlignmentSystemInterface& alignment, PlayerNames player, float treePullPutChange, bool planted);

} // namespace openblack::ecs::tree_alignment
