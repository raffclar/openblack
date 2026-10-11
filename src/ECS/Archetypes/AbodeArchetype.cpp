/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeArchetype.h"

#include <array>
#include <tuple>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/string_cast.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"
#include "VillageTotemArchetype.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	// The pit is made with all its piles, empty: five of wood and one of food where its model says
	const auto makePile = [&](size_t place, PotInfo type) {
		const auto& m = extraMetrics.at(place);
		const auto translation = static_cast<glm::vec3>(glm::eulerAngleY(-yAngleRadians) * m[3]);
		return PotArchetype::CreateEmpty(position + translation, yAngleRadians, type);
	};
	std::array<magic::piles::PileRoom, std::tuple_size_v<decltype(StoragePit::woodPiles)>> woodRoom {};
	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& potInfo = potInfoConstants.at(static_cast<size_t>(type));
		pit.woodPiles.at(i) = makePile(i, type);
		woodRoom.at(i) = {.maximum = potInfo.maxAmountInPot, .capped = magic::piles::IsCapped(potInfo.nextPotForResource)};
		++i;
	}
	assert(i == pit.woodPiles.size());
	pit.foodPile = makePile(5, info.potForResourceFood);

	// Then it is given its food and wood, which fill its piles in turn: each pile that leads on to another takes no
	// more than it is drawn full at, the last takes the rest
	const auto& foodInfo = potInfoConstants.at(static_cast<size_t>(info.potForResourceFood));
	const std::array foodRoom {magic::piles::PileRoom {.maximum = foodInfo.maxAmountInPot,
	                                                   .capped = magic::piles::IsCapped(foodInfo.nextPotForResource)}};
	registry.Get<Pot>(pit.foodPile).amount = magic::piles::ShareAmongPiles(foodAmount, foodRoom).front();
	const auto woodShares = magic::piles::ShareAmongPiles(woodAmount, woodRoom);
	for (size_t place = 0; place < pit.woodPiles.size(); ++place)
	{
		registry.Get<Pot>(pit.woodPiles.at(place)).amount = woodShares.at(place);
	}
}

entt::entity AbodeArchetype::CreatePlan(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                        float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Without its town, the town nearest the place plans it
	if (!registry.Context().towns.contains(townId))
	{
		const auto town = Locator::townSystem::value().FindClosestTown(position);
		if (town == entt::null)
		{
			return entt::null;
		}
		townId = registry.Get<Town>(town).id;
	}
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<PlannedAbode>(entity, townId, type);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Town {} plans building {} at {}", townId, static_cast<int>(type),
	                    glm::to_string(position));
	return entity;
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();

	// If there is no town, assign to closest
	if (registry.Context().towns.find(townId) == registry.Context().towns.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		const auto town = Locator::townSystem::value().FindClosestTown(position);
		if (town != entt::null)
		{
			townId = registry.Get<Town>(town).id;
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Function {} has could not find closest town.", __func__, townId);
			return entt::null;
		}
	}

	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(type));
	bool morphsWithTerrain = false;
	morphsWithTerrain |= info.abodeType == AbodeType::Graveyard;
	morphsWithTerrain |= info.abodeType == AbodeType::StoragePit;
	if (info.abodeType == AbodeType::Wonder)
	{
		morphsWithTerrain |= info.tribeType == Tribe::CELTIC;
		morphsWithTerrain |= info.tribeType == Tribe::JAPANESE;
		morphsWithTerrain |= info.tribeType == Tribe::INDIAN;
		morphsWithTerrain |= info.tribeType == Tribe::NORSE;
		morphsWithTerrain |= info.tribeType == Tribe::TIBETAN;
	}
	morphsWithTerrain |= info.abodeType == AbodeType::Workshop;
	morphsWithTerrain |= info.abodeType == AbodeType::Citadel;
	morphsWithTerrain |= info.abodeType == AbodeType::Creche;
	morphsWithTerrain |= info.abodeType == AbodeType::FootballPitch;
	morphsWithTerrain |= info.abodeType == AbodeType::TownCentre;
	morphsWithTerrain |= info.abodeType == AbodeType::Field;

	const auto& transform =
	    registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount).info = type;
	// It joins its town's buildings, the newest first
	if (const auto town = registry.Context().towns.find(townId); town != registry.Context().towns.end())
	{
		auto& abodes = registry.Get<Town>(town->second).abodes;
		abodes.insert(abodes.begin(), entity);
	}
	auto resourceId = resources::HashIdentifier(info.meshId);
	const auto& mesh = registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (morphsWithTerrain)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}

	// A home with a chimney smokes while someone is in
	if (const auto& meshes = Locator::resources::value().GetMeshes();
	    Locator::chimneySmokeSystem::has_value() && meshes.Contains(resourceId))
	{
		Locator::chimneySmokeSystem::value().Attach(entity, *meshes.Handle(resourceId), transform,
		                                            info.abodeType == AbodeType::Workshop);
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		break;
	case AbodeType::TownCentre:
		// A town centre standing built carries its town's totem
		VillageTotemArchetype::Create(entity, info);
		break;
	default:
		break;
	}

	return entity;
}
