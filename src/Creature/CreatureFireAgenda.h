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
#include <vector>

#include "CreatureIdleMind.h"

// What a creature does with fire.
//
// Putting a fire out, it goes right up to the burning thing, backs off to twice its own height, turns to face it and
// casts the water miracle at it from the casting pose, holding it three seconds. Then it douses the thing with five
// times a bucket of water's worth.
//
// Setting something alight, it first picks up the burning thing it uses, unless its hand is full already. It goes up
// close to what it sets alight, turns to face it and tosses the burning thing at it. It waits for the tossed thing to
// be back on the map, then for a second more. Without a burning thing it can't set anything alight.
//
// Pure, tested on its own.

namespace openblack::creature_mind
{
/// Putting a fire out, it goes this near the burning thing first, then backs off to this many times its height
inline constexpr float k_WaterGoNearDistance = 0.0f;
inline constexpr float k_WaterCastHeights = 2.0f;
/// Setting something alight, it goes this near it, tosses with this animation, and waits this long once the tossed
/// thing lands
inline constexpr float k_SetFireDistance = 1.0f;
inline constexpr size_t k_SetFireToss = 97;
inline constexpr float k_SetFireWaitSeconds = 1.0f;

/// Casting the water miracle at a burning object, then dousing it, by its entity's number, by a creature of a height
[[nodiscard]] std::vector<Step> PutOutFireWithWater(uint32_t object, float height);
/// Setting an object alight with a burning thing (the instrument), both by their entity's numbers; none without the
/// instrument
[[nodiscard]] std::optional<std::vector<Step>> SetFireTo(uint32_t object, std::optional<uint32_t> instrument, bool handFull);

} // namespace openblack::creature_mind
