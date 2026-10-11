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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager at an artefact's dance: waiting by the artefact for the dance to start, then dancing it. The state
/// functions are the living action system's.
namespace openblack::ecs::villager_artefact_dance
{

/// Waiting for the dance: from farther than 12 m it walks to a clear spot nearer, on its own side of the artefact;
/// there it makes itself room on its first turn if it has none, and once it has waited its time the dance starts. A
/// villager that has lost its dance decides what to do.
uint32_t WaitForArtefactDance(components::LivingAction& action);
/// Going into the wait, it starts counting its time afresh
bool EnterWaitForArtefactDance(components::LivingAction& action, VillagerStates previous, VillagerStates next);
/// Dancing the artefact's dance: a dancer standing at the dance's middle walks to its place, any other dances there
uint32_t ArtefactDance(components::LivingAction& action);
/// Leaving a dance for a state that isn't another dance's, it leaves the dance; it is never kept back
bool ExitDance(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_artefact_dance
