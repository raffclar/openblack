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

/// MagicTree: a tree the forest miracle made. The entity also has the Tree, Transform, Fixed and Mesh of a normal
/// tree. Magic/Objects/MagicTree.
struct MagicTree
{
	PlayerNames player {PlayerNames::NEUTRAL}; ///< the player number of the spell's player
	float woodValueMultiplier {1.0f};          ///< woodValueMultiplier x the spell's tribal power
};

} // namespace openblack::ecs::components
