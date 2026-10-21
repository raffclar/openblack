/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/ListPlace.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"

/// The order of the game's lists of things of one kind, as the dances, the forests, the fireflies and the rewards. The
/// game puts each new thing at the front of its list, and each turn it walks the list from the front. A thing that
/// leaves the list does not change the order of the others. Thus each turn takes the things of a kind newest first.
namespace openblack::ecs::list_order
{

/// A thing and its place in its list
struct Entry
{
	entt::entity id;
	/// How many listed things the game made before this one
	uint64_t made;
};

/// Puts the list in the game's order: the last made first. Two with the same place (things from an old save that keep
/// no place) go by their ids, the larger first.
inline void NewestFirst(std::vector<Entry>& list)
{
	std::ranges::sort(list, [](const Entry& a, const Entry& b) {
		if (a.made != b.made)
		{
			return a.made > b.made;
		}
		return a.id > b.id;
	});
}

/// Puts a new thing at the front of the list of its kind
inline void Join(Registry& registry, entt::entity entity)
{
	registry.AssignOrReplace<components::ListPlace>(entity, components::ListPlace {.made = registry.Context().listedMade++});
}

/// The things that have the component, in the order of the game's list of their kind
template <typename Component>
[[nodiscard]] std::vector<entt::entity> Walk(const Registry& registry)
{
	std::vector<Entry> list;
	registry.Each<const Component>([&registry, &list](entt::entity entity, const auto&...) {
		const auto* place = registry.TryGet<const components::ListPlace>(entity);
		list.push_back({.id = entity, .made = place != nullptr ? place->made : 0});
	});
	NewestFirst(list);
	std::vector<entt::entity> order;
	order.reserve(list.size());
	std::ranges::transform(list, std::back_inserter(order), &Entry::id);
	return order;
}

/// Some lists give one turn to one thing each game turn, as the reactions that spread again. The turn goes along the list
/// from the newest to the oldest, then starts again at the newest. The list here is in the order that the things were
/// made, the oldest first. The cursor is the thing that had the last turn.
///
/// Gives the place in the list of the thing that has this turn. It is the next older thing after the cursor. It is the
/// newest thing when there is no cursor, or when the cursor is the oldest thing or is not in the list. There is no place
/// when the list is empty.
template <typename Id>
[[nodiscard]] std::optional<size_t> NextInTurn(std::span<const Id> oldestFirst, std::optional<Id> cursor)
{
	if (oldestFirst.empty())
	{
		return std::nullopt;
	}
	if (cursor.has_value())
	{
		const auto at = std::ranges::find(oldestFirst, *cursor);
		if (at != oldestFirst.end() && at != oldestFirst.begin())
		{
			return static_cast<size_t>(at - oldestFirst.begin()) - 1;
		}
	}
	return oldestFirst.size() - 1;
}

/// Gives the cursor after a thing leaves the list. When the thing is the cursor, the cursor moves to the next older thing,
/// or to no thing when it was the oldest. Thus the next turn does not go to that next older thing, but to the thing after
/// it.
template <typename Id>
[[nodiscard]] std::optional<Id> CursorAfterLeave(std::span<const Id> oldestFirst, std::optional<Id> cursor, Id leaves)
{
	if (cursor != leaves)
	{
		return cursor;
	}
	const auto at = std::ranges::find(oldestFirst, leaves);
	if (at == oldestFirst.end() || at == oldestFirst.begin())
	{
		return std::nullopt;
	}
	return *(at - 1);
}

} // namespace openblack::ecs::list_order
