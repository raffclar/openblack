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

namespace openblack::ecs::components
{

/// A lantern of a town or of the country, whose crackle is heard while it is dark (its SoundTag). It takes miracles and
/// fire by its row of the statics' table, and its light goes with it.
struct StreetLantern
{
	/// A country lantern on a campfire, rather than a town's lantern on a post
	bool country;
	/// Its row of the statics' table
	MobileStaticInfo info {MobileStaticInfo::StreetLantern};
};

} // namespace openblack::ecs::components
