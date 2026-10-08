/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeArchetype.h"

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Abodes.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Wonders.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"
#include "Worship/TownCentreSpellIcon.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = Locator::resources::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& m = extraMetrics.at(i);
		// (inferred) the metric's point turned by the object's rotation and not scaled; worship::SpecialPoint scales it,
		// the store's own call is not read
		auto translation = affine::AngleY(yAngleRadians) * glm::vec3(m[3]);
		pit.woodPiles.at(i) = PotArchetype::Create(position + translation, yAngleRadians, type, 0, true);
		++i;
	}
	assert(i == pit.woodPiles.size());
	const auto& m = extraMetrics.at(5);
	auto translation = affine::AngleY(yAngleRadians) * glm::vec3(m[3]); // (inferred) as the wood piles above
	pit.foodPile = PotArchetype::Create(position + translation, yAngleRadians, info.potForResourceFood, 0, true);
	// The store's totals are spread over its piles as StoragePitStore::AddResource does.
	ecs::StoragePitStore::AddResource(entity, ResourceType::Wood, woodAmount);
	ecs::StoragePitStore::AddResource(entity, ResourceType::Food, foodAmount);
}

namespace
{
/// The town centre's totem (a built town centre of a town, when made functional): the tribe's plinth (the totem
/// statue info by the tribe) at the town centre's special point 6 (through its full matrix; raised like the morphing
/// town centre, by the land under the point minus the land under the origin), with its Y angle and scale, and the
/// icon on the plinth's top. Both static, no foundation sink (not an Abode). Facing the worship site and the
/// creature's icon wait for those systems: the icon is the hand (BuildingSpellHand) of a player without a creature.
void CreateTotemStatue(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto tribe = static_cast<size_t>(info.tribeType);
	const auto& statues = Locator::infoConstants::value().totemStatue;
	if (tribe >= statues.size() || !Locator::terrainSystem::has_value() ||
	    Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return;
	}
	const auto& transform = registry.Get<Transform>(townCentre);
	glm::vec3 point = transform.position;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto centreMesh = resources::HashIdentifier(info.meshId);
	if (meshes.Contains(centreMesh))
	{
		if (const auto& extra = meshes.Handle(centreMesh)->GetExtraMetrics(); extra.size() > 6)
		{
			point = transform.position + transform.rotation * (glm::vec3(extra[6][3]) * transform.scale);
			// the morphed town centre's extra point: (H(p) - H(pos)) + p.y
			const auto ground = land_morph::Altitude(Locator::terrainSystem::value());
			point.y = land_morph::Raised(ground, point, ground(glm::vec2(transform.position.x, transform.position.z)));
		}
	}
	const auto rotation = affine::AngleY(yAngleRadians);

	const auto plinth = registry.Create();
	ecs::object_index::Assign(plinth); // the totem is one object (the hand on top is part of it)
	registry.Assign<Transform>(plinth, point, rotation, glm::vec3(scale));
	registry.Assign<Mesh>(plinth, resources::HashIdentifier(statues.at(tribe).plinth), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	const auto top = registry.Create();
	registry.Assign<Transform>(top, point + glm::vec3(0.0f, TotemStatue::k_PlinthTop, 0.0f), rotation, glm::vec3(scale));
	registry.Assign<Mesh>(top, resources::HashIdentifier(MeshId::BuildingSpellHand), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	registry.Assign<TotemStatue>(plinth, townCentre, top, point.y, 0.0f);
}
} // namespace

void AbodeArchetype::MakeTownCentreFunctional(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	// only when the centre has no totem yet (a repaired centre is made functional again)
	bool hasTotem = false;
	registry.Each<const TotemStatue>([&](const TotemStatue& totem) { hasTotem = hasTotem || totem.townCentre == townCentre; });
	if (!hasTotem)
	{
		CreateTotemStatue(townCentre, info, yAngleRadians, scale);
	}
	// the town's centre = this when still null (as CREATE_TOWN_CENTRE)
	const auto townId = registry.Get<Abode>(townCentre).townId;
	if (const auto town = ecs::town_queries::TownByKey(townId); town != entt::null)
	{
		if (auto& component = registry.Get<Town>(town); component.centre == entt::null)
		{
			component.centre = townCentre;
		}
	}
	// then one spell icon per spell seed the town already has (at most 6). Their creation indexes are taken once, when
	// the centre is first made functional (its totem made now): a repaired centre or CHL 22 >= 1 again makes no new icon
	// (AddSpell), so the indexes do not move
	if (!hasTotem)
	{
		ecs::object_index::OnTownCentre(townId);
	}
	// the spell part (Worship/TownCentreSpellIcon.cpp): those icons and the town's worship site. The icons take no
	// creation index of their own (object_index counts them above).
	worship::town_centre::MakeFunctional(townCentre);
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount, bool underConstruction)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (openblack) the Abode class's tap handlers, once (InterfaceValidToTap / InterfaceTap)
	abodes::RegisterTapHandler();
	abodes::ConnectDrawMeshListener();

	// If there is no town, assign to closest
	if (ecs::town_queries::TownByKey(townId) == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		// the land script's CREATE_ABODE / CREATE_TOWN_CENTRE without their town: the global town list, the first
		// always taken, then GetDistanceInMetres < best. MapCoords x, z only (FromMetres: the altitude is not read)
		const auto town = ecs::map_cells::FindNearestTownInList(map_coords::FromMetres(glm::vec2(position.x, position.z)));
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
	ecs::object_index::Assign(entity);

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

	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	auto& abode = registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount);
	abode.info = type;
	// the abode joins its town (at the head of its structures, then the town's area)
	town_placement::SetTownArea(ecs::town_queries::TownByKey(townId), entity);
	if (underConstruction)
	{
		// a plan: built 0% and not yet complete (the percent argument is not used); the life stays the new object's
		// 1.0. TownStats counts it from its MakeFunctional
		abode.buildFlags = Abode::k_UnderConstruction;
		abode.percentBuilt = 0.0f;
		abode.addedToTownStats = false;
	}
	auto resourceId = resources::HashIdentifier(info.meshId);
	const auto& mesh = registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (morphsWithTerrain)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}
	// (only with an island loaded: the placeholder UnloadedIsland throws, and has no materials)
	else if (Locator::terrainSystem::has_value() && !Locator::terrainSystem::value().GetMaterialInfo().empty() &&
	         Locator::resources::value().GetMeshes().Contains(resourceId))
	{
		// an abode that doesn't follow the land sinks to the lowest ground under the corners of its mesh box (never
		// above the origin's), but by at most max(0.2 x its 2D radius, 0.8) (Get2DRadius: scale x the larger half-extent
		// in x or z). It replaces the script's altitude.
		const auto& island = Locator::terrainSystem::value();
		const auto box = Locator::resources::value().GetMeshes().Handle(resourceId)->GetBoundingBox();
		const glm::vec2 origin(position.x, position.z);
		const float ground = island.GetHeightAt(origin);
		float lowest = 0.0f;
		for (const auto& corner : {glm::vec3(box.minima.x, 0.0f, box.minima.z), glm::vec3(box.maxima.x, 0.0f, box.minima.z),
		                           glm::vec3(box.minima.x, 0.0f, box.maxima.z), glm::vec3(box.maxima.x, 0.0f, box.maxima.z)})
		{
			const glm::vec3 world = position + transform.rotation * (corner * transform.scale);
			lowest = std::min(lowest, island.GetHeightAt(glm::vec2(world.x, world.z)) - ground);
		}
		const float radius = ecs::object::Get2DRadius(entity);
		registry.Get<Transform>(entity).position.y = ground + std::max(lowest, -std::max(0.2f * radius, 0.8f));
	}

	// the smoke of a mesh with a chimney, at its final place
	if (const auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(resourceId))
	{
		ecs::chimney_smoke::Attach(entity, *meshes.Handle(resourceId), registry.Get<Transform>(entity),
		                           info.abodeType == AbodeType::Workshop);
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		// a whole one (the script's) is made functional: the town's storage pit = this (the last one wins; the
		// temporary pots). A plan's at Built
		if (const auto town = ecs::town_queries::TownByKey(townId); !underConstruction && town != entt::null)
		{
			ecs::town_stores::SetStoragePit(town, entity);
		}
		break;
	case AbodeType::Creche:
		// a whole one (the script's) is made functional: the town's creche = this when it is still null (the first one
		// wins); a plan's at Built
		if (const auto town = ecs::town_queries::TownByKey(townId); !underConstruction && town != entt::null)
		{
			auto& component = registry.Get<Town>(town);
			if (component.creche == entt::null)
			{
				component.creche = entity;
			}
		}
		break;
	case AbodeType::TownCentre:
		// a whole one is made functional; a plan's at Built (abodes::MakeFunctional)
		if (!underConstruction)
		{
			MakeTownCentreFunctional(entity, info, yAngleRadians, scale);
		}
		break;
	case AbodeType::Graveyard:
		// the Graveyard component (its dead); a whole one's MakeFunctional (one dead); a plan's at Built
		registry.Assign<Graveyard>(entity);
		if (!underConstruction)
		{
			ecs::graveyard::MakeFunctional(entity);
		}
		break;
	case AbodeType::Workshop:
		// the Workshop part, its ShowNeedsVisuals' creation index ((not ported) the object) then its wood pile, in that
		// order. A whole one's MakeFunctional (the town's list); a plan's at Built
		ecs::workshops::Create(entity);
		if (!underConstruction)
		{
			ecs::workshops::MakeFunctional(entity);
		}
		break;
	case AbodeType::Wonder:
		// SetPower(scale) and, built, the wonder added to its player
		ecs::wonders::Create(entity, scale);
		break;
	default:
		break;
	}
	// InsertMapObject: the head of every cell of its collide descriptor. (inferred) After the store's piles, the totem
	// and the spell icons made above: their order against it is not read
	ecs::map_cells::InsertMapObject(entity);
	// the workshop's insert above runs before its wood pile's: the pile goes into the cells after the workshop (its
	// creation index stays before the highlight's)
	if (info.abodeType == AbodeType::Workshop)
	{
		ecs::workshops::OnInsertedInMap(entity);
	}
	// last: the did-you-know sign (its creation index after everything above) and the street lantern
	ecs::abodes::CreateAbodeSurroundingObjects(entity);
	return entity;
}
