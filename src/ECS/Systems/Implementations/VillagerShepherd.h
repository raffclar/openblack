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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A shepherd herds one of its town's flocks that no one herds. It walks to the flock and up to its leader, joining the
/// flock, and looks the flock over. Then, if its town wants food, it leads the nearest animal to the store in short legs
/// at the animal's pace, waits inside the door and takes the animal as food, which it puts in the store. Otherwise it
/// takes the flock to drink at its home's water, waits there with it, brings it back, and goes home. The state
/// functions are the living action system's.
namespace openblack::ecs::villager_shepherd
{

/// The flock of the villager's town it would herd, and how much it wants to; none when no flock is near enough
struct FlockChoice
{
	entt::entity flock {entt::null};
	float score {0.0f};
};
[[nodiscard]] std::optional<FlockChoice> FindBestFlock(components::LivingAction& action);

/// The villager takes up herding a flock, or the first of its town's flocks no one herds when none is given, and
/// walks to it. 1 when it went.
uint32_t VillagerBecomesShepherd(components::LivingAction& action, entt::entity flock);

uint32_t ShepherdLookForFlock(components::LivingAction& action);
uint32_t ShepherdMoveFlockToWater(components::LivingAction& action);
uint32_t ShepherdMoveFlockToFood(components::LivingAction& action);
uint32_t ShepherdMoveFlockBack(components::LivingAction& action);
uint32_t ShepherdDecideWhatToDoWithFlock(components::LivingAction& action);
uint32_t ShepherdWaitForFlock(components::LivingAction& action);
uint32_t ShepherdSlaughterAnimal(components::LivingAction& action);
uint32_t ShepherdFetchStray(components::LivingAction& action);
uint32_t ShepherdGotoFlock(components::LivingAction& action);
uint32_t ShepherdTakeAnimalForSlaughter(components::LivingAction& action);
uint32_t ShepherdTakesControlOfFlock(components::LivingAction& action);
uint32_t ShepherdReleasesControlOfFlock(components::LivingAction& action);
uint32_t ShepherdCheckAnimalForSlaughter(components::LivingAction& action);
/// Leaving the shepherd's work for anything else, it puts its flock's place back, leaves the flock and stops herding it
bool ExitShepherding(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_shepherd
