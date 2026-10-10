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
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager in a script's hands: standing where the script put it, walking where it sends it and playing the clips it
/// asks for. The state functions are the living action system's; the rest serves the script natives.
namespace openblack::ecs::villager_script
{

/// Whether a script can set the villager's state: it is alive, on the land and not drowning
[[nodiscard]] bool CanBeDirected(entt::entity villager);

/// A script puts the villager straight into a state, which starts its clip from the beginning
void SetScriptState(entt::entity villager, VillagerStates state);
/// A script sends the villager to a point, where it stands waiting for the script; one already there waits at once
void MoveTo(entt::entity villager, glm::vec2 goal);
/// The clip the villager plays when a script puts it in the playing state, and how many times
void SetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays);
/// Whether the villager has finished the plays of the clip a script asked for
[[nodiscard]] bool HasPlayedScriptAnimation(entt::entity villager);
/// The clip a script asked the villager to play, none when it asked for none
[[nodiscard]] int32_t ScriptClip(entt::entity villager);
/// A script gives the villager an age: it becomes a child or an adult, its model and size follow, and it is that old by
/// the game's clock
void SetAge(entt::entity villager, uint32_t age);
/// The villager turns at once to face a point
void Face(entt::entity villager, glm::vec2 point);
/// A script makes the villager play a clip in place of its state's own, until its state next chooses one
void OverrideAnimation(entt::entity villager, int32_t clip);
/// The way the villager faces, as an angle about the upright
[[nodiscard]] std::optional<float> YAngle(entt::entity villager);
/// The villager turns at once to face the way of an angle about the upright
void SetYAngle(entt::entity villager, float angle);

/// A script sets the villager walking one of the camera editor's tracks, from `from` to `to` (shares of its way), at
/// its own walking speed, then waiting for the script. False when there is no such track.
bool StartPathWalk(entt::entity villager, int32_t number, bool forward, float from, float to);
/// How much of its track's way the villager has walked; none when it was given none to walk
[[nodiscard]] std::optional<float> PathWalkPercentage(entt::entity villager);

/// Waiting in a script's hands: nothing to do
uint32_t InScript(components::LivingAction& action);
/// Playing a script's clip: one more play starts, and after it the villager plays again or stands waiting
uint32_t ScriptPlayAnim(components::LivingAction& action);
/// Walking a script's track: on along it, facing the way it goes, and waiting for the script at its end
uint32_t MoveAlongPath(components::LivingAction& action);
/// Leaving a script's hands: only for the states a script lets take it away. True refuses.
bool ExitInScript(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_script
