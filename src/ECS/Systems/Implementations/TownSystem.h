/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/signal/sigh.hpp>

#include "ECS/Systems/TownSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class TownSystem final: public TownSystemInterface
{
public:
	/// Keeps the towns' lists of buildings and homeless people as they go from the world
	TownSystem();
	virtual ~TownSystem();
	TownSystem(const TownSystem&) = delete;
	TownSystem& operator=(const TownSystem&) = delete;

	[[nodiscard]] entt::entity FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const override;
	[[nodiscard]] entt::entity FindClosestTown(const glm::vec3& point) const override;
	bool AddVillagerToTown(entt::entity town, entt::entity villager) override;
	void AddVillagerToAbode(entt::entity abode, entt::entity villager) override;
	bool MakeHomeless(entt::entity villager) override;
	[[nodiscard]] bool CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter,
	                                     entt::entity ignore) const override;
	[[nodiscard]] std::optional<glm::vec2> FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
	                                                     ClearAreaFilter filter, entt::entity ignore) const override;
	glm::vec2 GetCongregationPos(entt::entity town) override;
	void BuildingCreated(entt::entity town, glm::vec2 position, float radius) override;
	[[nodiscard]] bool IsInStateOfEmergency(entt::entity town) const override;
	void SetInStateOfEmergency(entt::entity town) override;
	void ProcessTurn() override;
	void ClaimTown(entt::entity town, PlayerNames player) override;

private:
	/// A building or a person gone from the world leaves its town's lists
	void OnAbodeGone(entt::registry& registry, entt::entity abode);
	void OnVillagerGone(entt::registry& registry, entt::entity villager);

	Registry* _registry {nullptr};
	std::vector<entt::connection> _connections;
};
} // namespace openblack::ecs::systems
