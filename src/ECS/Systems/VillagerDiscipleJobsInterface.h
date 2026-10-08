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

namespace openblack::ecs::systems
{
/// The job a disciple looks for when it decides what to do
class VillagerDiscipleJobsInterface
{
public:
	virtual ~VillagerDiscipleJobsInterface() = default;

	/// The job check of the disciple type (forester, fisherman, builder, breeder, craftsman, trader); 0 for the others
	[[nodiscard]] virtual uint32_t DiscipleJob(entt::entity villager, uint8_t disciple) = 0;
};
} // namespace openblack::ecs::systems
