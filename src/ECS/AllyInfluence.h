/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "3D/MapCoords.h"
#include "Enums.h"

/// The influence that allies lend to a player, from the alliances of the player and the influence of each ally
namespace openblack::ecs::ally_influence
{

/// The own influence at the place of each ally that lends its influence to the player, in the order of the players. It
/// is the influence of the ally's towns, citadel and other sources, without what the ally's hand keeps past the border.
[[nodiscard]] std::vector<float> LentInfluence(PlayerNames player, const map_coords::MapCoords& place);

/// The influence of the player at the place. Where the player has none, it is the influence of the first ally that
/// lends its influence and has some there, else 0.
[[nodiscard]] float InfluenceWithAllies(PlayerNames player, const map_coords::MapCoords& place);

} // namespace openblack::ecs::ally_influence
