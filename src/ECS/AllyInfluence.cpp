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

#include "ECS/GamePlayers.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptInfluence.h"

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
	const float own = Locator::influenceSystem::value().PlayerOwnInfluence(player, place);
	return script::influence::Answer(own, false, LentInfluence(player, place));
}

} // namespace openblack::ecs::ally_influence
