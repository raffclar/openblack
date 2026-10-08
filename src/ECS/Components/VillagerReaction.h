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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A villager's reaction and the object its setup reacts to, for the reaction types of the villager type table
/// (ECS/Systems/Implementations/VillagerReactions.cpp: 7 REACT_TO_FOOD and 12 REACT_TO_WOOD; the miracle types 10 / 20
/// / 13 and type 23 keep their own maps for now). Present only while the villager follows one of them: written by
/// villager_reactions::AddReaction and the type's setup (the object), removed by StopReacting
struct VillagerReactionSlot
{
	uint32_t reaction {0};                                ///< The reaction (effects::reactions id)
	openblack::Reaction type {openblack::Reaction::None}; ///< Its type, kept for the record after it is gone
	entt::entity object {entt::null};                     ///< What the setup reacts to
	/// the place in the reaction's follower list (head insertion): shutting the reaction down stops the newest
	/// follower first
	uint32_t joinOrder {0};
};

} // namespace openblack::ecs::components
