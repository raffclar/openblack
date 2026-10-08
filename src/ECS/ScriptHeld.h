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

/// The things the scripts hold. openblack's CHL objects are the entities themselves, so a script's slot is the
/// components::ScriptHeld of the entity (ECS/Components/ScriptHeld.h).
namespace openblack::ecs::script_held
{

/// A command hands a thing to the script. `createdByScript` is true for the CREATE family, false for the finders; a
/// thing already in a script keeps its slot.
void AddScriptThing(entt::entity thing, bool createdByScript);
/// A script variable takes it (LHVM's reference callback, ADD_REFERENCE). The first reference sets IsInScript and, for
/// a thing the script created (or one already controlled), ControlledByScript.
void IncrementReference(entt::entity thing);
/// One reference less (the release waits for Process)
void DecrementReference(entt::entity thing);
/// Right after the scripts run each turn: every slot without a reference is released (ReleaseControlFromScript, then
/// out of the script) and freed.
void Process();

/// RELEASE_FROM_SCRIPT 159: a controlled thing is released: a container's controlled members first, then the thing
/// itself by its type (a villager DECIDE_WHAT_TO_DO and villager::ReleaseFromScript, an animal WANDER and
/// animal_ai::ReleaseFromScript); never deleted. The slot and its references stay
void ReleaseFromScript(entt::entity thing);

/// Whether a script holds a reference to it
[[nodiscard]] bool IsInScript(entt::entity thing);
/// Whether a script controls it
[[nodiscard]] bool IsControlledByScript(entt::entity thing);
/// Sets script control, also used by the vortex
void SetControlledByScript(entt::entity thing, bool controlled);
/// Cannot be eaten, survives an attack (components::CannotBeEaten)
[[nodiscard]] bool CannotBeEaten(entt::entity thing);
/// Marks it as not to be eaten (the landscape vortex, the puzzle objects)
void SetCannotBeEaten(entt::entity thing);

/// Sets a living thing's script state: a creature its own (TODO(creature)); a villager villager::SetScriptState, an animal
/// animal_ai::SetScriptState (available, in the map, the previous state stored, the state). FLOCK_ATTACH (27),
/// FLOCK_DISBAND (4) and the container loop of SET_SCRIPT_STATE call it; the single-object SET_SCRIPT_STATE tests
/// drowning first, this does not
void SetLivingScriptState(entt::entity living, uint32_t state);

/// The hunters' test (an animal hunting, moving to its prey): a target controlled by a script is only for a hunter
/// that is in a script itself
[[nodiscard]] bool MayTarget(entt::entity hunter, entt::entity target);

} // namespace openblack::ecs::script_held
