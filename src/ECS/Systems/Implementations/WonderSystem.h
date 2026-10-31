/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WonderSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class WonderSystem final: public WonderSystemInterface
{
public:
	void Made(entt::entity building, float scale) override;
	void Built(entt::entity building) override;
	void Lost(entt::entity building) override;
	void TownLeavesPlayer(entt::entity town) override;
	void TownJoinsPlayer(entt::entity town) override;
	void SetPower(entt::entity building, float power) override;
	[[nodiscard]] float PowerAt(entt::entity town, glm::vec3 place) const override;
	[[nodiscard]] float ScaleAt(entt::entity town, glm::vec3 place) const override;
	void ProcessTurn() override;
};

} // namespace openblack::ecs::systems
