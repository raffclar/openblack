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
#include "ECS/Components/Disciple.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

namespace openblack::ecs::town_census
{
namespace
{
using components::Abode;
using components::Disciple;
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

/// What a villager adds to the count of its town now, or nothing when it is not a villager now
std::optional<CountedVillager> CountedNow(const Registry& registry, entt::entity entity)
{
	const auto* villager = registry.Underlying().try_get<const Villager>(entity);
	if (villager == nullptr)
	{
		return std::nullopt;
	}
	const auto* disciple = registry.Underlying().try_get<const Disciple>(entity);
	return CountedVillager {
	    .town = villager->town,
	    .adult = villager->lifeStage != Villager::LifeStage::Child,
	    .disciple = disciple != nullptr ? static_cast<uint8_t>(disciple->type) : uint8_t {0},
	};
}

void AddTo(TownCount& count, const CountedVillager& counted, int32_t sign)
{
	const auto step = static_cast<uint32_t>(sign);
	count.people += step;
	if (counted.adult)
	{
		count.adults += step;
	}
	if (counted.disciple != 0)
	{
		count.disciples.at(counted.disciple) += step;
	}
}

/// Take away what the villager added before, and add what it adds now
void Settle(const Registry& registry, Counts& counts, entt::entity entity)
{
	auto& villagers = counts.villagers;
	const auto now = CountedNow(registry, entity);
	const auto before = villagers.find(entity);
	if (before != villagers.end())
	{
		if (now == before->second)
		{
			return;
		}
		AddTo(counts.towns[before->second.town], before->second, -1);
		villagers.erase(before);
	}
	if (now.has_value())
	{
		AddTo(counts.towns[now->town], *now, 1);
		villagers.emplace(entity, *now);
	}
}

void Recount(const Registry& registry, Counts& counts)
{
	counts.towns.clear();
	counts.villagers.clear();
	counts.changed.clear();
	registry.Each<const Villager>(
	    [&registry, &counts](entt::entity entity, const Villager& /*unused*/) { Settle(registry, counts, entity); });
	counts.recount = false;
}

/// Settle the entities whose component goes. An entity stays in the set while the store still holds its component.
template <typename Component>
void SettleGoing(const Registry& registry, Counts& counts, std::unordered_set<entt::entity>& going)
{
	const auto* store = registry.Underlying().storage<Component>();
	std::erase_if(going, [&](entt::entity entity) {
		// While the store holds the component, the villager adds to its town what it added before
		if (store != nullptr && store->contains(entity))
		{
			return false;
		}
		Settle(registry, counts, entity);
		return true;
	});
}

/// Bring the counts up to date with the villagers that changed
Counts& CountsOf(const Registry& registry)
{
	auto& counts = registry.TownCaches().counts;
	if (counts.resets != registry.GetResets())
	{
		// The entities of an earlier registry state are not the entities of this one
		counts.goingVillagers.clear();
		counts.goingDisciples.clear();
		counts.resets = registry.GetResets();
		counts.recount = true;
	}
	if (counts.recount)
	{
		Recount(registry, counts);
	}
	for (const auto entity : counts.changed)
	{
		Settle(registry, counts, entity);
	}
	counts.changed.clear();
	SettleGoing<Villager>(registry, counts, counts.goingVillagers);
	SettleGoing<Disciple>(registry, counts, counts.goingDisciples);
	return counts;
}

#ifndef NDEBUG
[[nodiscard]] TownCount WalkedCountOf(const Registry& registry, entt::entity town)
{
	TownCount count;
	registry.Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (villager.town == town)
		{
			if (const auto counted = CountedNow(registry, entity); counted.has_value())
			{
				AddTo(count, *counted, 1);
			}
		}
	});
	return count;
}
#endif

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

TownCount CountOf(const Registry& registry, entt::entity town)
{
	const auto& towns = CountsOf(registry).towns;
	const auto found = towns.find(town);
	const auto count = found != towns.end() ? found->second : TownCount {};
	assert(count == WalkedCountOf(registry, town));
	return count;
}

void VillagerChanged(const Registry& registry, entt::entity villager)
{
	registry.TownCaches().counts.changed.insert(villager);
}

void VillagerGoing(const Registry& registry, entt::entity villager)
{
	registry.TownCaches().counts.goingVillagers.insert(villager);
}

void DiscipleGoing(const Registry& registry, entt::entity villager)
{
	registry.TownCaches().counts.goingDisciples.insert(villager);
}

void RecountVillagers(const Registry& registry)
{
	registry.TownCaches().counts.recount = true;
}

bool CountsMatchWalk(const Registry& registry)
{
	const auto& kept = CountsOf(registry).towns;
	std::unordered_map<entt::entity, TownCount> walked;
	registry.Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (const auto counted = CountedNow(registry, entity); counted.has_value())
		{
			AddTo(walked[villager.town], *counted, 1);
		}
	});
	// A town that nobody is in now can keep a count of zero
	return std::ranges::all_of(kept,
	                           [&walked](const auto& entry) {
		                           const auto found = walked.find(entry.first);
		                           return entry.second == (found != walked.end() ? found->second : TownCount {});
	                           }) &&
	       std::ranges::all_of(walked, [&kept](const auto& entry) {
		       const auto found = kept.find(entry.first);
		       return found != kept.end() && found->second == entry.second;
	       });
}

} // namespace openblack::ecs::town_census
