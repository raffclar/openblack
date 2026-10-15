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
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The action with which a creature takes a living thing to the altar of a worship site of its temple, and puts the
/// thing down near the altar. The agenda and the rules here are pure, free of the game.
namespace openblack::creature_sacrifice
{
using creature_mind::Step;

/// The creature stops when it is within this many of its heights of the altar
constexpr float k_AltarHeights = 3.0f;

/// The place around the temple whose worship site the creature goes to: the first place, in the order of the places,
/// that has a site. None when no place has a site.
[[nodiscard]] std::optional<size_t> SitePlace(std::span<const bool> placeHasSite);

/// The agenda of the action:
/// 1. When its hand does not hold a living thing, the creature picks up the thing (pickUp). It looks at the thing as it
///    goes to it, and it shows anger.
/// 2. It walks to within k_AltarHeights of its height of the altar, and looks ahead as it walks.
/// 3. It turns to face the altar, and looks at the altar.
/// 4. It puts the thing down. The thing stays alive.
[[nodiscard]] std::vector<Step> Sacrifice(std::optional<uint32_t> pickUp, glm::vec3 altar, float height);

} // namespace openblack::creature_sacrifice
