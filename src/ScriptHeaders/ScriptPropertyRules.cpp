/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptPropertyRules.h"

#include <algorithm>

namespace openblack::script::property_rules
{

bool TownCompletelyDestroyed(std::span<const TownBuilding> buildings)
{
	return std::ranges::none_of(buildings, [](const TownBuilding& building) {
		return building.life > 0.0f && !building.field && (building.built >= 1.0f || building.built > k_StandingBuilt);
	});
}

float PlayerProperty(std::optional<PlayerNames> player, bool destroyedTown)
{
	if (destroyedTown)
	{
		return 1.0f;
	}
	if (!player.has_value() || *player == PlayerNames::NEUTRAL)
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(*player) + 1);
}

bool InCreatureHand(entt::entity thing, entt::entity carried, std::optional<entt::entity> eating)
{
	return thing != entt::null && (carried == thing || eating == thing);
}

namespace
{
/// Below the floor gives the floor, up to 1 is kept, anything else (above 1, or not a number) gives 1
float KeepBetween(float value, float floor)
{
	if (value < floor)
	{
		return floor;
	}
	return value <= 1.0f ? value : 1.0f;
}
} // namespace

float SetNeed(CreatureNeed need, float value)
{
	switch (need)
	{
	case CreatureNeed::Warmth:
		return KeepBetween(value, -1.0f);
	case CreatureNeed::Energy:
		return KeepBetween(value, 0.0f);
	default:
		return value;
	}
}

} // namespace openblack::script::property_rules
