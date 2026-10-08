/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerFieldsInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's fields (ecs::fields)
class VillagerFields final: public VillagerFieldsInterface
{
public:
	[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town) const override;
	[[nodiscard]] float GetDesireToBeFarmed(entt::entity field) const override;
	[[nodiscard]] int GetFieldActivity(entt::entity field) const override;
	[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity field) const override;
	[[nodiscard]] map_coords::MapCoords RandomFarmPoint(entt::entity field) const override;
	[[nodiscard]] bool RipeFarmPoint(entt::entity field, map_coords::MapCoords& out) const override;
	bool PlantCrop(entt::entity field) override;
	[[nodiscard]] bool IsStillSowing(entt::entity field) const override;
	int32_t RemoveFood(entt::entity field, float amount) override;
	void AddFarmer(entt::entity field, entt::entity villager) override;
	void RemoveFarmer(entt::entity field, entt::entity villager) override;
	[[nodiscard]] bool IsField(entt::entity thing) const override;
};
} // namespace openblack::ecs::systems
