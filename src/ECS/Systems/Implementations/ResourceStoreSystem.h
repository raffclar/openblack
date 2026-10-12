/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include "ECS/Systems/ResourceStoreSystemInterface.h"

namespace openblack::ecs::components
{
struct BuildingSite;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class ResourceStoreSystem final: public ResourceStoreSystemInterface
{
public:
	[[nodiscard]] ObjectResource ResourceOf(entt::entity object) const override;
	[[nodiscard]] bool IsStore(entt::entity store, ResourceType type) const override;
	uint32_t AddToStore(entt::entity store, ResourceType type, uint32_t amount, std::optional<PlayerNames> giver, bool poisoned,
	                    std::optional<glm::vec3> at) override;
	uint32_t AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned) override;
	uint32_t TakeFromPile(entt::entity pile, ResourceType type, uint32_t amount, std::optional<PlayerNames> taker) override;
	bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver) override;
	bool PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player, bool poisoned) override;
	[[nodiscard]] std::optional<entt::entity> StoreOf(entt::entity pile) const override;
	[[nodiscard]] uint32_t GetResource(entt::entity object, ResourceType type) const override;
	uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, std::optional<glm::vec3> at) override;
	uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount) override;
	uint32_t AddToTownsSite(entt::entity building, entt::entity town, ResourceType type, uint32_t amount,
	                        std::optional<glm::vec3> at) override;
	uint32_t RemoveFromTownsSite(entt::entity building, entt::entity town, ResourceType type, uint32_t amount) override;

	/// One site at a building takes wood: a temple's onto its pile nearest where it is brought, any other's onto its one
	/// pile, made for the first wood. What it took.
	uint32_t AddToSite(entt::entity building, components::BuildingSite& site, ResourceType type, uint32_t amount,
	                   std::optional<glm::vec3> at);
	/// One site at a building gives up to an amount of wood from its piles. What it gave.
	uint32_t TakeFromSite(entt::entity building, components::BuildingSite& site, ResourceType type, uint32_t amount);

private:
	/// A storage pit takes what it will into its piles, without the giving's count in its town
	uint32_t FillPit(entt::entity store, ResourceType type, uint32_t amount, bool poisoned);
	/// The store's own count loses what was taken; taking counts against the town's owner, and is remembered of the taker
	uint32_t RemovedFromStore(entt::entity store, ResourceType type, uint32_t amount, std::optional<PlayerNames> taker);
	/// What a storage pit's piles give of what is asked, the last wood pile first
	uint32_t TakeFromPit(entt::entity store, ResourceType type, uint32_t amount, std::optional<PlayerNames> taker);

	/// The wood mulch sounds come one after another
	uint32_t _mulch {0};
};

} // namespace openblack::ecs::systems
