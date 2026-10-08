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

/// The original's raw wall-hug move-state number of a mobile, as the forester's walk to the forest reads it. Nothing
/// in the game sets it for villagers, so it is absent there (read as 0); the forester looks ahead for trees only when
/// it is 2
struct WallHugMoveState
{
	uint8_t value;
};

} // namespace openblack::ecs::components
