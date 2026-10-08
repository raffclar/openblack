/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerDiscipleJobsInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's job checks of the villager states
class VillagerDiscipleJobs final: public VillagerDiscipleJobsInterface
{
public:
	[[nodiscard]] uint32_t DiscipleJob(entt::entity villager, uint8_t disciple) override;
};
} // namespace openblack::ecs::systems
