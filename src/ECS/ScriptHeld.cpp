/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHeld.h"

#include <cstdlib>

#include <vector>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Villager.h"
#include "ECS/Flocks.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::script_held
{
using components::ScriptHeld;

namespace
{
bool Trace()
{
	static const bool trace = debug_env::AnimalTrace();
	return trace;
}

ScriptHeld* SlotOf(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<ScriptHeld>(thing) : nullptr;
}

/// Gives a thing back to the game: no longer controlled; at a task's end (fromCommand false) a thing deleted when
/// released (only a script timer) or a highlight the script created goes (with "Deleting thing not created by
/// script" when the slot was not created by the script); then (pending, audio) its music is removed, and by the
/// script object type: a villager, an animal, a creature, a ball; "Unknown thing-Tell Jonty" for some types, every
/// other type nothing
void ReleaseScriptThingIntoTheGame(entt::entity thing, ScriptHeld& slot, bool fromCommand)
{
	auto& registry = Locator::entitiesRegistry::value();
	slot.controlledByScript = false;
	if (!fromCommand && (script_timer::IsTimer(thing) || (slot.createdByScript && script_highlight::IsHighlight(thing))))
	{
		// (a CREATE_HIGHLIGHT scroll goes with its last reference unless RELEASE_FROM_SCRIPT released it first)
		if (!slot.createdByScript)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Deleting thing not created by script");
		}
		ecs::ToBeDeleted(thing);
		return;
	}
	if (registry.AllOf<components::Villager>(thing))
	{
		// a villager: not in the map and not in the physics -> "Releasing object not in map"; in the physics its
		// previous state becomes DECIDE_WHAT_TO_DO, else its script state does; then villager::ReleaseFromScript
		const bool flying = physics::PhysicsObjects::IsFlying(thing);
		if (!villager::IsObjectInMap(thing) && !flying)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Releasing object not in map");
		}
		if (flying)
		{
			if (auto* action = registry.TryGet<components::LivingAction>(thing); action != nullptr)
			{
				action->states[static_cast<size_t>(components::LivingAction::Index::Previous)] =
				    static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
			}
		}
		else
		{
			SetLivingScriptState(thing, static_cast<uint32_t>(VillagerStates::DecideWhatToDo));
		}
		villager::ReleaseFromScript(thing);
		return;
	}
	if (registry.AllOf<components::Animal>(thing))
	{
		// an animal: WANDER the same way (in the physics the previous state), then animal_ai::ReleaseFromScript
		if (physics::PhysicsObjects::IsFlying(thing))
		{
			if (auto* brain = registry.TryGet<components::AnimalBrain>(thing); brain != nullptr)
			{
				brain->previousState = static_cast<uint8_t>(animal_ai::AnimalState::Wander);
			}
		}
		else
		{
			SetLivingScriptState(thing, static_cast<uint32_t>(animal_ai::AnimalState::Wander));
		}
		animal_ai::ReleaseFromScript(thing);
		return;
	}
	// a creature's release, TODO: creature spells; a ball's release flag, (pending) no Ball
}

/// A flock's members from the head (the next read first), each controlled one with a slot released with fromCommand; a
/// controlled member without a slot ends the whole loop; another container: "CANNOT RELEASE THIS! USE DISPAND!"
void ReleaseContainerContents(entt::entity container)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<components::Flock>(container))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CANNOT RELEASE THIS! USE DISPAND!");
		return;
	}
	for (const auto member : flocks::MembersFromHead(container))
	{
		auto* slot = SlotOf(member);
		if (slot != nullptr && slot->controlledByScript)
		{
			if (!slot->hasSlot)
			{
				return; // a controlled member without a slot ends the loop
			}
			ReleaseScriptThingIntoTheGame(member, *slot, true);
		}
	}
}

/// Releases a thing from the script's control: controlled: a container at a task's end is disbanded and, when the
/// script created it, no longer controlled and deleted (nothing more); with fromCommand its controlled members are
/// released (ReleaseContainerContents); then ReleaseScriptThingIntoTheGame. Not controlled: a container's members give
/// back their references (a flock's from the head)
void ReleaseControlFromScript(entt::entity thing, ScriptHeld& slot, bool fromCommand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool container = script_containers::IsContainer(thing);
	if (!slot.controlledByScript)
	{
		if (container && registry.AllOf<components::Flock>(thing))
		{
			for (const auto member : flocks::MembersFromHead(thing))
			{
				DecrementReference(member);
			}
		}
		return;
	}
	if (container)
	{
		if (!fromCommand)
		{
			// disband; created by the script -> not controlled, deleted
			script_containers::Disband(static_cast<uint32_t>(thing));
			if (slot.createdByScript)
			{
				slot.controlledByScript = false;
				ecs::ToBeDeleted(thing);
				return;
			}
		}
		else
		{
			ReleaseContainerContents(thing);
		}
	}
	ReleaseScriptThingIntoTheGame(thing, slot, fromCommand);
}
} // namespace

void AddScriptThing(entt::entity thing, bool createdByScript)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return; // "Adding Null script thing"
	}
	// a thing in a script already has its slot; else a free slot: the created flag set and the count cleared. Not
	// ported: with createdByScript a flag on the object's mesh and the object.
	if (auto* slot = registry.TryGet<ScriptHeld>(thing); slot != nullptr && slot->inScript)
	{
		return;
	}
	auto& slot = registry.AllOf<ScriptHeld>(thing) ? registry.Get<ScriptHeld>(thing) : registry.Assign<ScriptHeld>(thing);
	slot.createdByScript = createdByScript;
	slot.references = 0;
	slot.hasSlot = true;
}

void IncrementReference(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	// the original's script values are slot numbers handed out when a thing is added; openblack's are entities and its
	// CHL finders (CALL, GET_...) do not call AddScriptThing, so a thing without a slot gets one here as the finders
	// would have given it (created false) [approximate: the slot is taken at the first reference, not by the finder]
	auto* slot = registry.TryGet<ScriptHeld>(thing);
	if (slot == nullptr)
	{
		slot = &registry.Assign<ScriptHeld>(thing);
	}
	slot->hasSlot = true;
	if (slot->references == 0xFF)
	{
		return;
	}
	if (slot->references == 0)
	{
		// in script; controlled when it already was or the script created it
		slot->inScript = true;
		slot->controlledByScript = slot->controlledByScript || slot->createdByScript;
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: entity {} in script (controlled {})", static_cast<uint32_t>(thing),
			                   slot->controlledByScript);
		}
	}
	slot->inScript = true;
	++slot->references;
}

void DecrementReference(entt::entity thing)
{
	if (auto* slot = SlotOf(thing); slot != nullptr && slot->references != 0)
	{
		--slot->references;
	}
}

void Process()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> released;
	registry.Each<const ScriptHeld>([&released](entt::entity thing, const ScriptHeld& slot) {
		if (slot.hasSlot && slot.references == 0)
		{
			released.push_back(thing);
		}
	});
	for (const auto thing : released)
	{
		// an earlier release may have changed the registry
		auto* held = registry.Valid(thing) ? registry.TryGet<ScriptHeld>(thing) : nullptr;
		if (held == nullptr)
		{
			continue;
		}
		auto& slot = *held;
		const bool wasInScript = slot.inScript;
		ReleaseControlFromScript(thing, slot, false); // the task's end
		if (Trace() && wasInScript)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: entity {} released", static_cast<uint32_t>(thing));
		}
		// out of the script, then the slot is freed; ControlledByScript is clear now (the original panics with "Thing
		// should be released! PANIC" otherwise)
		if (registry.Valid(thing))
		{
			registry.Remove<ScriptHeld>(thing);
		}
	}
}

void ReleaseFromScript(entt::entity thing)
{
	// a controlled thing -> ReleaseControlFromScript(thing, fromCommand true)
	auto* slot = SlotOf(thing);
	if (slot == nullptr || !slot->controlledByScript)
	{
		return;
	}
	ReleaseControlFromScript(thing, *slot, true);
}

bool IsInScript(entt::entity thing)
{
	const auto* slot = SlotOf(thing);
	return slot != nullptr && slot->inScript;
}

bool IsControlledByScript(entt::entity thing)
{
	const auto* slot = SlotOf(thing);
	return slot != nullptr && slot->controlledByScript;
}

void SetLivingScriptState(entt::entity living, uint32_t state)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (living == entt::null || !registry.Valid(living))
	{
		return;
	}
	// a creature sets its own state. TODO(creature)
	if (registry.AllOf<components::Creature>(living))
	{
		return;
	}
	if (registry.AllOf<components::Villager>(living))
	{
		villager::SetScriptState(living, static_cast<VillagerStates>(static_cast<uint8_t>(state)));
		return;
	}
	if (registry.AllOf<components::Animal>(living))
	{
		animal_ai::SetScriptState(living, static_cast<animal_ai::AnimalState>(static_cast<uint8_t>(state)));
	}
}

void SetControlledByScript(entt::entity thing, bool controlled)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	if (auto* slot = registry.TryGet<ScriptHeld>(thing); slot != nullptr)
	{
		slot->controlledByScript = controlled;
	}
	else if (controlled)
	{
		auto& bits = registry.Assign<ScriptHeld>(thing);
		bits.controlledByScript = true;
		bits.hasSlot = false;
	}
}

bool CannotBeEaten(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<components::CannotBeEaten>(thing);
}

void SetCannotBeEaten(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing != entt::null && registry.Valid(thing) && !registry.AllOf<components::CannotBeEaten>(thing))
	{
		registry.Assign<components::CannotBeEaten>(thing);
	}
}

bool MayTarget(entt::entity hunter, entt::entity target)
{
	// a script-controlled target is only for a hunter that is in a script itself
	return !IsControlledByScript(target) || IsInScript(hunter);
}

} // namespace openblack::ecs::script_held
