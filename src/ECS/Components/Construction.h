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

#include "Enums.h"

namespace openblack::ecs::components
{

/// A temple a town plans to build for its player, not yet started: nothing of it stands, it isn't drawn and it is no
/// temple yet. A script starts it, and the temple then goes up where its Transform places it.
struct PlannedTemple
{
	int32_t townId;
	PlayerNames owner;
	/// How far round the temple will be turned, as the game measures it
	float yAngle {0.0f};
};

/// A building going up with a site for its builders: how much the town wants it built, which a script may force
struct BuildingSite
{
	float desire {0.0f};
};

} // namespace openblack::ecs::components
