/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/DanceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class DanceSystem final: public DanceSystemInterface
{
public:
	entt::entity Create(uint32_t type, const glm::vec3& place, entt::entity centre, uint32_t durationTurns,
	                    bool madeByScript) override;
	void Destroy(entt::entity dance) override;
	void ProcessTurn() override;
};

} // namespace openblack::ecs::systems
