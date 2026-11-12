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

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/Registry.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct Pot;
}

namespace openblack::ecs::pot_call
{

/// Whether a pot calls the living to it now. The pot calls when its kind has a call, and it did not call before. When it
/// called before, it calls again only after the removal of its call. The end of the time of a call does not remove it.
[[nodiscard]] constexpr bool Calls(bool calling, Reaction call)
{
	return !calling && call != Reaction::None;
}

/// The player of the call of a pot that no store keeps: the player of a pile that magic made, else the neutral player. A
/// loose pot and a handful that the hand takes from a pile belong to no player.
[[nodiscard]] PlayerNames FreePlayerOf(const ecs::Registry& registry, entt::entity pot);

/// The pot calls the living to it with the reaction of its call, when it can. Its other reactions then also stop the
/// living that react to them when the pot goes. Gives the id of the new reaction, or nothing when the pot does not call.
std::optional<uint32_t> Make(entt::entity pot, components::Pot& data, systems::ReactionSystemInterface& reactions,
                             const systems::ReactionSystemInterface::Source& call);

/// The call of the pot is removed, as when a hand takes the pot, when it catches fire, or when it is emptied. When the
/// pot called before, all the reactions of the pot end, of each kind, and the living that react to them stop.
void Remove(entt::entity pot, components::Pot& data, systems::ReactionSystemInterface& reactions);

} // namespace openblack::ecs::pot_call
