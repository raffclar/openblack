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

/// A town's graveyard: the burials it has counted, which decide the graves shown on its model
struct Graveyard
{
	uint32_t burials {0};
};

/// A civic building standing in as its town's graveyard that a burial has been counted on, as in the original game: the
/// count it keeps (a totem keeps it from burial to burial) and how many of its model's numbered parts it shows, which is
/// its scaffold over its finished model
struct StandInGraves
{
	uint32_t count {0};
	uint32_t partsShown {0};
};

/// A villager made into a walking skeleton by a script: it wears the skeleton's model at every distance and goes about
/// its life as any other villager, but predators pass it by
struct WalkingSkeleton
{
};

} // namespace openblack::ecs::components
