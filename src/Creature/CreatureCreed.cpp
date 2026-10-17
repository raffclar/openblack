/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCreed.h"

#include <array>

using namespace openblack;

std::optional<creature_creed::GlowChange> creature_creed::FromAnimationEvent(int32_t event)
{
	if (event < k_FirstGlowEvent || event > k_LastGlowEvent)
	{
		return std::nullopt;
	}
	constexpr std::array<size_t, 3> k_GlowsInOrder {0, 2, 1};
	constexpr std::array<float, 5> k_Levels {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
	const auto place = static_cast<size_t>(event - k_FirstGlowEvent);
	const auto level = k_Levels.at(place % k_Levels.size());
	return GlowChange {.glow = k_GlowsInOrder.at(place / k_Levels.size()), .scale = 1.0f, .power = level + level};
}

void creature_creed::Set(ecs::components::CreatureCreedPowers& powers, size_t glow, float scale, float power)
{
	if (glow >= powers.glows.size())
	{
		return;
	}
	powers.glows.at(glow) = power == 0.0f ? ecs::components::CreatureCreedPowers::Glow {}
	                                      : ecs::components::CreatureCreedPowers::Glow {.scale = scale, .power = power};
}
