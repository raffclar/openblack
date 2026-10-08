/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerStateSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class VillagerStateSystem final: public VillagerStateSystemInterface
{
public:
	[[nodiscard]] entt::entity& EndingPhysics() override { return _endingPhysics; }
	[[nodiscard]] std::unordered_map<uint32_t, uint32_t>& MourningTakers() override { return _mourningTakers; }

private:
	entt::entity _endingPhysics {entt::null};
	std::unordered_map<uint32_t, uint32_t> _mourningTakers;
};
} // namespace openblack::ecs::systems
