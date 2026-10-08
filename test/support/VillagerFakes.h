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
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Systems/VillagerBuildingSitesInterface.h"
#include "ECS/Systems/VillagerChildFactoryInterface.h"
#include "ECS/Systems/VillagerDiscipleJobsInterface.h"
#include "ECS/Systems/VillagerFieldsInterface.h"
#include "ECS/Systems/VillagerFishFarmsInterface.h"
#include "ECS/Systems/VillagerRulesInterface.h"
#include "ECS/Systems/VillagerStoresInterface.h"
#include "ECS/Systems/VillagerTentQueriesInterface.h"
#include "ECS/Systems/VillagerWorldQueriesInterface.h"
#include "ECS/Systems/VillagerWorshipCheckInterface.h"
#include "ECS/Town/TownStores.h"
#include "Enums.h"
#include "FakeCallOr.h"

// Fakes of the villager services for the tests. Each fake has one std::function per method, named like the method in
// camelCase; a test sets the ones it needs and injects the fake with Locator::<service>::emplace<Fake>(fake). A
// function left empty calls the fallback service given to the constructor, or throws std::bad_function_call without
// one.

namespace openblack::test
{
class FakeVillagerFields final: public ecs::systems::VillagerFieldsInterface
{
public:
	using Interface = ecs::systems::VillagerFieldsInterface;

	FakeVillagerFields() = default;
	explicit FakeVillagerFields(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<std::vector<entt::entity>(entt::entity town)> townFields;
	std::function<float(entt::entity field)> getDesireToBeFarmed;
	std::function<int(entt::entity field)> getFieldActivity;
	std::function<map_coords::MapCoords(entt::entity field)> getArrivePos;
	std::function<map_coords::MapCoords(entt::entity field)> randomFarmPoint;
	std::function<bool(entt::entity field, map_coords::MapCoords& out)> ripeFarmPoint;
	std::function<bool(entt::entity field)> plantCrop;
	std::function<bool(entt::entity field)> isStillSowing;
	std::function<int32_t(entt::entity field, float amount)> removeFood;
	std::function<void(entt::entity field, entt::entity villager)> addFarmer;
	std::function<void(entt::entity field, entt::entity villager)> removeFarmer;
	std::function<bool(entt::entity thing)> isField;

	[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town) const override
	{
		return detail::CallOr(townFields, _fallback, &Interface::TownFields, town);
	}
	[[nodiscard]] float GetDesireToBeFarmed(entt::entity field) const override
	{
		return detail::CallOr(getDesireToBeFarmed, _fallback, &Interface::GetDesireToBeFarmed, field);
	}
	[[nodiscard]] int GetFieldActivity(entt::entity field) const override
	{
		return detail::CallOr(getFieldActivity, _fallback, &Interface::GetFieldActivity, field);
	}
	[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity field) const override
	{
		return detail::CallOr(getArrivePos, _fallback, &Interface::GetArrivePos, field);
	}
	[[nodiscard]] map_coords::MapCoords RandomFarmPoint(entt::entity field) const override
	{
		return detail::CallOr(randomFarmPoint, _fallback, &Interface::RandomFarmPoint, field);
	}
	[[nodiscard]] bool RipeFarmPoint(entt::entity field, map_coords::MapCoords& out) const override
	{
		return detail::CallOr(ripeFarmPoint, _fallback, &Interface::RipeFarmPoint, field, out);
	}
	bool PlantCrop(entt::entity field) override { return detail::CallOr(plantCrop, _fallback, &Interface::PlantCrop, field); }
	[[nodiscard]] bool IsStillSowing(entt::entity field) const override
	{
		return detail::CallOr(isStillSowing, _fallback, &Interface::IsStillSowing, field);
	}
	int32_t RemoveFood(entt::entity field, float amount) override
	{
		return detail::CallOr(removeFood, _fallback, &Interface::RemoveFood, field, amount);
	}
	void AddFarmer(entt::entity field, entt::entity villager) override
	{
		detail::CallOr(addFarmer, _fallback, &Interface::AddFarmer, field, villager);
	}
	void RemoveFarmer(entt::entity field, entt::entity villager) override
	{
		detail::CallOr(removeFarmer, _fallback, &Interface::RemoveFarmer, field, villager);
	}
	[[nodiscard]] bool IsField(entt::entity thing) const override
	{
		return detail::CallOr(isField, _fallback, &Interface::IsField, thing);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerFishFarms final: public ecs::systems::VillagerFishFarmsInterface
{
public:
	using Interface = ecs::systems::VillagerFishFarmsInterface;

	FakeVillagerFishFarms() = default;
	explicit FakeVillagerFishFarms(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<std::vector<entt::entity>(entt::entity town)> townFishFarms;
	std::function<int32_t(entt::entity farm)> score;
	std::function<map_coords::MapCoords(entt::entity farm)> getArrivePos;
	std::function<map_coords::MapCoords(entt::entity farm)> fishingSpot;
	std::function<uint32_t(entt::entity farm)> fishermanCount;
	std::function<bool(entt::entity farm, entt::entity villager)> hasFisherman;
	std::function<void(entt::entity farm, entt::entity villager)> addFisherman;
	std::function<void(entt::entity farm, entt::entity villager)> removeFisherman;
	std::function<bool(entt::entity farm)> isAvailable;
	std::function<uint32_t()> season;
	std::function<float(entt::entity villager)> tribalPower;

	[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town) const override
	{
		return detail::CallOr(townFishFarms, _fallback, &Interface::TownFishFarms, town);
	}
	[[nodiscard]] int32_t Score(entt::entity farm) const override
	{
		return detail::CallOr(score, _fallback, &Interface::Score, farm);
	}
	[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity farm) const override
	{
		return detail::CallOr(getArrivePos, _fallback, &Interface::GetArrivePos, farm);
	}
	[[nodiscard]] map_coords::MapCoords FishingSpot(entt::entity farm) const override
	{
		return detail::CallOr(fishingSpot, _fallback, &Interface::FishingSpot, farm);
	}
	[[nodiscard]] uint32_t FishermanCount(entt::entity farm) const override
	{
		return detail::CallOr(fishermanCount, _fallback, &Interface::FishermanCount, farm);
	}
	[[nodiscard]] bool HasFisherman(entt::entity farm, entt::entity villager) const override
	{
		return detail::CallOr(hasFisherman, _fallback, &Interface::HasFisherman, farm, villager);
	}
	void AddFisherman(entt::entity farm, entt::entity villager) override
	{
		detail::CallOr(addFisherman, _fallback, &Interface::AddFisherman, farm, villager);
	}
	void RemoveFisherman(entt::entity farm, entt::entity villager) override
	{
		detail::CallOr(removeFisherman, _fallback, &Interface::RemoveFisherman, farm, villager);
	}
	[[nodiscard]] bool IsAvailable(entt::entity farm) const override
	{
		return detail::CallOr(isAvailable, _fallback, &Interface::IsAvailable, farm);
	}
	[[nodiscard]] uint32_t Season() const override { return detail::CallOr(season, _fallback, &Interface::Season); }
	[[nodiscard]] float TribalPower(entt::entity villager) const override
	{
		return detail::CallOr(tribalPower, _fallback, &Interface::TribalPower, villager);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerBuildingSites final: public ecs::systems::VillagerBuildingSitesInterface
{
public:
	using Interface = ecs::systems::VillagerBuildingSitesInterface;
	using MapCoords = map_coords::MapCoords;

	FakeVillagerBuildingSites() = default;
	explicit FakeVillagerBuildingSites(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<bool(entt::entity town)> isBuildingHappening;
	std::function<entt::entity(entt::entity town, const MapCoords& pos, bool includeFull)> getBestBuildingSite;
	std::function<entt::entity(entt::entity town)> getBestRepairBuildingSite;
	std::function<bool(entt::entity town, entt::entity site)> isBuildingSiteValid;
	std::function<entt::entity(entt::entity town, entt::entity building)> getBuildingSiteInList;
	std::function<entt::entity(entt::entity town, entt::entity building)> addBuildingSite;
	std::function<bool(entt::entity town)> requestBestPlanned;
	std::function<bool(entt::entity town)> requestANewAbode;
	std::function<void(entt::entity town, uint32_t wood)> addWoodUsedForBuilding;
	std::function<entt::entity(entt::entity site)> getBuilding;
	std::function<bool(entt::entity site)> needsBuilders;
	std::function<bool(entt::entity site, entt::entity villager)> isBuilder;
	std::function<int32_t(entt::entity site)> getBuilderCount;
	std::function<float(entt::entity site)> getClearAreaRadius;
	std::function<float(entt::entity site)> getWoodValue;
	std::function<bool(entt::entity site, entt::entity villager, const std::function<MapCoords()>& resourceDropoffPos)>
	    shouldIGetWood;
	std::function<uint32_t(entt::entity site, ResourceType type)> getResource;
	std::function<uint32_t(entt::entity site, ResourceType type, uint32_t amount, const MapCoords* pos)> addResource;
	std::function<uint32_t(entt::entity site, ResourceType type, uint32_t amount)> removeResource;
	std::function<void(entt::entity site, float amount)> buildBy;
	std::function<bool(entt::entity site)> isAvailable;
	std::function<MapCoords(entt::entity site, entt::entity villager, int32_t& index)> getRandomBuildPos;
	std::function<MapCoords(entt::entity site, int32_t& index)> getNextPosFromIndex;
	std::function<std::optional<MapCoords>(entt::entity site, int32_t index)> getBuildPos;
	std::function<void(entt::entity site, entt::entity villager)> addBuilder;
	std::function<void(entt::entity site, entt::entity villager)> removeBuilder;
	std::function<bool(entt::entity building)> isBuilt;
	std::function<bool(entt::entity building)> isRepaired;
	std::function<bool(entt::entity villager, entt::entity building, float margin)> isTouching;
	std::function<float(const glm::vec3& position)> landAlignmentAt;

	[[nodiscard]] bool IsBuildingHappening(entt::entity town) const override
	{
		return detail::CallOr(isBuildingHappening, _fallback, &Interface::IsBuildingHappening, town);
	}
	[[nodiscard]] entt::entity GetBestBuildingSite(entt::entity town, const MapCoords& pos, bool includeFull) const override
	{
		return detail::CallOr(getBestBuildingSite, _fallback, &Interface::GetBestBuildingSite, town, pos, includeFull);
	}
	[[nodiscard]] entt::entity GetBestRepairBuildingSite(entt::entity town) const override
	{
		return detail::CallOr(getBestRepairBuildingSite, _fallback, &Interface::GetBestRepairBuildingSite, town);
	}
	[[nodiscard]] bool IsBuildingSiteValid(entt::entity town, entt::entity site) const override
	{
		return detail::CallOr(isBuildingSiteValid, _fallback, &Interface::IsBuildingSiteValid, town, site);
	}
	[[nodiscard]] entt::entity GetBuildingSiteInList(entt::entity town, entt::entity building) const override
	{
		return detail::CallOr(getBuildingSiteInList, _fallback, &Interface::GetBuildingSiteInList, town, building);
	}
	entt::entity AddBuildingSite(entt::entity town, entt::entity building) override
	{
		return detail::CallOr(addBuildingSite, _fallback, &Interface::AddBuildingSite, town, building);
	}
	bool RequestBestPlanned(entt::entity town) override
	{
		return detail::CallOr(requestBestPlanned, _fallback, &Interface::RequestBestPlanned, town);
	}
	bool RequestANewAbode(entt::entity town) override
	{
		return detail::CallOr(requestANewAbode, _fallback, &Interface::RequestANewAbode, town);
	}
	void AddWoodUsedForBuilding(entt::entity town, uint32_t wood) override
	{
		detail::CallOr(addWoodUsedForBuilding, _fallback, &Interface::AddWoodUsedForBuilding, town, wood);
	}
	[[nodiscard]] entt::entity GetBuilding(entt::entity site) const override
	{
		return detail::CallOr(getBuilding, _fallback, &Interface::GetBuilding, site);
	}
	[[nodiscard]] bool NeedsBuilders(entt::entity site) const override
	{
		return detail::CallOr(needsBuilders, _fallback, &Interface::NeedsBuilders, site);
	}
	[[nodiscard]] bool IsBuilder(entt::entity site, entt::entity villager) const override
	{
		return detail::CallOr(isBuilder, _fallback, &Interface::IsBuilder, site, villager);
	}
	[[nodiscard]] int32_t GetBuilderCount(entt::entity site) const override
	{
		return detail::CallOr(getBuilderCount, _fallback, &Interface::GetBuilderCount, site);
	}
	[[nodiscard]] float GetClearAreaRadius(entt::entity site) const override
	{
		return detail::CallOr(getClearAreaRadius, _fallback, &Interface::GetClearAreaRadius, site);
	}
	[[nodiscard]] float GetWoodValue(entt::entity site) const override
	{
		return detail::CallOr(getWoodValue, _fallback, &Interface::GetWoodValue, site);
	}
	[[nodiscard]] bool ShouldIGetWood(entt::entity site, entt::entity villager,
	                                  const std::function<MapCoords()>& resourceDropoffPos) const override
	{
		return detail::CallOr(shouldIGetWood, _fallback, &Interface::ShouldIGetWood, site, villager, resourceDropoffPos);
	}
	[[nodiscard]] uint32_t GetResource(entt::entity site, ResourceType type) const override
	{
		return detail::CallOr(getResource, _fallback, &Interface::GetResource, site, type);
	}
	uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, const MapCoords* pos) override
	{
		return detail::CallOr(addResource, _fallback, &Interface::AddResource, site, type, amount, pos);
	}
	uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount) override
	{
		return detail::CallOr(removeResource, _fallback, &Interface::RemoveResource, site, type, amount);
	}
	void BuildBy(entt::entity site, float amount) override
	{
		detail::CallOr(buildBy, _fallback, &Interface::BuildBy, site, amount);
	}
	[[nodiscard]] bool IsAvailable(entt::entity site) const override
	{
		return detail::CallOr(isAvailable, _fallback, &Interface::IsAvailable, site);
	}
	[[nodiscard]] MapCoords GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index) const override
	{
		return detail::CallOr(getRandomBuildPos, _fallback, &Interface::GetRandomBuildPos, site, villager, index);
	}
	[[nodiscard]] MapCoords GetNextPosFromIndex(entt::entity site, int32_t& index) const override
	{
		return detail::CallOr(getNextPosFromIndex, _fallback, &Interface::GetNextPosFromIndex, site, index);
	}
	[[nodiscard]] std::optional<MapCoords> GetBuildPos(entt::entity site, int32_t index) const override
	{
		return detail::CallOr(getBuildPos, _fallback, &Interface::GetBuildPos, site, index);
	}
	void AddBuilder(entt::entity site, entt::entity villager) override
	{
		detail::CallOr(addBuilder, _fallback, &Interface::AddBuilder, site, villager);
	}
	void RemoveBuilder(entt::entity site, entt::entity villager) override
	{
		detail::CallOr(removeBuilder, _fallback, &Interface::RemoveBuilder, site, villager);
	}
	[[nodiscard]] bool IsBuilt(entt::entity building) const override
	{
		return detail::CallOr(isBuilt, _fallback, &Interface::IsBuilt, building);
	}
	[[nodiscard]] bool IsRepaired(entt::entity building) const override
	{
		return detail::CallOr(isRepaired, _fallback, &Interface::IsRepaired, building);
	}
	[[nodiscard]] bool IsTouching(entt::entity villager, entt::entity building, float margin) const override
	{
		return detail::CallOr(isTouching, _fallback, &Interface::IsTouching, villager, building, margin);
	}
	[[nodiscard]] float LandAlignmentAt(const glm::vec3& position) const override
	{
		return detail::CallOr(landAlignmentAt, _fallback, &Interface::LandAlignmentAt, position);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerStores final: public ecs::systems::VillagerStoresInterface
{
public:
	using Interface = ecs::systems::VillagerStoresInterface;

	FakeVillagerStores() = default;
	explicit FakeVillagerStores(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<ecs::town_stores::TemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)>
	    temporaryStore;
	std::function<void(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3 angular,
	                   std::optional<glm::vec3> momentum)>
	    makeDroppedLog;

	ecs::town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from,
	                                                   ResourceType type) override
	{
		return detail::CallOr(temporaryStore, _fallback, &Interface::GetTemporaryStore, town, from, type);
	}
	void MakeDroppedLog(glm::vec3 position, uint32_t mesh, float multiplier, glm::vec3 velocity, glm::vec3 angular,
	                    std::optional<glm::vec3> momentum) override
	{
		detail::CallOr(makeDroppedLog, _fallback, &Interface::MakeDroppedLog, position, mesh, multiplier, velocity, angular,
		               momentum);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerTentQueries final: public ecs::systems::VillagerTentQueriesInterface
{
public:
	using Interface = ecs::systems::VillagerTentQueriesInterface;

	FakeVillagerTentQueries() = default;
	explicit FakeVillagerTentQueries(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<entt::entity(glm::ivec2 pos, float radius)> nearestTree;
	std::function<uint32_t(glm::ivec2 pos)> collide;

	[[nodiscard]] entt::entity NearestTree(glm::ivec2 pos, float radius) const override
	{
		return detail::CallOr(nearestTree, _fallback, &Interface::NearestTree, pos, radius);
	}
	[[nodiscard]] uint32_t Collide(glm::ivec2 pos) const override
	{
		return detail::CallOr(collide, _fallback, &Interface::Collide, pos);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerRules final: public ecs::systems::VillagerRulesInterface
{
public:
	using Interface = ecs::systems::VillagerRulesInterface;

	FakeVillagerRules() = default;
	explicit FakeVillagerRules(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<bool()> goHomeEnabled;

	[[nodiscard]] bool GoHomeEnabled() const override
	{
		return detail::CallOr(goHomeEnabled, _fallback, &Interface::GoHomeEnabled);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerWorldQueries final: public ecs::systems::VillagerWorldQueriesInterface
{
public:
	using Interface = ecs::systems::VillagerWorldQueriesInterface;

	FakeVillagerWorldQueries() = default;
	explicit FakeVillagerWorldQueries(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<bool()> isVisualNight;
	std::function<std::optional<bool>(entt::entity town)> graveyard;

	[[nodiscard]] bool IsVisualNight() const override
	{
		return detail::CallOr(isVisualNight, _fallback, &Interface::IsVisualNight);
	}
	[[nodiscard]] std::optional<bool> Graveyard(entt::entity town) const override
	{
		return detail::CallOr(graveyard, _fallback, &Interface::Graveyard, town);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerWorshipCheck final: public ecs::systems::VillagerWorshipCheckInterface
{
public:
	using Interface = ecs::systems::VillagerWorshipCheckInterface;

	FakeVillagerWorshipCheck() = default;
	explicit FakeVillagerWorshipCheck(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<bool(entt::entity villager)> worshipCheck;

	[[nodiscard]] bool WorshipCheck(entt::entity villager) override
	{
		return detail::CallOr(worshipCheck, _fallback, &Interface::WorshipCheck, villager);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerChildFactory final: public ecs::systems::VillagerChildFactoryInterface
{
public:
	using Interface = ecs::systems::VillagerChildFactoryInterface;

	FakeVillagerChildFactory() = default;
	explicit FakeVillagerChildFactory(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<entt::entity(const glm::vec3& position, VillagerInfo info, uint32_t age)> createChild;

	[[nodiscard]] entt::entity CreateChild(const glm::vec3& position, VillagerInfo info, uint32_t age) override
	{
		return detail::CallOr(createChild, _fallback, &Interface::CreateChild, position, info, age);
	}

private:
	std::shared_ptr<Interface> _fallback;
};

class FakeVillagerDiscipleJobs final: public ecs::systems::VillagerDiscipleJobsInterface
{
public:
	using Interface = ecs::systems::VillagerDiscipleJobsInterface;

	FakeVillagerDiscipleJobs() = default;
	explicit FakeVillagerDiscipleJobs(std::shared_ptr<Interface> fallback)
	    : _fallback(std::move(fallback))
	{
	}

	std::function<uint32_t(entt::entity villager, uint8_t disciple)> discipleJob;

	[[nodiscard]] uint32_t DiscipleJob(entt::entity villager, uint8_t disciple) override
	{
		return detail::CallOr(discipleJob, _fallback, &Interface::DiscipleJob, villager, disciple);
	}

private:
	std::shared_ptr<Interface> _fallback;
};
} // namespace openblack::test
