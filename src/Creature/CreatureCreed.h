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

#include "ECS/Components/CreatureCreedPowers.h"

// The glows of the creed on a creature's hands, as the saved game keeps them. A script and the creature's animations
// set a glow's scale and power. A power of 0 puts the glow out, and the glow then keeps no values.
namespace openblack::creature_creed
{

/// The first and the last animation event that sets a glow
inline constexpr int32_t k_FirstGlowEvent = 95;
inline constexpr int32_t k_LastGlowEvent = 109;

/// What an animation event does to a glow
struct GlowChange
{
	size_t glow {0};
	float scale {0.0f};
	float power {0.0f};
};

/// The change of a glow event: five events for each of the glows 0, 2 and 1, in that order, at the levels 0, a quarter,
/// a half, three quarters and all. The power is twice the level, and the scale is 1. None for another event.
[[nodiscard]] std::optional<GlowChange> FromAnimationEvent(int32_t event);

/// Sets a glow's scale and power. A power of 0 puts the glow out. A glow past the four does nothing.
void Set(ecs::components::CreatureCreedPowers& powers, size_t glow, float scale, float power);

} // namespace openblack::creature_creed
