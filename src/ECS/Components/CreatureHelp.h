/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureHelp.h"

namespace openblack::ecs::components
{

/// What a creature remembers of the help's messages about it: those told and when, those waiting, its prompts and how
/// steadily the hand has been over it (see creature_help)
struct CreatureHelpMemory
{
	creature_help::Memory memory;
};

} // namespace openblack::ecs::components
