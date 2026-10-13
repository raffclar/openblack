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

#include <entt/entity/fwd.hpp>

#include "3D/AllMeshes.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers watching a creature fight (see fight_watch for the rules): each walks to its place on the ring round the
/// arena, faces the arena and cheers, despairs or chats by how the fight was going as it arrived. The state functions
/// are the living action system's.
namespace openblack::ecs::villager_fight_watch
{

/// A villager reacting to a creature that has gone into a fight takes up the reaction of the fight's arena instead
void WatchCreaturesFight(components::LivingAction& action, entt::entity creature);

/// The villager sets off for its place round the arena; with the arena gone it stops reacting
uint32_t InitialiseFightReaction(components::LivingAction& action);
/// At its place it faces the arena
uint32_t PerformFightReaction(components::LivingAction& action);

/// The clip a villager watching a fight plays from when it reaches its place; standing when the arena or either fighter
/// is gone
[[nodiscard]] AnimId WatchClip(entt::entity villager);

} // namespace openblack::ecs::villager_fight_watch
