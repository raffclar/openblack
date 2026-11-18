/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

/// The order of the turn of the living. The game keeps all the creatures, villagers and animals in one list, and each new
/// one goes to the front of it. Each turn, the game walks the list from the front: each living thing does its whole turn
/// before the next one starts. Thus the random numbers that the living draw come in the order of the list, the newest
/// first. The order keeps no game state, so that the game and the tests use it both.
namespace openblack::living_turn
{

enum class Kind : uint8_t
{
	Creature,
	Villager,
	Animal,
};

/// A living thing and its place in the list
template <typename Id>
struct Living
{
	Id id;
	/// How many living things the game made before this one
	uint64_t made;
	Kind kind;
};

/// True when a comes before b in the game's order: the last made first. Two with the same place (things from an old
/// save that keep no place) go by their ids, the larger first.
template <typename Id>
[[nodiscard]] bool ComesBefore(const Living<Id>& a, const Living<Id>& b)
{
	if (a.made != b.made)
	{
		return a.made > b.made;
	}
	return a.id > b.id;
}

/// Puts the list in the game's order (see ComesBefore)
template <typename Id>
void NewestFirst(std::vector<Living<Id>>& list)
{
	std::ranges::sort(list, ComesBefore<Id>);
}

/// The list in the game's order, kept from turn to turn, and the living things that changed since the list was put in
/// order
template <typename Id>
struct Kept
{
	/// The resets of the world when the list was made. Other values mean that the list is old.
	std::optional<uint32_t> resets;
	/// The list must come again from a walk of all living things
	bool rebuild {true};
	std::vector<Living<Id>> list;
	/// The things that came, went or changed their place since the list was put in order
	std::unordered_set<Id> changed;
};

/// Puts the list in order again after a change to some of its things. The list goes out of order only at these things,
/// so the result is the same as a sort of all the living things. live(id, out) adds to out what the thing is now: one
/// entry for each kind that it is, none when it is not a living thing now.
template <typename Id, typename Live>
void Refresh(std::vector<Living<Id>>& list, const std::unordered_set<Id>& changed, Live&& live)
{
	if (changed.empty())
	{
		return;
	}
	std::erase_if(list, [&changed](const Living<Id>& living) { return changed.contains(living.id); });
	const auto kept = static_cast<std::ptrdiff_t>(list.size());
	for (const auto id : changed)
	{
		live(id, list);
	}
	std::sort(list.begin() + kept, list.end(), ComesBefore<Id>);
	std::inplace_merge(list.begin(), list.begin() + kept, list.end(), ComesBefore<Id>);
}

/// Walks the list in its order. Each creature, villager and animal does its whole turn (turn) before the next one
/// starts.
template <typename Id, typename Turn>
void Walk(std::span<const Living<Id>> list, Turn&& turn)
{
	for (const auto& living : list)
	{
		turn(living);
	}
}

} // namespace openblack::living_turn
