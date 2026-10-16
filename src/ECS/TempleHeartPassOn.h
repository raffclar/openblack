/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Physics/TempleHeart.h"

namespace openblack::ecs::physics_hooks
{
class World;
}

/// A temple's heart and its worship sites give the harm that comes to them to the buildings and homeless people of the
/// heart's player. Only a temple with nothing to give the harm to takes it.
namespace openblack::ecs::temple_heart_pass_on
{

/// The heart chooses what takes an effect, a blow or heat that comes to it. When the effect breaks buildings, the heart
/// fires a beam at what it chose, or at a point on itself, at most once every two seconds.
physics::temple_heart::Target PassOn(physics_hooks::World& world, entt::entity heart, bool breaksBuildings);

/// What takes an effect that comes to a worship site: the thing its heart chose, but the site itself when the heart
/// chose itself and the site has life left.
[[nodiscard]] entt::entity SiteTaker(entt::entity heartChoice, entt::entity heart, entt::entity site, float siteLife);

/// What takes an effect or heat that comes to an object. A temple's heart and a worship site give it on as above. Every
/// other thing takes it itself.
entt::entity TakerOf(physics_hooks::World& world, entt::entity object, bool breaksBuildings);

} // namespace openblack::ecs::temple_heart_pass_on
