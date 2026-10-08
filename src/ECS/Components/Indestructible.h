/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// The indestructible flag (GAME_THING_WITH_POS_FLAG_INDESTRUCTIBLE): set and cleared by the script's
/// SET_INDESTRUCTABLE (for objects that are not script containers), the puzzle objects (puzzle game, Hanoi block,
/// puzzle pig) and loading a saved instance. It stops death by losing life and destruction by spells, and holds a
/// drowning villager's counter at 10: he never drowns.
struct Indestructible
{
	int dummy;
};

} // namespace openblack::ecs::components
