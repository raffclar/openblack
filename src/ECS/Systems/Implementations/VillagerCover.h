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
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A frightened villager taking cover: it runs along its town's paths to the nearest building it can hide in, stays
/// inside out of sight for a while, then comes out and looks about before going back to what it was doing. One with
/// nowhere to hide panics in the open: having run off it turns to look back at what it ran from, then decides afresh.
/// The state functions are the living action system's.
namespace openblack::ecs::villager_cover
{

/// The villager sets off for the nearest building of its town it can hide in, if there is one near enough; whether it
/// did. What frightened it, if known, keeps it inside while that stays within the keep-away distance of a kind of
/// reaction.
bool SetupGoAndHideInNearbyBuilding(components::LivingAction& action, entt::entity threat = entt::null,
                                    Reaction threatKind = Reaction::None);
/// The point a panicking villager ran from, which it looks back at once it stops
void SetPanicPoint(entt::entity villager, glm::vec2 point);

uint32_t GoAndHideInNearbyBuilding(components::LivingAction& action);
uint32_t LookToSeeIfItIsSafe(components::LivingAction& action);
uint32_t PanicReaction(components::LivingAction& action);

} // namespace openblack::ecs::villager_cover
