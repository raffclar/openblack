/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Physics/TempleHeart.h"

namespace openblack::ecs
{
class Registry;
}

/// The towns a temple's heart looks over for something to pass a blow on to
namespace openblack::ecs::temple_heart_towns
{

/// What the gathering asks of the game about a building or a villager
struct Lookups
{
	std::function<float(entt::entity)> lifeOf;
	std::function<bool(entt::entity)> footballPitch;
	std::function<bool(entt::entity)> dying;
};

/// The player's towns in the order the player gained them, each with its buildings and homeless people, the newest first
[[nodiscard]] std::vector<physics::temple_heart::Town> Gather(Registry& registry, PlayerNames owner, const Lookups& lookups);

} // namespace openblack::ecs::temple_heart_towns
