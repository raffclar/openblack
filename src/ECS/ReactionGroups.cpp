/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReactionGroups.h"

#include <cassert>

#include <algorithm>

#include "ECS/Components/LivingReaction.h"
#include "ECS/Registry.h"

namespace openblack::ecs::reaction_groups
{
namespace
{
using components::LivingReaction;

/// What a living thing adds to the counts now, or nothing when it has no reaction component now
std::optional<Held> HeldNow(const Registry& registry, entt::entity entity)
{
	const auto* state = registry.Underlying().try_get<const LivingReaction>(entity);
	if (state == nullptr)
	{
		return std::nullopt;
	}
	return Held {.reaction = state->reaction, .left = state->left};
}

/// Adds a living thing to the count of the group of its reaction, or removes it. A count that becomes zero goes from the
/// map. Each reaction has a new number, thus the map must not keep the numbers of all the old reactions.
void AddTo(std::unordered_map<uint32_t, uint32_t>& members, const Held& held, int32_t sign)
{
	if (held.left == held.reaction)
	{
		return;
	}
	const auto found = members.try_emplace(held.reaction, 0).first;
	found->second += static_cast<uint32_t>(sign);
	if (found->second == 0)
	{
		members.erase(found);
	}
}

/// Take away what the living thing added before, and add what it adds now
void Settle(const Registry& registry, Counts& counts, entt::entity entity)
{
	const auto now = HeldNow(registry, entity);
	const auto before = counts.held.find(entity);
	if (before != counts.held.end())
	{
		if (now == before->second)
		{
			return;
		}
		AddTo(counts.members, before->second, -1);
		counts.held.erase(before);
	}
	if (now.has_value())
	{
		AddTo(counts.members, *now, 1);
		counts.held.emplace(entity, *now);
	}
}

/// Bring the counts up to date with the living things that changed
Counts& CountsOf(const Registry& registry)
{
	auto& counts = registry.ReactionGroupCounts();
	if (counts.resets != registry.GetResets())
	{
		// The entities of an earlier registry state are not the entities of this one
		counts.going.clear();
		counts.resets = registry.GetResets();
		counts.recount = true;
	}
	if (counts.recount)
	{
		counts.members.clear();
		counts.held.clear();
		counts.changed.clear();
		registry.Each<const LivingReaction>(
		    [&registry, &counts](entt::entity entity, const LivingReaction& /*unused*/) { Settle(registry, counts, entity); });
		counts.recount = false;
	}
	for (const auto entity : counts.changed)
	{
		Settle(registry, counts, entity);
	}
	counts.changed.clear();
	const auto* store = registry.Underlying().storage<LivingReaction>();
	std::erase_if(counts.going, [&](entt::entity entity) {
		// While the store holds the component, the living thing adds to the counts what it added before
		if (store != nullptr && store->contains(entity))
		{
			return false;
		}
		Settle(registry, counts, entity);
		return true;
	});
	return counts;
}

std::unordered_map<uint32_t, uint32_t> WalkedMembers(const Registry& registry)
{
	std::unordered_map<uint32_t, uint32_t> members;
	registry.Each<const LivingReaction>([&members](entt::entity /*unused*/, const LivingReaction& state) {
		AddTo(members, Held {.reaction = state.reaction, .left = state.left}, 1);
	});
	return members;
}

#ifndef NDEBUG
uint32_t WalkedMembersOf(const Registry& registry, uint32_t reaction)
{
	const auto walked = WalkedMembers(registry);
	const auto found = walked.find(reaction);
	return found != walked.end() ? found->second : 0;
}
#endif
} // namespace

uint32_t MembersOf(const Registry& registry, uint32_t reaction)
{
	const auto& members = CountsOf(registry).members;
	const auto found = members.find(reaction);
	const auto count = found != members.end() ? found->second : 0;
	assert(count == WalkedMembersOf(registry, reaction));
	return count;
}

void Changed(const Registry& registry, entt::entity living)
{
	registry.ReactionGroupCounts().changed.insert(living);
}

void Going(const Registry& registry, entt::entity living)
{
	registry.ReactionGroupCounts().going.insert(living);
}

void Recount(const Registry& registry)
{
	registry.ReactionGroupCounts().recount = true;
}

bool CountsMatchWalk(const Registry& registry)
{
	const auto& kept = CountsOf(registry).members;
	const auto walked = WalkedMembers(registry);
	// The counts keep no reaction that nobody holds now
	const bool keptMatch = std::ranges::all_of(kept, [&walked](const auto& entry) {
		const auto found = walked.find(entry.first);
		return found != walked.end() && found->second == entry.second;
	});
	const bool walkedMatch = std::ranges::all_of(walked, [&kept](const auto& entry) {
		const auto found = kept.find(entry.first);
		return found != kept.end() && found->second == entry.second;
	});
	return keptMatch && walkedMatch;
}

} // namespace openblack::ecs::reaction_groups
