/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

#include "ECS/Components/LivingAction.h"

namespace openblack::ecs::systems
{

class LivingActionSystemInterface
{
public:
	virtual void Update() = 0;
	/// Every frame: the clip each villager's state plays is advanced by the game's clock since the last frame, the
	/// sounds of the clip's frames it passed are played, and the keyframes it has reached are kept for posing it
	virtual void UpdatePoses(uint32_t turn, float turnFraction) = 0;
	/// Every frame, once the camera has moved: the villagers the camera sees, or sees reflected in the sea, are posed
	/// between the keyframes their clips have reached
	virtual void PoseVillagersInView(const glm::mat4& viewProjection) = 0;

	[[nodiscard]] virtual VillagerStates VillagerGetState(const components::LivingAction& action,
	                                                      components::LivingAction::Index index) const = 0;
	/// The state the villager works towards: its top state when that is a final one, else its final state
	[[nodiscard]] virtual VillagerStates VillagerGetFinalState(const components::LivingAction& action) const = 0;
	virtual void VillagerSetState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state,
	                              bool skipTransition) const = 0;
	virtual uint32_t VillagerCallState(components::LivingAction& action, components::LivingAction::Index index) const = 0;
	virtual bool VillagerCallEntryState(components::LivingAction& action, components::LivingAction::Index index,
	                                    VillagerStates src, VillagerStates dst) const = 0;
	virtual bool VillagerCallExitState(components::LivingAction& action, components::LivingAction::Index index,
	                                   VillagerStates next) const = 0;
	virtual int VillagerCallOutOfAnimation(components::LivingAction& action, components::LivingAction::Index index) const = 0;
	virtual bool VillagerCallValidate(components::LivingAction& action, components::LivingAction::Index index) const = 0;
	/// Goes into a state on the way to another, as the game sets both at once. Whether it went.
	virtual bool VillagerSetCurrentAndDestinationState(components::LivingAction& action, VillagerStates current,
	                                                   VillagerStates destination) const = 0;
	/// Lets the clip the villager plays now play to its end, then goes into the next state
	virtual void VillagerPlayAnimThenSetState(components::LivingAction& action, VillagerStates next) const = 0;
	/// Goes on into the state the villager works towards
	virtual void VillagerSetTopStateToFinal(components::LivingAction& action) const = 0;
	/// Whether the villager's clip has played through so many times since its state changed
	[[nodiscard]] virtual bool VillagerIsReadyForNewAnimation(const components::LivingAction& action, uint32_t times) const = 0;
};

} // namespace openblack::ecs::systems
