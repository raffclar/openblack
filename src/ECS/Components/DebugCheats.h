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

/// The debug cheats of a player that are on (debug_cheats::Cheat), on the entity of the player. Only a command of a turn
/// changes them, so they are the same on each computer of a network game.
struct DebugCheats
{
	/// One bit for each cheat that is on (debug_cheats::BitOf)
	uint32_t on {0};
	/// The infinite miracle charge makes the prayer power of the player infinite, and the player had infinite prayer
	/// power before the cheat
	bool prayerWasInfinite {false};
	/// The infinite miracle charge makes the prayer power of the player infinite now
	bool prayerForced {false};
};

/// The flags that a worship site had before a debug cheat of its player forced them on, on the entity of the site. The
/// site gets these flags back when the cheat goes off, or when the site goes to a player with no cheat.
struct DebugCheatSite
{
	bool infinite {false};
	bool freeMaintenance {false};
};

} // namespace openblack::ecs::components
