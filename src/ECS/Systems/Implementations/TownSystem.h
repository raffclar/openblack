/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>
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
	[[nodiscard]] entt::entity NearestBuilding(glm::vec2 point) const override;
	[[nodiscard]] entt::entity NearestTown(glm::vec2 point, float reach,
	                                       const std::function<bool(entt::entity)>& accepts) const override;
	bool AddVillagerToTown(entt::entity town, entt::entity villager) override;
	void AddVillagerToAbode(entt::entity abode, entt::entity villager) override;
	bool MakeHomeless(entt::entity villager) override;
	void MoveInAnyway(entt::entity abode, entt::entity villager) override;
	bool MoveInByBuilding(entt::entity abode, entt::entity villager) override;
	bool MoveIntoTown(entt::entity town, entt::entity villager) override;
	town_homes::NewHome LookForBetterHome(entt::entity villager) override;
	[[nodiscard]] bool HasRoomForNewcomer(entt::entity town) const override;
	void RemoveDeadVillager(entt::entity villager) override;
	void RemoveVillager(entt::entity villager) override;
	[[nodiscard]] std::vector<entt::entity> ChildrenOf(entt::entity town, entt::entity mother) const override;
	[[nodiscard]] bool CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter,
	                                     entt::entity ignore) const override;
	[[nodiscard]] std::optional<glm::vec2> FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
	                                                     ClearAreaFilter filter, entt::entity ignore) const override;
	glm::vec2 GetCongregationPos(entt::entity town) override;
	void BuildingCreated(entt::entity town, glm::vec2 position, float radius) override;
	void StructuresChanged(entt::entity town) override;
	[[nodiscard]] bool IsInStateOfEmergency(entt::entity town) const override;
	void SetInStateOfEmergency(entt::entity town) override;
	void ProcessTurn() override;
	void ProcessTownBuildings(entt::entity town) override;
	void ProcessTownTurn(entt::entity town) override;
	[[nodiscard]] std::vector<entt::entity> TownsInGainedOrder(PlayerNames player) const override;
	void MarkDesiresChanged(entt::entity town) override;
	[[nodiscard]] bool DesiresChanged(entt::entity town) const override;
	void ClaimTown(entt::entity town, PlayerNames player) override;
	std::optional<TemporaryStore> TemporaryStorePile(entt::entity town, glm::vec2 from, ResourceType type) override;
	void SetStoragePit(entt::entity town) override;
	void UpdateFrame(float gameSeconds) override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

private:
	/// A building or a person gone from the world leaves its town's lists
	void OnAbodeGone(entt::registry& registry, entt::entity abode);
	void OnFieldGone(entt::registry& registry, entt::entity field);
	/// Make the town's ground again, without a field that is about to go
	void StructuresChanged(entt::entity town, entt::entity leaving);
	void OnVillagerGone(entt::registry& registry, entt::entity villager);
	/// The people of a town
	[[nodiscard]] static uint32_t PopulationOf(entt::entity town);
	/// A person has left the town: with nobody left, its countdown to an empty town starts
	static void StartEmptyTownTimerIfEmpty(entt::entity town);
	/// The town stayed empty: its belief starts over and it goes back to the neutral player
	void SetTownEmpty(entt::entity town);
	/// At the town's turn it forgets its temporary piles that have gone, and removes the empty ones once it has a
	/// storage pit that works
	static void TemporaryStoresTurn(entt::entity town);

	Registry* _registry {nullptr};
	std::vector<entt::connection> _connections;
	/// Each village centre's effect of its symbols of belief
	std::unordered_map<entt::entity, uint32_t> _beliefSymbols;
};
} // namespace openblack::ecs::systems
