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

/// A street lantern of the map script or of CHL CREATE. Made with any mobile static row but the 8th (index 7): a
/// country lantern (MSH_B_CAMPFIRE) rather than a town one (MSH_O_TOWNLIGHT).
struct StreetLantern
{
	bool country {false};
};

/// A village light (night_lights): the flames, the glow and the light stamped on the land.
/// type 0 = town light (5 units up), 1 = country lantern (1 unit up). A street lantern keeps one; the two lamps
/// of the Norse Gate (an animated static) have one each.
struct LanternLight
{
	uint8_t type {0};
};

} // namespace openblack::ecs::components
