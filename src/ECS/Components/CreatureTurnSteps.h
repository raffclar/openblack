/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureAudio.h"

namespace openblack::ecs::components
{

/// Where the layers of a creature's body were at the end of the last game turn that a fish puzzle read them. The next
/// turn finds the footsteps that the body made since then, and a step in the water scares the fish of the puzzle.
struct CreatureTurnSteps
{
	creature_audio::Layers last;
};

} // namespace openblack::ecs::components
