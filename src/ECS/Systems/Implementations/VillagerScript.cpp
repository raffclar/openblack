/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerScript.h"

#include <cmath>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerMemory.h"
#include "ECS/VillagerScriptRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_script = openblack::ecs::villager_script;
namespace rules = openblack::ecs::villager_script_rules;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

VillagerStates StateOf(const LivingAction& action, LivingAction::Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

/// The state its top state works towards: the top state itself when that is a final one
VillagerStates FinalStateOf(const LivingAction& action)
{
	const auto top = StateOf(action, LivingAction::Index::Top);
	if (StateInfo(top).isFinalState != 0)
	{
		return top;
	}
	return StateOf(action, LivingAction::Index::Final);
}
} // namespace

bool villager_script::CanBeDirected(entt::entity villager)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr || !registry.AllOf<Villager>(villager) || !Locator::livingActionSystem::has_value() ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	// Alive, lying on the land rather than in a hand, flying or in a tornado, and not drowning
	// A villager is on the land unless held, flying or carried off; one just made is on it before it is filed in the
	// map's cells
	const bool onLand = !registry.AnyOf<InHand, InPhysics, CarriedByTornado>(villager);
	return onLand && FinalStateOf(*action) != VillagerStates::Dead &&
	       StateOf(*action, LivingAction::Index::Top) != VillagerStates::Drowning;
}

void villager_script::SetScriptState(entt::entity villager, VillagerStates state)
{
	if (!CanBeDirected(villager))
	{
		return;
	}
	auto& action = Entities().Get<LivingAction>(villager);
	auto& living = Locator::livingActionSystem::value();
	villager_memory::StorePreviousState(action);
	// The states it leaves are told, but have no say: the script's word goes
	living.VillagerCallExitState(action, LivingAction::Index::Top, state);
	if (FinalStateOf(action) != StateOf(action, LivingAction::Index::Top))
	{
		living.VillagerCallExitState(action, LivingAction::Index::Final, state);
	}
	// TODO(opening): a state whose entry sets the villager off somewhere else first (a script's remembered walk) is not
	// gone into directly; none of the states the scripts put villagers in has one yet
	living.VillagerSetState(action, LivingAction::Index::Top, state, true);
	// Its clip starts from the beginning
	villager_animate::SetAnim(villager, villager_animate::StateClip(villager), true);
	action.turnsSinceStateChange = 0;
}

void villager_script::MoveTo(entt::entity villager, glm::vec2 goal)
{
	auto& registry = Entities();
	if (!CanBeDirected(villager) || !registry.AllOf<WallHug, Transform>(villager))
	{
		return;
	}
	// Already within a turn's step of the goal: it waits there for the script
	const auto here = glm::xz(registry.Get<const Transform>(villager).position);
	const float step = rules::WalkSpeedToScriptSpeed(registry.Get<const WallHug>(villager).speed);
	const auto offset = goal - here;
	if (glm::dot(offset, offset) < step * step)
	{
		SetScriptState(villager, VillagerStates::InScript);
		return;
	}
	auto& action = registry.Get<LivingAction>(villager);
	if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos,
	                                                                               VillagerStates::InScript))
	{
		villager_home::SetupMobileMoveTo(action, goal, VillagerStates::InScript);
	}
}

void villager_script::SetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays)
{
	Entities().AssignOrReplace<ScriptAnimation>(villager, clip, plays);
}

bool villager_script::HasPlayedScriptAnimation(entt::entity villager)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return true;
	}
	const auto* animation = registry.TryGet<const ScriptAnimation>(villager);
	return rules::HasPlayedScriptClip(StateOf(*action, LivingAction::Index::Top),
	                                  animation != nullptr ? animation->playsLeft : 0);
}

int32_t villager_script::ScriptClip(entt::entity villager)
{
	const auto* animation = Entities().TryGet<const ScriptAnimation>(villager);
	return animation != nullptr ? static_cast<int32_t>(animation->clip) : static_cast<int32_t>(AnimId::Invalid);
}

void villager_script::Face(entt::entity villager, glm::vec2 point)
{
	auto& registry = Entities();
	auto* transform = registry.TryGet<Transform>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (transform == nullptr || wallHug == nullptr)
	{
		return;
	}
	const auto offset = point - glm::xz(transform->position);
	if (offset == glm::vec2(0.0f))
	{
		return;
	}
	const float angle = std::atan2(offset.y, offset.x);
	wallHug->yAngle = angle;
	transform->rotation = glm::eulerAngleY(-angle - glm::radians(90.0f));
	registry.SetDirty();
}

uint32_t villager_script::InScript(LivingAction& /*action*/)
{
	// TODO(opening): remembering what the script had it doing, to go back to it after being picked up or reacting
	return 1;
}

uint32_t villager_script::ScriptPlayAnim(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto* animation = registry.TryGet<ScriptAnimation>(villager);
	if (animation == nullptr)
	{
		return 1;
	}
	const auto step = rules::StepScriptClip(animation->playsLeft);
	animation->playsLeft = step.playsLeft;
	if (step.then.has_value())
	{
		Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, *step.then);
	}
	return 1;
}

bool villager_script::ExitInScript(LivingAction& action, VillagerStates next)
{
	const auto& info = StateInfo(next);
	const rules::StateRules nextRules {.isScriptState = info.isScriptState != 0,
	                                   .isScriptInterruptable = info.isScriptInterruptableState != 0};
	return !rules::ScriptLetsGo(FinalStateOf(action), next, nextRules);
}
