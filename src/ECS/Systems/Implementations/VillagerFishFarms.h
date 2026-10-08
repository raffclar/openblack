/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerFishFarmsInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's fish farms (ecs::fish_farms), the calendar's season and the town owner's tribal power
class VillagerFishFarms final: public VillagerFishFarmsInterface
{
public:
	[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town) const override;
	[[nodiscard]] int32_t Score(entt::entity farm) const override;
	[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity farm) const override;
	[[nodiscard]] map_coords::MapCoords FishingSpot(entt::entity farm) const override;
	[[nodiscard]] uint32_t FishermanCount(entt::entity farm) const override;
	[[nodiscard]] bool HasFisherman(entt::entity farm, entt::entity villager) const override;
	void AddFisherman(entt::entity farm, entt::entity villager) override;
	void RemoveFisherman(entt::entity farm, entt::entity villager) override;
	[[nodiscard]] bool IsAvailable(entt::entity farm) const override;
	[[nodiscard]] uint32_t Season() const override;
	[[nodiscard]] float TribalPower(entt::entity villager) const override;
};
} // namespace openblack::ecs::systems
