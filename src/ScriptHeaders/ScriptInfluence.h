/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

/// What the scripts are told when they ask how much a place is in a player's influence
namespace openblack::script::influence
{

/// The answer to a script's question about a player's influence at a place.
/// - influence: the player's influence there as every rule of the game asks it (what their hand keeps past the border
///   near the hand, else their own); none when there is no such player, who has no influence anywhere.
/// - raw: the script asked for the player's own influence only. Otherwise, where the player has none, they are lent
///   that of the first of their allies, in the players' order, who lets them use it and has some there.
/// - alliesInfluence: the own influence at the place of each ally who lets the player use theirs, in the players' order.
[[nodiscard]] float Answer(std::optional<float> influence, bool raw, std::span<const float> alliesInfluence);

} // namespace openblack::script::influence
