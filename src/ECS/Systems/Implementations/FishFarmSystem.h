/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FishFarmSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FishFarmSystem final: public FishFarmSystemInterface
{
public:
	void ProcessTurn(uint32_t turn) override;
	void Update(float seconds, glm::vec3 camera) override;
	void Scare(glm::vec3 point) override;
	[[nodiscard]] std::optional<entt::entity> FarmWithFishAt(glm::vec2 point) const override;
	[[nodiscard]] fish_farm::Type GetType() const override;
	uint32_t TakeFish(entt::entity farm, uint32_t wanted) override;
	void Reset() override;

private:
	/// What scared the fish since the shoals last swam: the last of it
	std::optional<glm::vec3> _scare;
};

} // namespace openblack::ecs::systems
