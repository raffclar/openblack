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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

/// A villager or animal walking a camera track (the CHL WALK_PATH on a villager): a path like the mobile object's
/// (components::LivingWalkPath, ECS/MobileWalkPaths.h), timed by the walker's speed.
namespace openblack::ecs::living
{

/// The script's WALK_PATH (with final 4 IN_SCRIPT): a new path (the old one deleted) with the track's camera, the track,
/// to, forward, step = duration / (focus.length / (speed / 655 x 0.1)) (no zero guard), the speed as a float and the
/// current point = duration x from; then the current and destination states set to (28 MOVE_ALONG_PATH, final) and,
/// when that succeeds, the state's animation. False when the track cannot be read (the original crashes on the missing
/// camera) or the state was refused. (pending) an animal's state 28
bool StartWalkPath(entt::entity living, int32_t track, VillagerStates final, float from, float to, bool forward);

/// State 28 MOVE_ALONG_PATH. A speed change retimes the step (0 stops it); the sample and the point are the mobile
/// object path's; below `to` the walker advances one step, turns towards the point (SetYAngle of the angle from its
/// position at the turn's start to the point, when it is more than sqrt(0.001) away) and moves there (on the land,
/// relative y 0); at `to`, SetTopStateToFinal. 1
uint32_t MoveAlongPath(components::LivingAction& action);

/// current / duration; nullopt without a path (the original reads it unchecked)
[[nodiscard]] std::optional<float> GetWalkPathPercentage(entt::entity living);
/// True without a path or when the percentage is >= v
[[nodiscard]] bool WalkPathReached(entt::entity living, float v);

} // namespace openblack::ecs::living
