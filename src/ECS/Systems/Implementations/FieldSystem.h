/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FieldSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FieldSystem final: public FieldSystemInterface
{
public:
	void ProcessTurn(uint32_t turn) override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] std::optional<field_crop::Look> GetLook(const components::Field& field) const override;
	[[nodiscard]] field_crop::Activity GetActivity(entt::entity field) const override;
	[[nodiscard]] bool IsRipe(entt::entity field) const override;
	[[nodiscard]] std::optional<FieldChoice> BestFieldFor(entt::entity town, const glm::vec3& position) const override;
	void AddFarmer(entt::entity field, entt::entity villager) override;
	void RemoveFarmer(entt::entity field, entt::entity villager) override;
	bool PlantCrop(entt::entity field) override;
	uint32_t RemoveFood(entt::entity field, float amount) override;
};

} // namespace openblack::ecs::systems
