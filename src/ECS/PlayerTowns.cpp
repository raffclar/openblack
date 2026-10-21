/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerTowns.h"

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <utility>

#include "ECS/Components/Town.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

std::vector<entt::entity> player_towns::InOrder(const Registry& registry, PlayerNames player)
{
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry.Each<const components::Town>([&](entt::entity entity, const components::Town& town) {
		if (town.owner == player)
		{
			towns.emplace_back(town.gained, entity);
		}
	});
	std::ranges::sort(towns, {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<entt::entity> result;
	result.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(result), &std::pair<uint32_t, entt::entity>::second);
	return result;
}

} // namespace openblack::ecs
