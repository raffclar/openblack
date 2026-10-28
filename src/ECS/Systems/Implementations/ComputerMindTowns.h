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

#include "3D/MapCoords.h"
#include "ComputerMindRuleTable.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The town rules and the actions for the food and the wood of towns that the other parts of the mind of a computer
/// god use
namespace openblack::ecs::systems::computer_world
{

/// How much the god wants to cast the miracle at the place, from its charge, the time to charge the rest and the pull
/// of the place. With no place, the pull is 1. When the god can cast the miracle, its icon becomes the node's object.
float ShallCastAt(RuleContext& context, computer_mind::Mind& mind, uint16_t node, MagicType type,
                  const std::optional<map_coords::MapCoords>& place);

/// The hand takes the thing that the chosen leaf found and drops it on the store of the town above the node
bool DropOnTownStore(RuleContext& context, computer_mind::Mind& mind, uint16_t node);
/// The god charges the miracle of the node's icon and casts it on the store of the town above the node
bool CastIconOnTownStore(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

} // namespace openblack::ecs::systems::computer_world
