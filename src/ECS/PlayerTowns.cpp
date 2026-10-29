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
#include <tuple>
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

std::vector<entt::entity> player_towns::AllInOrder(const Registry& registry)
{
	struct Entry
	{
		PlayerNames owner;
		uint32_t gained;
		entt::entity town;
	};
	std::vector<Entry> towns;
	registry.Each<const components::Town>([&](entt::entity entity, const components::Town& town) {
		towns.push_back({.owner = town.owner, .gained = town.gained, .town = entity});
	});
	// The neutral player has the last number, so its list comes after the lists of all the other players
	static_assert(PlayerNames::NEUTRAL == static_cast<PlayerNames>(static_cast<uint8_t>(PlayerNames::_COUNT) - 1));
	std::ranges::stable_sort(
	    towns, [](const Entry& a, const Entry& b) { return std::tie(a.owner, a.gained) < std::tie(b.owner, b.gained); });
	std::vector<entt::entity> result;
	result.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(result), &Entry::town);
	return result;
}

} // namespace openblack::ecs
