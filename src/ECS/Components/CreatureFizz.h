/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureFizz.h"

namespace openblack::ecs::components
{

/// A creature fizzing out of sight or back in, or fizzed out. It goes once the creature is back in full sight. While it
/// is there, how far the creature is drawn fizzed comes from it.
struct CreatureFizz
{
	creature_fizz::Fizz fizz;
};

} // namespace openblack::ecs::components
