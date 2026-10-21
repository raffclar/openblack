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
#include <span>
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

/// Puts the list in the game's order: the last made first. Two with the same place (things from an old save that keep
/// no place) go by their ids, the larger first.
template <typename Id>
void NewestFirst(std::vector<Living<Id>>& list)
{
	std::ranges::sort(list, [](const Living<Id>& a, const Living<Id>& b) {
		if (a.made != b.made)
		{
			return a.made > b.made;
		}
		return a.id > b.id;
	});
}

/// Walks the list in its order. Each villager and animal does its own turn (ownTurn). The creatures do their turns
/// together (creaturesTurn), at the place of the first creature in the list. When the list has no creature, the creatures'
/// turn comes before all the others, because the creatures' steps also do work that is not for one creature.
template <typename Id, typename CreaturesTurn, typename OwnTurn>
void Walk(std::span<const Living<Id>> list, CreaturesTurn&& creaturesTurn, OwnTurn&& ownTurn)
{
	bool creaturesDone = std::ranges::none_of(list, [](const Living<Id>& living) { return living.kind == Kind::Creature; });
	if (creaturesDone)
	{
		creaturesTurn();
	}
	for (const auto& living : list)
	{
		if (living.kind != Kind::Creature)
		{
			ownTurn(living);
		}
		else if (!creaturesDone)
		{
			creaturesTurn();
			creaturesDone = true;
		}
	}
}

} // namespace openblack::living_turn
