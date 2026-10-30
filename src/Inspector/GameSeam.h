/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "GameSeamInterface.h"

namespace openblack::inspector
{

/// The parts of the game on the player's computer that the inspector controls, through the running game
[[nodiscard]] std::unique_ptr<GameSeamInterface> MakeGameSeam();

} // namespace openblack::inspector
