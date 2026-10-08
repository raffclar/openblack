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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::systems
{
/// The building side as the builders see it: the town's sites, a site's ring, pile and builders, and the building
class VillagerBuildingSitesInterface
{
public:
	virtual ~VillagerBuildingSitesInterface() = default;

	[[nodiscard]] virtual bool IsBuildingHappening(entt::entity town) const = 0;
	[[nodiscard]] virtual entt::entity GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos,
	                                                       bool includeFull) const = 0;
	[[nodiscard]] virtual entt::entity GetBestRepairBuildingSite(entt::entity town) const = 0;
	[[nodiscard]] virtual bool IsBuildingSiteValid(entt::entity town, entt::entity site) const = 0;
	[[nodiscard]] virtual entt::entity GetBuildingSiteInList(entt::entity town, entt::entity building) const = 0;
	virtual entt::entity AddBuildingSite(entt::entity town, entt::entity building) = 0;
	[[nodiscard]] virtual bool RequestBestPlanned(entt::entity town) = 0;
	[[nodiscard]] virtual bool RequestANewAbode(entt::entity town) = 0;
	virtual void AddWoodUsedForBuilding(entt::entity town, uint32_t wood) = 0;
	[[nodiscard]] virtual entt::entity GetBuilding(entt::entity site) const = 0;
	[[nodiscard]] virtual bool NeedsBuilders(entt::entity site) const = 0;
	[[nodiscard]] virtual bool IsBuilder(entt::entity site, entt::entity villager) const = 0;
	[[nodiscard]] virtual int32_t GetBuilderCount(entt::entity site) const = 0;
	[[nodiscard]] virtual float GetClearAreaRadius(entt::entity site) const = 0;
	[[nodiscard]] virtual float GetWoodValue(entt::entity site) const = 0;
	[[nodiscard]] virtual bool ShouldIGetWood(entt::entity site, entt::entity villager,
	                                          const std::function<map_coords::MapCoords()>& resourceDropoffPos) const = 0;
	[[nodiscard]] virtual uint32_t GetResource(entt::entity site, ResourceType type) const = 0;
	virtual uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, const map_coords::MapCoords* pos) = 0;
	[[nodiscard]] virtual uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount) = 0;
	virtual void BuildBy(entt::entity site, float amount) = 0;
	[[nodiscard]] virtual bool IsAvailable(entt::entity site) const = 0;
	/// A random point of the site's ring for the villager; index receives its slot
	[[nodiscard]] virtual map_coords::MapCoords GetRandomBuildPos(entt::entity site, entt::entity villager,
	                                                              int32_t& index) const = 0;
	/// The ring point after index; index moves on to it
	[[nodiscard]] virtual map_coords::MapCoords GetNextPosFromIndex(entt::entity site, int32_t& index) const = 0;
	[[nodiscard]] virtual std::optional<map_coords::MapCoords> GetBuildPos(entt::entity site, int32_t index) const = 0;
	virtual void AddBuilder(entt::entity site, entt::entity villager) = 0;
	virtual void RemoveBuilder(entt::entity site, entt::entity villager) = 0;
	[[nodiscard]] virtual bool IsBuilt(entt::entity building) const = 0;
	[[nodiscard]] virtual bool IsRepaired(entt::entity building) const = 0;
	[[nodiscard]] virtual bool IsTouching(entt::entity villager, entt::entity building, float margin) const = 0;
	[[nodiscard]] virtual float LandAlignmentAt(const glm::vec3& position) const = 0;
};
} // namespace openblack::ecs::systems
