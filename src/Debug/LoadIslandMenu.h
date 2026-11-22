/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Common/DeveloperTools.h"

namespace openblack::debug::gui
{

/// What the Load Island menu does on this computer now: from the network session and the editor claim of the player
[[nodiscard]] developer_tools::LandMenuAct CurrentLandMenuAct() noexcept;

/// Draws the items of the Load Island menu inside an open menu or pop-up: the story islands, the playground islands,
/// the creature testbed and the map for 32 gods. The debug menu bar and the Land tools tab of the editor show the same
/// items. A click loads the land on this computer, or asks the game server to start the room again on it.
void DrawLoadIslandItems(developer_tools::LandMenuAct act) noexcept;

} // namespace openblack::debug::gui
