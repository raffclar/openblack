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

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The list of the towns of each player, in the order of the game: a town goes to the end of the list of its player
/// when the land makes it, and again when another player wins it
namespace openblack::ecs::player_towns
{

/// The towns of a player, the first town that the player got first
[[nodiscard]] std::vector<entt::entity> InOrder(const Registry& registry, PlayerNames player);
/// How many towns a player has now
[[nodiscard]] size_t Total(const Registry& registry, PlayerNames player);

/// The towns of all players: the list of each player in turn, from the first player, and the list of the neutral
/// player last
[[nodiscard]] std::vector<entt::entity> AllInOrder(const Registry& registry);

/// The game's own list of all the towns, not a list of a player: the town that the game made last comes first. A new
/// player does not change this order.
[[nodiscard]] std::vector<entt::entity> AllMadeNewestFirst(const Registry& registry);

/// A town and how far it is from a place
struct Distance
{
	entt::entity town;
	float metres;
};

/// The nearest town of all to a place, with no limit: the first town of the list, then each town that is nearer than
/// the best before it. Thus the first of equals stays. None when there is no town.
[[nodiscard]] entt::entity NearestOf(std::span<const Distance> towns);

/// The nearest town to a place of all the towns, however far, in the order of the game's list of all the towns. The
/// distance is the game's map distance between the place and the town.
[[nodiscard]] entt::entity NearestOfAll(const Registry& registry, glm::vec2 place);

/// The nearest town of a player to a place within a reach: each town of the player's list in turn that is at the
/// reach or nearer than the best before it. Thus the last of equals stays. None when no town is near enough.
[[nodiscard]] entt::entity NearestOfPlayer(std::span<const Distance> towns, float reach);

} // namespace openblack::ecs::player_towns
