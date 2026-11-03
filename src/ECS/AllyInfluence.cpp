/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AllyInfluence.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>

#include "ECS/GamePlayers.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::ally_influence
{

std::vector<float> LentInfluence(PlayerNames player, const map_coords::MapCoords& place)
{
	std::vector<float> lent;
	if (!Locator::playerSystem::has_value() || !Locator::influenceSystem::has_value())
	{
		return lent;
	}
	const auto& players = Locator::playerSystem::value();
	const auto& influence = Locator::influenceSystem::value();
	// Every place of a player, the neutral player too, in the order of the numbers
	for (const auto ally : game_players::All())
	{
		if (player_relations::LendsInfluence(players.AllyStrength(player, ally)))
		{
			lent.push_back(influence.PlayerRawInfluence(ally, place));
		}
	}
	return lent;
}

float InfluenceWithAllies(PlayerNames player, const map_coords::MapCoords& place)
{
	if (!Locator::influenceSystem::has_value())
	{
		return 0.0f;
	}
	const auto& influence = Locator::influenceSystem::value();
	const float own = influence.PlayerOwnInfluence(player, place);
	if (own > 0.0f || !Locator::playerSystem::has_value())
	{
		return own > 0.0f ? own : 0.0f;
	}
	// The alliances of the player come from one search. When no alliance lends influence, no ally can lend some here.
	const auto strengths = Locator::playerSystem::value().AllyStrengths(player);
	if (std::ranges::none_of(strengths, [](float strength) { return player_relations::LendsInfluence(strength); }))
	{
		return 0.0f;
	}
	// The rule asks each player, the neutral player too, in the order of the numbers, as the lent influence lists them
	const auto lends = [&strengths](PlayerNames ally) {
		const auto index = static_cast<size_t>(ally);
		return player_relations::LendsInfluence(index < strengths.size() ? strengths.at(index) : 0.0f);
	};
	return OwnOrFirstLent(own, game_players::All(), lends,
	                      [&influence, &place](PlayerNames ally) { return influence.PlayerRawInfluence(ally, place); });
}

} // namespace openblack::ecs::ally_influence
