/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PotCall.h"

#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Pot.h"

namespace openblack::ecs::pot_call
{

PlayerNames FreePlayerOf(const ecs::Registry& registry, entt::entity pot)
{
	const auto* magic = registry.Valid(pot) ? registry.TryGet<const components::MagicPile>(pot) : nullptr;
	return magic != nullptr ? magic->player : PlayerNames::NEUTRAL;
}

std::optional<uint32_t> Make(entt::entity pot, components::Pot& data, systems::ReactionSystemInterface& reactions,
                             const systems::ReactionSystemInterface::Source& call)
{
	if (!Calls(data.calling, call.type))
	{
		return std::nullopt;
	}
	// The pot keeps that it called before the reaction spreads
	data.calling = true;
	const auto id = reactions.Create(call);
	// The reactions that the pot made before its call now also stop their living when the pot goes
	reactions.UpdateDeletionStop(pot);
	return id;
}

void Remove(entt::entity pot, components::Pot& data, systems::ReactionSystemInterface& reactions)
{
	if (!data.calling)
	{
		return;
	}
	data.calling = false;
	reactions.RemoveFrom(pot);
}

} // namespace openblack::ecs::pot_call
