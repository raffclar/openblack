/*******************************************************************************
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

/// A named villager: one of the game's table of named people, whose name floats over its head
struct NamedVillager
{
	/// Its row in the table
	uint8_t row {0};
};

/// The player asked for this living thing's details with the Show Villager Details key while the hand was over it;
/// a villager shows them over its head until asked again
struct DetailsShown
{
	char dummy {0};
};

/// A script made the object: a named villager made so never shows its name
struct NameHidden
{
	char dummy {0};
};

} // namespace openblack::ecs::components
