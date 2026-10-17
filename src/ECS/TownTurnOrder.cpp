/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownTurnOrder.h"

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <utility>

#include "ECS/Components/Town.h"
#include "ECS/Registry.h"

namespace openblack::ecs::town_turns
{

std::vector<entt::entity> TownsInGainedOrder(const Registry& registry, PlayerNames player)
{
	using Gained = std::pair<uint32_t, entt::entity>;
	std::vector<Gained> towns;
	registry.Each<const components::Town>([&towns, player](entt::entity entity, const components::Town& town) {
		if (town.owner == player)
		{
			towns.emplace_back(town.gained, entity);
		}
	});
	std::ranges::sort(towns);
	std::vector<entt::entity> ordered;
	ordered.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(ordered), &Gained::second);
	return ordered;
}

} // namespace openblack::ecs::town_turns
