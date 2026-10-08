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

/// How good or evil a player is, from -1, evil, to 1, good (GAlignment +0x8)
struct Alignment
{
	float value {0.0f};
	/// A change still to come, as what the player's miracles did to the world moves them: it is let through a little
	/// each turn
	float pending {0.0f};
};

} // namespace openblack::ecs::components
