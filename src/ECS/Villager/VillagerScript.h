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

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// What the CHL scripts do to a villager, as the original does it (docs/bw1-notes/map-loading.md, "Script commands
// that move things"): MOVE_GAME_THING's walk (SetupMoveToPos), the script states IN_SCRIPT (4) and SCRIPT_PLAY_ANIM
// (200) with WAIT_FOR_ANIMATION (23) between the clips, SetScriptState and IsScriptAnimationComplete. Not ported:
// the original's script reminder data, with which a villager taken out of a script state remembers its walk and
// resumes it when it comes back.

namespace openblack::ecs::components
{
struct Transform;
struct WallHug;
} // namespace openblack::ecs::components

namespace openblack::ecs::villager
{
/// Sets the villager's Y angle as openblack keeps it: WallHug::yAngle (the 3D angle, VillagerCore's convention) and
/// the drawn rotation the pathfinding gives the transform, AngleY(angle + 90 degrees), the angle the world matrix
/// uses. (approximate) no u16 game angle is stored: VillagerCore derives it from yAngle when it reads it (rounded to
/// 2048ths). Shared by PathfindingSystem's InitializeStep (openblack's orbit) and ECS/SuperVillager; VillagerCore's
/// SetGameAngle (the game angle in, then FaceAngle) is a second place that sets the object's Y angle, kept
void SetYAngle(components::Transform& transform, components::WallHug& wallHug, float angle);
/// For villagers: a = the u16 game angle from Pos to GetDestPos (integer arctangent), SetTowardsAngle(a)
/// (SetGameAngle, no turn limit), then the step ((COS / SIN[a] x (speed >> 4)) >> 12, gutils::StepFromAngle) in
/// MapCoords units, kept in metres (WallHug::step). The one copy: SetupMobileMoveToPos and PathfindingSystem's
/// InitializeStepToGoal call it. (approximate) Pos and the goal are metres made MapCoords here
void InitStepsXZ(components::Transform& transform, components::WallHug& wallHug);
/// (a, b): the entry functions of the state rows (VillagerOriginalFns.h)
/// of both states are the same (EnterInScript, EnterPlayAnim, EnterBuilding)
[[nodiscard]] bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b);
/// AreWeThere(pos, r): the x / z distance from the villager to `pos` is less than
/// its speed (u16 MapCoords a turn: openblack's WallHug::speed in metres) + r. (approximate: floats in metres
/// instead of the 16.16 MapCoords integers)
[[nodiscard]] bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r);
/// The walk's goal (openblack's WallHug::goal)
[[nodiscard]] std::optional<glm::vec2> GetDestPos(entt::entity villager);
/// AreWeThere(GetDestPos(), r)
[[nodiscard]] bool AreWeThereAtDestination(entt::entity villager, float r);
/// (pos, final): SetCurrentAndDestinationState(the living info's moveState, final) and,
/// only if it returns 1, SetupMobileMoveToPos: a STEP_THROUGH walk (no obstacle hugging), or
/// ARRIVED if it is there already. Returns 1 if the walk was set up, else 0.
uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final);
/// The final state goes to PREVIOUS, unless it is a passing one
/// (not stored as previous, or reactive, in the state info), which keeps what was stored
void StorePreviousState(entt::entity villager);
/// Not being deleted and the final state is not 14 DYING
[[nodiscard]] bool IsAvailable(entt::entity villager);
/// The object's in-map flag. (approximate: openblack keeps no such flag; a villager in the
/// hand is the one taken out of the map)
[[nodiscard]] bool IsObjectInMap(entt::entity villager);
/// The script's SetScriptState for a villager (the Living that is not a creature)
void SetScriptState(entt::entity villager, VillagerStates state);
/// Called when a script releases the thing into the game, after its state:
/// (pending) nothing when the game's flag 0x8000 is set; out of its flock; in the
/// physics or the hand only a villager without a town joins the vagrants; dead (life <= 0) -> VillagerDead(0,
/// the dropper's player, 0, 1) and, unless its top state is a death state (13..17),
/// SetTopState(15 DEAD); a town -> DecideWhatToDo; else the vagrants, SetTopState(130 VAGRANT_START) and
/// VagrantStart
void ReleaseFromScript(entt::entity villager);
/// The script's SetScriptUlong on a villager: the clip and the times it plays
void SetScriptAnimation(entt::entity villager, uint32_t anim, uint32_t loops);
/// SCRIPT_PLAY_ANIM's clip function: the script's clip
[[nodiscard]] int32_t ScriptAnimation(entt::entity villager);
/// PLAYED 064: TOP 23 -> 0; TOP 200 -> the times left == 0;
/// else 1
[[nodiscard]] bool IsScriptAnimationComplete(entt::entity villager);
/// (state, unused): CallExitStateFunction(state) and, if 1,
/// CallEntryStateFunction(23 WAIT_FOR_ANIMATION, state); the clip is left as it is
void PlayAnimThenSetState(entt::entity villager, VillagerStates state);

/// State 4 IN_SCRIPT
uint32_t StateInScript(components::LivingAction& action);
uint32_t EnterInScript(components::LivingAction& action, VillagerStates final, VillagerStates next);
uint32_t ExitInScript(components::LivingAction& action, VillagerStates next);
/// State 200 SCRIPT_PLAY_ANIM
uint32_t ScriptPlayAnim(components::LivingAction& action);
uint32_t EnterPlayAnim(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The same as ExitInScript
uint32_t ExitPlayAnim(components::LivingAction& action, VillagerStates next);
/// State 23 WAIT_FOR_ANIMATION
uint32_t WaitForAnimation(components::LivingAction& action);
} // namespace openblack::ecs::villager
