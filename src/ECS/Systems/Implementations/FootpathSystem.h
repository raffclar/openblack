/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FootpathSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FootpathSystem final: public FootpathSystemInterface
{
public:
	void BuildingWorking(entt::entity building) override;
	void TownJoinedSite(entt::entity site, entt::entity town) override;
	void DanceMade(entt::entity dance, entt::entity town) override;
	void ProcessTurn() override;
	[[nodiscard]] std::optional<std::vector<glm::ivec2>> PlanBetween(glm::ivec2 from, glm::ivec2 to) const override;
};

} // namespace openblack::ecs::systems
