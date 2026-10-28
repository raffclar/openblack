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

#include <optional>

#include <glm/vec3.hpp>

#include "ECS/Components/DrawnHeight.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Enums.h"

namespace openblack::ecs
{

/// Whether a thing can still own sounds. A thing that is gone cannot. A villager that dies cannot.
[[nodiscard]] inline bool IsAvailableForSound(const Registry& registry, entt::entity thing)
{
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	if (registry.AllOf<components::Villager>(thing))
	{
		if (const auto* action = registry.TryGet<const components::LivingAction>(thing); action != nullptr)
		{
			constexpr auto k_Final = static_cast<size_t>(components::LivingAction::Index::Final);
			return action->states.at(k_Final) != static_cast<uint8_t>(VillagerStates::Dying);
		}
	}
	return true;
}

/// The point that the sounds of a thing come from: its position, which is its base on the land. A thing in a hand
/// sounds from the hand, because the hand moves the thing to the hand each turn. A thing that is gone, that is not
/// available or that has no position gives no point.
[[nodiscard]] inline std::optional<glm::vec3> SoundPointOf(const Registry& registry, entt::entity thing)
{
	if (!IsAvailableForSound(registry, thing))
	{
		return std::nullopt;
	}
	if (const auto* inHand = registry.TryGet<const components::InHand>(thing);
	    inHand != nullptr && inHand->hand != entt::null && registry.Valid(inHand->hand))
	{
		if (const auto* hand = registry.TryGet<const components::Transform>(inHand->hand); hand != nullptr)
		{
			return hand->position;
		}
	}
	if (const auto* transform = registry.TryGet<const components::Transform>(thing); transform != nullptr)
	{
		// A town's totem sounds from the height where the frames draw it as it rises and sinks
		if (const auto* height = registry.TryGet<const components::DrawnHeight>(thing); height != nullptr)
		{
			return glm::vec3(transform->position.x, height->y, transform->position.z);
		}
		return transform->position;
	}
	return std::nullopt;
}

} // namespace openblack::ecs
