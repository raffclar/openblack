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

#include "ComputerMindRuleTable.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The defence of a computer god that the other parts of its mind use: the threat to a town from the attacks on it,
/// and how much the god wants a shield over a town.
namespace openblack::ecs::systems::computer_world
{

/// The threat to a town from the attack of a player that it fears most. None when that attack did no harm.
[[nodiscard]] std::optional<computer_mind_rules::TownThreat> ThreatToTown(entt::entity town, uint32_t turn);

/// How much the god wants a shield over the town above the node, against the threat to the town. The god chooses the
/// miracle shield against the harm of miracles, else the physical shield. The node's object becomes the icon of the
/// shield. 0 when the god has that shield over the town already.
float ShieldForTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

} // namespace openblack::ecs::systems::computer_world
