/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Components/LivingAction.h"
#include "ECS/Systems/LivingActionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class LivingActionSystem final: public LivingActionSystemInterface
{
public:
	void Update() override;
	void UpdatePoses(uint32_t turn, float turnFraction) override;

	[[nodiscard]] VillagerStates VillagerGetState(const components::LivingAction& action,
	                                              components::LivingAction::Index index) const override;
	void VillagerSetState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state,
	                      bool skipTransition) const override;
	uint32_t VillagerCallState(components::LivingAction& action, components::LivingAction::Index index) const override;
	bool VillagerCallEntryState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates src,
	                            VillagerStates dst) const override;
	bool VillagerCallExitState(components::LivingAction& action, components::LivingAction::Index index,
	                           VillagerStates next) const override;
	int VillagerCallOutOfAnimation(components::LivingAction& action, components::LivingAction::Index index) const override;
	bool VillagerCallValidate(components::LivingAction& action, components::LivingAction::Index index) const override;
	bool VillagerSetCurrentAndDestinationState(components::LivingAction& action, VillagerStates current,
	                                           VillagerStates destination) const override;
	void VillagerPlayAnimThenSetState(components::LivingAction& action, VillagerStates next) const override;
	void VillagerSetTopStateToFinal(components::LivingAction& action) const override;
	[[nodiscard]] bool VillagerIsReadyForNewAnimation(const components::LivingAction& action, uint32_t times) const override;
	[[nodiscard]] bool VillagerCanBeDirected(entt::entity villager) const override;
	void VillagerSetScriptState(entt::entity villager, VillagerStates state) const override;
	void VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal) const override;
	void VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const override;
	[[nodiscard]] bool VillagerHasPlayedScriptAnimation(entt::entity villager) const override;
	void VillagerFace(entt::entity villager, glm::vec2 point) const override;

private:
	enum class SetResult : uint8_t
	{
		Done,
		/// The state it is in won't let it go
		Refused,
		/// The new state won't have it
		EntryFailed,
	};

	/// Puts a state in with none of its rules
	void SetStateDirectly(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state) const;
	/// Whether the villager's top state, and the state it works towards, let it go into another
	[[nodiscard]] bool ExitAllowed(components::LivingAction& action, VillagerStates next) const;
	/// Goes into a state if the state will have it
	bool EnterState(components::LivingAction& action, VillagerStates state) const;
	/// Leaves its state for another, going on to a destination after it when given, with the clips of leaving one state
	/// and going into the next
	SetResult ChangeTopState(components::LivingAction& action, VillagerStates current,
	                         std::optional<VillagerStates> destination) const;
	/// Now and then a villager stops for a second before going into a state, more often when hurt
	bool PausesBefore(components::LivingAction& action, VillagerStates state) const;
	void SetTopState(components::LivingAction& action, VillagerStates state) const;

	/// The game's clock in milliseconds at the last frame the villagers were posed
	uint32_t _poseDrawTime {0};
};
} // namespace openblack::ecs::systems
