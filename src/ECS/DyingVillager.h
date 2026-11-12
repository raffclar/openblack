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

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Enums.h"

namespace openblack::ecs
{

/// Whether a thing is a villager on its way to die: its final state is the state of a villager that dies. The hand can
/// do nothing with such a villager, and the cursor does not pick it.
[[nodiscard]] inline bool IsDyingVillager(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing) || !registry.AllOf<components::Villager>(thing))
	{
		return false;
	}
	const auto* action = registry.TryGet<const components::LivingAction>(thing);
	constexpr auto k_Final = static_cast<size_t>(components::LivingAction::Index::Final);
	return action != nullptr && action->states.at(k_Final) == static_cast<uint8_t>(VillagerStates::Dying);
}

} // namespace openblack::ecs
