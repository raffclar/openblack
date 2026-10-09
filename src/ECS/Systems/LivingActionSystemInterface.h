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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/LivingAction.h"

namespace openblack::ecs::systems
{

class LivingActionSystemInterface
{
public:
	virtual void Update() = 0;
	/// Every frame: each villager's model is posed by the clip its state plays, advanced by the game's clock since the
	/// last frame, and the sounds of the clip's frames it passed are played
	virtual void UpdatePoses(uint32_t turn, float turnFraction) = 0;

	[[nodiscard]] virtual VillagerStates VillagerGetState(const components::LivingAction& action,
	                                                      components::LivingAction::Index index) const = 0;
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

	/// Whether a script can direct the villager: it is alive, on the land and not drowning
	[[nodiscard]] virtual bool VillagerCanBeDirected(entt::entity villager) const = 0;
	/// A script puts the villager straight into a state
	virtual void VillagerSetScriptState(entt::entity villager, VillagerStates state) const = 0;
	/// A script sends the villager to a point, where it waits for the script
	virtual void VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal) const = 0;
	/// The clip a script asks the villager to play in the playing state, and how many times
	virtual void VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const = 0;
	/// Whether the villager has played the clip a script asked for
	[[nodiscard]] virtual bool VillagerHasPlayedScriptAnimation(entt::entity villager) const = 0;
	/// The villager turns at once to face a point
	virtual void VillagerFace(entt::entity villager, glm::vec2 point) const = 0;
};

} // namespace openblack::ecs::systems
