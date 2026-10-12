/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>

#include "Creature/CreatureNames.h"
#include "Enums.h"

/// Where the game finds what a creature is named with: who it belongs to, the help texts' words and the profile being
/// played
namespace openblack::ecs::creature_naming
{

/// Who a creature of an owner belongs to, as far as its name goes. The scripts' creatures belong to no one (the neutral
/// player).
[[nodiscard]] creature_names::Owner OwnerOf(PlayerNames owner);

/// The words of a help text; number 0 and numbers past the last give the first text's. Empty before the texts are read.
[[nodiscard]] std::u16string HelpWords(uint32_t text);

/// The creature name of the profile being played, none without a profile
[[nodiscard]] std::optional<std::u16string> ProfileCreatureName();

/// The name a creature of an owner is given as it is made, with the creatures there are counting it
[[nodiscard]] std::u16string NewName(PlayerNames owner, size_t creatures);

} // namespace openblack::ecs::creature_naming
