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
#include "3D/LandIslandInterface.h"
#include "Common/PortableRotations.h"
#include "ECS/AbodeFoundation.h"
#include "ECS/AbodeSurroundings.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Creches.h"
#include "ECS/DrinkingWater.h"
#include "ECS/Graveyards.h"
#include "ECS/PlacedTurn.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"
#include "ECS/ShepherdRules.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WonderSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/TownCentreParts.h"
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
namespace abode_foundation = openblack::ecs::abode_foundation;
namespace abode_surroundings = openblack::ecs::abode_surroundings;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	// The pit is made with all its piles, empty: one of food, then five of wood, where its model says
	const auto makePile = [&](size_t place, PotInfo type) {
		const auto& m = extraMetrics.at(place);
		const auto translation = static_cast<glm::vec3>(portable_math::EulerAngleY(-yAngleRadians) * m[3]);
		return PotArchetype::CreateEmpty(position + translation, yAngleRadians, type);
	};
	pit.foodPile = makePile(5, info.potForResourceFood);
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
	// Without its town, the town nearest the place plans it. The search goes over the game's list of all the towns.
	if (!registry.Context().towns.contains(townId))
	{
		const auto town = ecs::player_towns::NearestOfAll(registry, {position.x, position.z});
		if (town == entt::null)
		{
			return entt::null;
		}
		townId = registry.Get<Town>(town).id;
	}
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(portable_math::EulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<PlannedAbode>(entity, PlannedAbode {.townId = townId, .info = type, .yAngle = yAngleRadians});
	// It joins its town's plans, after those planned before it
	registry.Get<Town>(registry.Context().towns.at(townId)).planned.push_back(entity);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Town {} plans building {} at {}", townId, static_cast<int>(type),
	                    glm::to_string(position));
	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount, float built)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Without its town, the building goes to the town nearest the place. The search goes over the game's list of all
	// the towns.
	if (registry.Context().towns.find(townId) == registry.Context().towns.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		const auto town = ecs::player_towns::NearestOfAll(registry, {position.x, position.z});
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
	// It takes its place in the order of the things made, before the things it makes for itself
	TakeThingMadePlace();

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
	    registry.Assign<Transform>(entity, position, glm::mat3(portable_math::EulerAngleY(-yAngleRadians)), glm::vec3(scale));
	// A workshop gets its food and wood through its store, after it has its wood pile
	const bool workshop = info.abodeType == AbodeType::Workshop;
	registry.Assign<Abode>(entity, info.abodeNumber, townId, workshop ? 0u : foodAmount, workshop ? 0u : woodAmount).info =
	    type;
	if (workshop)
	{
		registry.Assign<Workshop>(entity);
	}
	// Under construction until all of it is built: it doesn't work until then
	if (built < 1.0f)
	{
		registry.Assign<BuildProgress>(entity, built < 0.0f ? 0.0f : built);
	}
	// It joins its town's buildings, the newest first
	if (const auto town = registry.Context().towns.find(townId); town != registry.Context().towns.end())
	{
		auto& abodes = registry.Get<Town>(town->second).abodes;
		abodes.insert(abodes.begin(), entity);
	}
	auto resourceId = resources::HashIdentifier(info.meshId);
	const auto& mesh = registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// The town makes its ground again with the new building, before the building puts up anything round itself
	if (const auto town = registry.Context().towns.find(townId);
	    town != registry.Context().towns.end() && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().StructuresChanged(town->second);
	}
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

	// A building that does not bend with the land settles into a slope: it is lowered so that the low side of its
	// foundations reaches the land. The chimney's smoke above was placed before, where the building first stood.
	if (!morphsWithTerrain && Locator::terrainSystem::has_value() && Locator::terrainSystem::value().IsLoaded())
	{
		if (const auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(resourceId))
		{
			const auto& bounds = meshes.Handle(resourceId)->GetBoundingBox();
			const auto box = abode_foundation::GroundBoxOf(bounds.minima, bounds.maxima);
			const auto& land = Locator::terrainSystem::value();
			auto& placed = registry.Get<Transform>(entity);
			placed.position.y +=
			    abode_foundation::FoundationSink(box, placed.rotation, placed.scale.x, {placed.position.x, placed.position.z},
			                                     [&land](glm::vec2 point) { return land.GetHeightAt(point); });
		}
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	// Every building of a town looks for water nearby when it is built, for its people's flocks to drink at
	if (info.abodeType != AbodeType::Citadel)
	{
		ecs::drinking_water::FindFor(entity, ecs::shepherd_rules::k_HomeWaterReach);
	}

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		break;
	case AbodeType::Workshop:
		// A workshop makes its empty wood pile, then takes its food and its wood as a store does: it keeps only the wood
		if (Locator::workshopSystem::has_value())
		{
			Locator::workshopSystem::value().WoodPileOf(entity);
		}
		if (Locator::resourceStoreSystem::has_value())
		{
			auto& stores = Locator::resourceStoreSystem::value();
			stores.AddResource(entity, ResourceType::Food, foodAmount, std::nullopt);
			stores.AddResource(entity, ResourceType::Wood, woodAmount, std::nullopt);
		}
		else
		{
			registry.Get<Abode>(entity).woodAmount = woodAmount;
		}
		break;
	case AbodeType::TownCentre:
		// A town centre standing built carries its town's totem, and its stone hands show the miracles its town offers.
		// A town centre not yet built gets them when its town finishes it.
		if (ecs::town_centre_parts::PartsWhenMade(built))
		{
			VillageTotemArchetype::Create(entity, info);
			if (Locator::spellIconSystem::has_value())
			{
				Locator::spellIconSystem::value().CentreWorking(entity);
			}
		}
		break;
	case AbodeType::Graveyard:
		// A graveyard standing built works at once, its first grave shown
		graveyards::Made(entity);
		break;
	case AbodeType::Creche:
		// A crèche standing built works at once, its town sending its children there
		creches::Made(entity);
		break;
	case AbodeType::Wonder:
		// A wonder gets the power of its size. A wonder that stands finished adds its power to its player at once.
		if (Locator::wonderSystem::has_value())
		{
			Locator::wonderSystem::value().Made(entity, scale);
		}
		break;
	default:
		break;
	}

	// A building made standing in play gets its footpath from its town's storage pit
	if (built >= 1.0f && Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().BuildingWorking(entity);
	}
	// A building made standing starts to work at once, and may make its town a new town
	if (built >= 1.0f && Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().BuildingWorks(entity);
	}

	// A building puts up the things it has round itself, built or not
	abode_surroundings::Create(entity);

	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));
	return entity;
}
