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

namespace openblack::creature
{
/// The player at this machine's interface: whose creature the hand leads, whose fights the camera watches and the
/// fight panel shows. It is the player service's local player (PlayerSystemInterface::LocalPlayer), the first player
/// without that service.
[[nodiscard]] PlayerNames LocalPlayer();
} // namespace openblack::creature
