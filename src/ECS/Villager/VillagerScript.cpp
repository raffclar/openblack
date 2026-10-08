/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScript.h"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Flocks.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

// What the CHL scripts do to a villager (VillagerScript.h; docs/bw1-notes/map-loading.md).

namespace openblack::ecs::villager
{
using namespace components;
using state_info::StateInfo;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

/// The state is IN_HAND
bool IsStateForInterface(VillagerStates state)
{
	return state == VillagerStates::InHand;
}

/// 1 if `next` is script-interruptable (state info table), is IN_HAND or IsStateExitFunctionSameAs; else 0
uint32_t ExitNoChangeState(entt::entity villager, VillagerStates next)
{
	if (state_info::IsScriptInterruptable(StateInfo(next)) || IsStateForInterface(next) ||
	    IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	return 0;
}

/// (pos): the goal = pos, InitStepsXZ, off the circle-hug lists (openblack's WallHug keeps none), then
/// AreWeThere(0) -> ARRIVED; else the circle-hug info is reset and the walk is STEP_THROUGH
void SetupMobileMoveToPos(entt::entity villager, const glm::vec2& goal)
{
	auto& registry = Entities();
	auto& wallHug = registry.Get<WallHug>(villager);
	auto& transform = registry.Get<Transform>(villager);
	wallHug.goal = goal;
	InitStepsXZ(transform, wallHug);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	if (AreWeThere(villager, goal, 0.0f))
	{
		// ARRIVED: PathfindingSystem puts it on the goal and VillagerMoveToPos sees ARRIVED
		registry.Assign<MoveStateArrivedTag>(villager, MoveStateClockwise::Undefined, goal);
		return;
	}
	registry.Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform.position));
}

/// SetAnim(n): the state's clip if it is another one, from its start when n != 0 and not dancing (IN_SCRIPT and the
/// script states are never a dance: not tested)
void SetAnim(entt::entity villager, int32_t n)
{
	VillagerSetStateClip(villager, n != 0);
}
} // namespace

bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b)
{
	// The entry functions of both states, from the original state table (an empty slot in the first -> the same at
	// once)
	const auto entryOf = [](VillagerStates s) {
		const auto i = static_cast<size_t>(static_cast<uint8_t>(s));
		return i < k_OriginalStateFns.size() ? k_OriginalStateFns.at(i).entry : 0u;
	};
	return entryOf(a) == entryOf(b);
}

bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r)
{
	const auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	if (transform == nullptr || wallHug == nullptr)
	{
		return false;
	}
	// dx = Pos.x - pos.x, dz = Pos.z - pos.z, R = (float) speed + r; dx^2 + dz^2 < R^2 -> 1
	// (R^2 <= d^2 -> 0)
	const float radius = wallHug->speed + r;
	return glm::distance2(glm::xz(transform->position), pos) < radius * radius;
}

std::optional<glm::vec2> GetDestPos(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	if (wallHug == nullptr)
	{
		return std::nullopt;
	}
	return wallHug->goal;
}

bool AreWeThereAtDestination(entt::entity villager, float r)
{
	// GetDestPos, then AreWeThere(dest, r)
	const auto dest = GetDestPos(villager);
	return dest.has_value() && AreWeThere(villager, *dest, r);
}

uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction, WallHug, Transform>(villager))
	{
		return 0;
	}
	// The living info's moveState (1 MOVE_TO_POS in all the villager rows of info.dat);
	// 3 MOVE_ON_STRUCTURE instead when the moving-on-structure flag is set, a flag only
	// MOVE_ON_STRUCTURE sets (state 3, not ported in openblack: never set here)
	const auto moveState = static_cast<VillagerStates>(static_cast<uint8_t>(InfoOf(villager).moveState));
	// SetCurrentAndDestinationState(moveState, final); not 1 -> 0
	if (SetCurrentAndDestinationState(villager, moveState, final) != 1)
	{
		return 0;
	}
	// SetupMobileMoveToPos(pos) (the one without a hug state), then 1
	SetupMobileMoveToPos(villager, goal);
	return 1;
}

void StorePreviousState(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// GetFinalState; a passing one (never kept in PREVIOUS, or a reaction) keeps what
	// PREVIOUS had; a raw SetState, no town modifiers
	const auto final = GetFinalState(villager);
	const auto& info = StateInfo(final);
	auto stored = final;
	if (state_info::NotStoredAsPrevious(info) || state_info::IsReactive(info))
	{
		stored = GetState(villager, Index::Previous);
	}
	action->states.at(static_cast<size_t>(Index::Previous)) = static_cast<uint8_t>(stored);
}

bool IsAvailable(entt::entity villager)
{
	// Being deleted (ecs::IsAvailable) -> 0; GetFinalState == DYING -> 0; else 1
	return ecs::IsAvailable(villager) && ActionOf(villager) != nullptr && GetFinalState(villager) != VillagerStates::Dying;
}

bool IsObjectInMap(entt::entity villager)
{
	return GetState(villager, Index::Top) != VillagerStates::InHand;
}

void SetScriptState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// IsAvailable and IsObjectInMap, else nothing
	if (!IsAvailable(villager) || !IsObjectInMap(villager))
	{
		return;
	}
	// StorePreviousState
	StorePreviousState(villager);
	// CallExitStateFunction(state), CallEntryStateFunction(state); both
	// results are ignored
	CallExitStateFunction(villager, state);
	CallEntryStateFunction(villager, state);
	// (approximate) The original's walk only advances from MOVE_TO_POS's state function, so a villager the script
	// takes out of it stops where it is; openblack's
	// PathfindingSystem steps every entity with a move tag whatever its state, so the tags go here
	if (GetState(villager, Index::Top) != VillagerStates::MoveToPos)
	{
		Entities()
		    .Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		            MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		Entities().Remove<WallHugObjectReference>(villager);
	}
	// SetAnim(1)
	SetAnim(villager, 1);
	// The counter = 0
	action->turnsUntilStateChange = 0;
}

void SetScriptAnimation(entt::entity villager, uint32_t anim, uint32_t loops)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return;
	}
	// The times (the first pop), then the clip (the second pop)
	v->scriptAnimLoops = loops;
	v->scriptAnim = anim;
}

int32_t ScriptAnimation(entt::entity villager)
{
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr ? static_cast<int32_t>(v->scriptAnim) : 0;
}

bool IsScriptAnimationComplete(entt::entity villager)
{
	// TOP 23 WAIT_FOR_ANIMATION -> 0
	const auto top = GetState(villager, Index::Top);
	if (top == VillagerStates::WaitForAnimation)
	{
		return false;
	}
	// TOP 200 SCRIPT_PLAY_ANIM -> the times left == 0
	if (top == VillagerStates::ScriptPlayAnim)
	{
		const auto* v = Entities().TryGet<const Villager>(villager);
		return v == nullptr || v->scriptAnimLoops == 0;
	}
	return true;
}

void PlayAnimThenSetState(entt::entity villager, VillagerStates state)
{
	// CallExitStateFunction(state); 0 -> nothing
	if (CallExitStateFunction(villager, state) == 0)
	{
		return;
	}
	// CallEntryStateFunction(23, state): TOP 23 WAIT_FOR_ANIMATION, FINAL state; no clip
	CallEntryStateFunction(villager, VillagerStates::WaitForAnimation, state);
}

uint32_t StateInScript([[maybe_unused]] LivingAction& action)
{
	// No script reminder data -> create it (not ported); 1
	return 1;
}

uint32_t EnterInScript(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// No script reminder data, or it is not for `next` -> 1. The reminder data is not ported
	// (inferred: without it the villager has no walk to resume), so the branch that resumes it (SetupMoveToPos to
	// the stored goal or the reminder's point, next, and 0x23) is never taken
	(void)action;
	return 1;
}

uint32_t ExitInScript(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	// The circle-hug info is reset (openblack's WallHug has none); IsDancing,
	// result unused
	// IsScriptState(next) (state info table) -> 1
	if (state_info::IsScriptState(StateInfo(next)))
	{
		return 1;
	}
	// The script reminder data is created if missing and keeps GetFinalState in mind: not ported
	// ExitNoChangeState(next)
	return ExitNoChangeState(villager, next);
}

uint32_t ScriptPlayAnim(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// No times left -> nothing
	if (v->scriptAnimLoops > 0)
	{
		// One time less
		--v->scriptAnimLoops;
		// PlayAnimThenSetState(times left ? 200 SCRIPT_PLAY_ANIM : 4 IN_SCRIPT, 1)
		PlayAnimThenSetState(villager, v->scriptAnimLoops > 0 ? VillagerStates::ScriptPlayAnim : VillagerStates::InScript);
		// The script reminder data is created if missing (not ported)
	}
	return 1;
}

uint32_t EnterPlayAnim(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// No script reminder data or it is not for `next` -> 1; the reminder branch (back to its walk, or its clip
	// again) is not ported (inferred: never taken without the reminder data)
	(void)action;
	return 1;
}

uint32_t ExitPlayAnim(LivingAction& action, VillagerStates next)
{
	// ExitInScript
	return ExitInScript(action, next);
}

uint32_t WaitForAnimation(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// IsReadyForNewAnimation -> SetTopStateToFinal and 0; else 1
	if (VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		SetTopStateToFinal(villager);
		return 0;
	}
	return 1;
}

void SetYAngle(Transform& transform, WallHug& wallHug, float angle)
{
	transform.rotation = affine::AngleY(angle + glm::radians(90.0f)); // drawn a quarter turn on from the 3D angle
	wallHug.yAngle = angle;
}

void InitStepsXZ(Transform& transform, WallHug& wallHug)
{
	// GetAngleFromXZ(Pos, GetDestPos), each one a MapCoords first
	// ((approximate) from the metres openblack keeps)
	const uint16_t angle = gutils::GetAngleFromXZ(glm::xz(transform.position), wallHug.goal);
	// SetTowardsAngle, for a villager SetGameAngle
	// (no turn limit): the game angle = a, then the Y angle from ConvertGameAngleTo3D(a)
	SetGameAngle(transform, wallHug, angle);
	// The step x = (COS[angle] x (speed >> 4)) >> 12, z the same with SIN, y = 0
	// (gutils::StepFromAngle); MapCoords units a turn, kept in metres
	const auto step = gutils::StepFromAngle(angle, WholeSpeed(wallHug));
	wallHug.step = glm::vec2(map_coords::ToMetres(step.x), map_coords::ToMetres(step.y));
}

void ReleaseFromScript(entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return;
	}
	// The game's flag 0x8000 -> nothing ((pending) the flag: clear here)
	// Its flock: the flock's link to it cleared when it is this one ((pending) not ported), then it leaves the
	// flock
	if (const auto flock = flocks::FlockOf(villager); flock != entt::null)
	{
		flocks::RemoveLiving(flock, villager, false);
	}
	const bool hasTown = v->town != entt::null && registry.Valid(v->town);
	// In the physics or in the hand: without a town, the vagrants' head
	if (physics::PhysicsObjects::IsFlying(villager) || fire::traits::InHand(villager))
	{
		if (!hasTown)
		{
			town_villagers::AddToVagrants(villager);
		}
		return;
	}
	// life <= 0 -> VillagerDead(0, the player who last dropped it,
	// 0, 1) ((approximate) openblack keeps no dropper: none); not a death state -> SetTopState(15)
	if (!(v->life > 0.0f))
	{
		VillagerDead(villager, DeathReason::None, std::nullopt, 0.0f, 1);
		const auto* action = registry.TryGet<const LivingAction>(villager);
		const auto top = action != nullptr ? action->states[static_cast<size_t>(LivingAction::Index::Top)] : 0;
		if (!(top >= 13 && top <= 17))
		{
			SetTopState(villager, VillagerStates::Dead);
		}
		return;
	}
	// A town -> DecideWhatToDo
	if (hasTown)
	{
		if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr)
		{
			DecideWhatToDo(*action);
		}
		return;
	}
	// The vagrants (unless there already), SetTopState(130), VagrantStart
	town_villagers::AddToVagrants(villager);
	SetTopState(villager, VillagerStates::VagrantStart);
	VagrantStart(villager);
}

} // namespace openblack::ecs::villager
