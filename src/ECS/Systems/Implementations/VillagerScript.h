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
/// The villager turns at once to face a point
void Face(entt::entity villager, glm::vec2 point);

/// Waiting in a script's hands: nothing to do
uint32_t InScript(components::LivingAction& action);
/// Playing a script's clip: one more play starts, and after it the villager plays again or stands waiting
uint32_t ScriptPlayAnim(components::LivingAction& action);
/// Leaving a script's hands: only for the states a script lets take it away. True refuses.
bool ExitInScript(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_script
