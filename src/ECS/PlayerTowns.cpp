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
#include <array>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
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

size_t player_towns::Total(const Registry& registry, PlayerNames player)
{
	size_t total = 0;
	registry.Each<const components::Town>([&](entt::entity, const components::Town& town) {
		if (town.owner == player)
		{
			++total;
		}
	});
	return total;
}

namespace
{
/// A town and the key of its place in the order of all the towns
struct OrderEntry
{
	PlayerNames owner;
	uint32_t gained;
	/// The place of the town in the walk, which keeps the order of towns with the same owner and the same gain turn
	uint32_t walked;
	entt::entity town;
};

/// Sorts the towns into the order of all the players' lists, then calls the function with the sorted entries. Up to
/// k_InlineTowns entries stay on the stack.
void SortedTowns(const Registry& registry, const std::function<void(std::span<const OrderEntry>)>& use)
{
	const auto count = registry.Size<const components::Town>();
	std::array<OrderEntry, player_towns::k_InlineTowns> inline_ {};
	std::vector<OrderEntry> onHeap;
	if (count > inline_.size())
	{
		onHeap.resize(count);
	}
	const std::span<OrderEntry> all = count > inline_.size() ? std::span<OrderEntry>(onHeap) : std::span<OrderEntry>(inline_);
	size_t walked = 0;
	registry.Each<const components::Town>([&](entt::entity entity, const components::Town& town) {
		all[walked] = {.owner = town.owner, .gained = town.gained, .walked = static_cast<uint32_t>(walked), .town = entity};
		++walked;
	});
	const auto towns = all.first(walked);
	// The neutral player has the last number, so its list comes after the lists of all the other players
	static_assert(PlayerNames::NEUTRAL == static_cast<PlayerNames>(static_cast<uint8_t>(PlayerNames::_COUNT) - 1));
	// The place in the walk makes each key different. Thus this sort gives the order of a stable sort, with no memory
	// from the heap.
	std::ranges::sort(towns, [](const OrderEntry& a, const OrderEntry& b) {
		return std::tie(a.owner, a.gained, a.walked) < std::tie(b.owner, b.gained, b.walked);
	});
	use(towns);
}
} // namespace

std::vector<entt::entity> player_towns::AllInOrder(const Registry& registry)
{
	std::vector<entt::entity> result;
	SortedTowns(registry, [&result](std::span<const OrderEntry> towns) {
		result.reserve(towns.size());
		std::ranges::transform(towns, std::back_inserter(result), &OrderEntry::town);
	});
	return result;
}

void player_towns::ForEachInOrder(const Registry& registry, const std::function<void(entt::entity)>& visit)
{
	SortedTowns(registry, [&visit](std::span<const OrderEntry> towns) {
		for (const auto& entry : towns)
		{
			visit(entry.town);
		}
	});
}

std::vector<entt::entity> player_towns::AllMadeNewestFirst(const Registry& registry)
{
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry.Each<const components::Town>(
	    [&towns](entt::entity entity, const components::Town& town) { towns.emplace_back(town.made, entity); });
	std::ranges::sort(towns, std::ranges::greater {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<entt::entity> result;
	result.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(result), &std::pair<uint32_t, entt::entity>::second);
	return result;
}

void player_towns::PutFirstInMadeOrder(Registry& registry, std::span<const entt::entity> newestFirst)
{
	// The town made last has the highest stamp, so the last town of the list takes the stamp first
	for (const auto town : newestFirst | std::views::reverse)
	{
		if (auto* component = registry.TryGet<components::Town>(town))
		{
			component->made = registry.Context().nextTownMade++;
		}
	}
}

std::vector<entt::entity> player_towns::AllPlansInOrder(const Registry& registry)
{
	std::vector<entt::entity> plans;
	for (const auto town : AllMadeNewestFirst(registry))
	{
		// The list of a town can keep a plan that has gone or became a building
		std::ranges::copy_if(
		    registry.Get<const components::Town>(town).planned, std::back_inserter(plans), [&registry](entt::entity plan) {
			    return registry.Valid(plan) && registry.AnyOf<components::PlannedAbode, components::PlannedTemple>(plan);
		    });
	}
	return plans;
}

entt::entity player_towns::NearestOf(std::span<const Distance> towns)
{
	const Distance* nearest = nullptr;
	for (const auto& town : towns)
	{
		if (nearest == nullptr || town.metres < nearest->metres)
		{
			nearest = &town;
		}
	}
	return nearest != nullptr ? nearest->town : entt::null;
}

entt::entity player_towns::NearestOfAll(const Registry& registry, glm::vec2 place)
{
	std::vector<Distance> towns;
	for (const auto town : AllMadeNewestFirst(registry))
	{
		if (const auto* transform = registry.TryGet<const components::Transform>(town))
		{
			towns.push_back(
			    {.town = town,
			     .metres = gutils::GetDistanceInMetres(place, glm::vec2(transform->position.x, transform->position.z))});
		}
	}
	return NearestOf(towns);
}

entt::entity player_towns::NearestOfPlayer(std::span<const Distance> towns, float reach)
{
	entt::entity nearest = entt::null;
	float best = reach;
	for (const auto& town : towns)
	{
		if (town.metres <= best)
		{
			best = town.metres;
			nearest = town.town;
		}
	}
	return nearest;
}

} // namespace openblack::ecs
