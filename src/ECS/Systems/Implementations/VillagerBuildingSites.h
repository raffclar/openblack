/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/VillagerBuildingSitesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The game's building sites and abodes (ecs::building_sites, ecs::abodes), object contact and land alignment
class VillagerBuildingSites final: public VillagerBuildingSitesInterface
{
public:
	[[nodiscard]] bool IsBuildingHappening(entt::entity town) const override;
	[[nodiscard]] entt::entity GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos,
	                                               bool includeFull) const override;
	[[nodiscard]] entt::entity GetBestRepairBuildingSite(entt::entity town) const override;
	[[nodiscard]] bool IsBuildingSiteValid(entt::entity town, entt::entity site) const override;
	[[nodiscard]] entt::entity GetBuildingSiteInList(entt::entity town, entt::entity building) const override;
	entt::entity AddBuildingSite(entt::entity town, entt::entity building) override;
	bool RequestBestPlanned(entt::entity town) override;
	bool RequestANewAbode(entt::entity town) override;
	void AddWoodUsedForBuilding(entt::entity town, uint32_t wood) override;
	[[nodiscard]] entt::entity GetBuilding(entt::entity site) const override;
	[[nodiscard]] bool NeedsBuilders(entt::entity site) const override;
	[[nodiscard]] bool IsBuilder(entt::entity site, entt::entity villager) const override;
	[[nodiscard]] int32_t GetBuilderCount(entt::entity site) const override;
	[[nodiscard]] float GetClearAreaRadius(entt::entity site) const override;
	[[nodiscard]] float GetWoodValue(entt::entity site) const override;
	[[nodiscard]] bool ShouldIGetWood(entt::entity site, entt::entity villager,
	                                  const std::function<map_coords::MapCoords()>& resourceDropoffPos) const override;
	[[nodiscard]] uint32_t GetResource(entt::entity site, ResourceType type) const override;
	uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos) override;
	uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount) override;
	void BuildBy(entt::entity site, float amount) override;
	[[nodiscard]] bool IsAvailable(entt::entity site) const override;
	[[nodiscard]] map_coords::MapCoords GetRandomBuildPos(entt::entity site, entt::entity villager,
	                                                      int32_t& index) const override;
	[[nodiscard]] map_coords::MapCoords GetNextPosFromIndex(entt::entity site, int32_t& index) const override;
	[[nodiscard]] std::optional<map_coords::MapCoords> GetBuildPos(entt::entity site, int32_t index) const override;
	void AddBuilder(entt::entity site, entt::entity villager) override;
	void RemoveBuilder(entt::entity site, entt::entity villager) override;
	[[nodiscard]] bool IsBuilt(entt::entity building) const override;
	[[nodiscard]] bool IsRepaired(entt::entity building) const override;
	[[nodiscard]] bool IsTouching(entt::entity villager, entt::entity building, float margin) const override;
	[[nodiscard]] float LandAlignmentAt(const glm::vec3& position) const override;
};
} // namespace openblack::ecs::systems
