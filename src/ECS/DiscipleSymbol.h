/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Disciple.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

namespace openblack::ecs::disciple_symbol
{

/// True when the hand holds its thing. A hand that only grabs or takes a thing does not hold it yet.
[[nodiscard]] constexpr bool Holds(components::HandGrab::State state)
{
	using State = components::HandGrab::State;
	return state == State::Holding || state == State::ReadyToThrow || state == State::LettingGo;
}

/// The thing that the first hand that holds a thing holds. None when no hand holds a thing.
[[nodiscard]] inline std::optional<entt::entity> HeldThing(const Registry& land, std::span<const entt::entity> hands)
{
	for (const auto hand : hands)
	{
		const auto* grab = land.Valid(hand) ? land.TryGet<const components::HandGrab>(hand) : nullptr;
		if (grab != nullptr && Holds(grab->state) && grab->object != entt::null && land.Valid(grab->object))
		{
			return grab->object;
		}
	}
	return std::nullopt;
}

/// True for a villager that is a disciple
[[nodiscard]] inline bool IsDisciple(const Registry& land, entt::entity thing)
{
	return land.Valid(thing) && land.AllOf<components::Villager, components::Disciple>(thing);
}

/// The disciple that shows its symbol. Only the hands of the player at this computer and the thing under the pointer
/// of this computer can show a symbol. The hands of other gods never show one. The thing in the hand shows the symbol
/// when it is a disciple. If not, the thing under the pointer shows it when it is a disciple. The symbol shows with the
/// glow of the disciple, thus a disciple that the frames do not draw shows no symbol.
[[nodiscard]] inline std::optional<entt::entity> Shown(const Registry& land, std::span<const entt::entity> ownHands,
                                                       std::optional<entt::entity> underPointer)
{
	std::optional<entt::entity> chosen;
	if (const auto held = HeldThing(land, ownHands); held.has_value() && IsDisciple(land, *held))
	{
		chosen = held;
	}
	else if (underPointer.has_value() && IsDisciple(land, *underPointer))
	{
		chosen = underPointer;
	}
	if (chosen.has_value() && land.AnyOf<components::HiddenByState>(*chosen))
	{
		return std::nullopt;
	}
	return chosen;
}

} // namespace openblack::ecs::disciple_symbol
