/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCensus.h"

#include <cassert>

#include <algorithm>

#include "ECS/Components/Abode.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

namespace openblack::ecs::town_census
{
namespace
{
using components::Abode;
using components::SpellDispenser;
using components::Town;
using components::Villager;

template <typename Component>
size_t SizeOf(const Registry& registry)
{
	const auto* storage = registry.Underlying().storage<Component>();
	return storage != nullptr ? storage->size() : 0;
}

/// Empty each list but keep its memory, so that the next fill does not ask for memory again
template <typename Key>
void Empty(std::unordered_map<Key, std::vector<entt::entity>>& lists)
{
	for (auto& [key, list] : lists)
	{
		list.clear();
	}
}

const Members& MembersOf(const Registry& registry)
{
	auto& members = registry.TownCaches().members;
	const MembersKey key {
	    .resets = registry.GetResets(),
	    .changes = registry.GetTownMemberChanges(),
	    .villagers = SizeOf<Villager>(registry),
	    .abodes = SizeOf<Abode>(registry),
	    .dispensers = SizeOf<SpellDispenser>(registry),
	};
	if (members.key == key)
	{
		return members;
	}
	// A load can give an entity the same id as before, so the lists of an earlier registry state go
	if (!members.key.has_value() || members.key->resets != key.resets)
	{
		members = {};
	}
	Empty(members.people);
	Empty(members.abodes);
	Empty(members.dispensers);
	registry.Each<const Villager>(
	    [&members](entt::entity entity, const Villager& villager) { members.people[villager.town].push_back(entity); });
	registry.Each<const Abode>(
	    [&members](entt::entity entity, const Abode& abode) { members.abodes[abode.townId].push_back(entity); });
	registry.Each<const SpellDispenser>([&members](entt::entity entity, const SpellDispenser& dispenser) {
		members.dispensers[dispenser.town].push_back(entity);
	});
	members.key = key;
	return members;
}

template <typename Key>
std::span<const entt::entity> ListOf(const std::unordered_map<Key, std::vector<entt::entity>>& lists, Key key)
{
	const auto found = lists.find(key);
	return found != lists.end() ? std::span<const entt::entity>(found->second) : std::span<const entt::entity> {};
}

#ifndef NDEBUG
/// The debug builds make sure that the list is the same as a walk of the world finds
template <typename Component, typename Test>
bool SameAsWalk(const Registry& registry, std::span<const entt::entity> list, Test test)
{
	std::vector<entt::entity> walked;
	registry.Each<const Component>([&](entt::entity entity, const Component& component) {
		if (test(component))
		{
			walked.push_back(entity);
		}
	});
	return std::ranges::equal(walked, list);
}
#endif
} // namespace

std::span<const entt::entity> PeopleOf(const Registry& registry, entt::entity town)
{
	const auto list = ListOf(MembersOf(registry).people, town);
	assert((SameAsWalk<Villager>(registry, list, [town](const Villager& villager) { return villager.town == town; })));
	return list;
}

std::span<const entt::entity> AbodesOf(const Registry& registry, uint32_t townNumber)
{
	const auto list = ListOf(MembersOf(registry).abodes, townNumber);
	assert((SameAsWalk<Abode>(registry, list, [townNumber](const Abode& abode) { return abode.townId == townNumber; })));
	return list;
}

std::span<const entt::entity> DispensersOf(const Registry& registry, entt::entity town)
{
	const auto list = ListOf(MembersOf(registry).dispensers, town);
	assert((SameAsWalk<SpellDispenser>(registry, list,
	                                   [town](const SpellDispenser& dispenser) { return dispenser.town == town; })));
	return list;
}

entt::entity TownByNumber(const Registry& registry, uint32_t townNumber)
{
	auto& towns = registry.TownCaches().towns;
	const TownsKey key {
	    .resets = registry.GetResets(),
	    .changes = registry.GetTownChanges(),
	    .towns = SizeOf<Town>(registry),
	};
	if (towns.key != key)
	{
		towns.byNumber.clear();
		// A later town of the walk with the same number takes the place of an earlier one, as the walk keeps the last
		registry.Each<const Town>(
		    [&towns](entt::entity entity, const Town& town) { towns.byNumber.insert_or_assign(town.id, entity); });
		towns.key = key;
	}
	const auto found = towns.byNumber.find(townNumber);
	const auto town = found != towns.byNumber.end() ? found->second : static_cast<entt::entity>(entt::null);
#ifndef NDEBUG
	entt::entity walked = entt::null;
	registry.Each<const Town>([&walked, townNumber](entt::entity entity, const Town& candidate) {
		if (candidate.id == townNumber)
		{
			walked = entity;
		}
	});
	assert(walked == town);
#endif
	return town;
}

entt::entity TownBySignedNumber(const Registry& registry, int32_t townNumber)
{
	// The cast is the same both ways, so the town number reads as this value only for this number
	return TownByNumber(registry, static_cast<uint32_t>(townNumber));
}

} // namespace openblack::ecs::town_census
