/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	/// Registers the livings' physics handlers (ECS/LivingPhysics)
	LivingActionSystem();

	/// After the one living list (living_turn::ProcessLiving, ECS/LivingTurn.h): the souls of the dead
	void Update() override;

	[[nodiscard]] VillagerStates VillagerGetState(const components::LivingAction& action,
	                                              components::LivingAction::Index index) const override;
	void VillagerSetState(components::LivingAction& action, components::LivingAction::Index index, VillagerStates state,
	                      bool skipTransition) const override;
	uint32_t VillagerCallState(components::LivingAction& action, components::LivingAction::Index index) const override;
	/// The entry function of the table row `row`, told the final state from before the change and the state
	/// entered: 1 = accepted, 0x23 = accepted and the states set by it, else refused. An empty slot is 1 (no function)
	uint32_t VillagerCallEntry(components::LivingAction& action, VillagerStates row, VillagerStates final,
	                           VillagerStates next) const override;
	/// The exit function of the table row `row`, told the state that follows: 1 = it may leave. Empty: 1
	uint32_t VillagerCallExit(components::LivingAction& action, VillagerStates row, VillagerStates next) const override;
	int VillagerCallOutOfAnimation(components::LivingAction& action, components::LivingAction::Index index) const override;
	bool VillagerCallValidate(components::LivingAction& action, components::LivingAction::Index index) const override;
};
} // namespace openblack::ecs::systems
