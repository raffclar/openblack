/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WorkshopSystem.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87Angle.h"
#include "ECS/Archetypes/ScaffoldArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Workshop.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/Map.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/TownTurnOrder.h"
#include "ECS/WorkshopRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace workshop = openblack::ecs::workshop;

namespace
{

/// A town that is near enough to a scaffold builds with it, in metres
constexpr float k_TownReach = 50.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// A place on the map as the game keeps it: on the ground, at a position of the map's fixed grid
glm::vec3 OnMap(const map_coords::MapCoords& coords)
{
	const glm::vec2 metres {map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z)};
	return {metres.x, GroundAt(metres), metres.y};
}

map_coords::MapCoords CoordsOf(glm::vec3 position)
{
	return map_coords::FromMetres(glm::vec2(position.x, position.z));
}

std::optional<workshop::SpecialPlace> SpecialPlaceOf(entt::entity building, size_t point)
{
	const auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(building);
	const auto* mesh = registry.TryGet<const Mesh>(building);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	const auto& points = meshes.Handle(mesh->id)->GetExtraMetrics();
	if (point >= points.size())
	{
		return std::nullopt;
	}
	return workshop::SpecialPlaceOf(points.at(point), workshop::BuildingMatrix(ecs::placed_turn::AnglesOf(registry, building),
	                                                                           ecs::placed_turn::CanLean(registry, building),
	                                                                           transform->scale.x, transform->position));
}

/// The town's workshops in the town's order, the newest first
std::vector<entt::entity> WorkshopsOf(entt::entity town)
{
	const auto& registry = Entities();
	std::vector<entt::entity> workshops;
	if (const auto* data = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr)
	{
		for (const auto abode : data->abodes)
		{
			if (registry.Valid(abode) && registry.AllOf<Workshop>(abode))
			{
				workshops.push_back(abode);
			}
		}
	}
	std::ranges::reverse(workshops);
	return workshops;
}

void MarkDesiresChanged(entt::entity town)
{
	if (town != entt::null && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().MarkDesiresChanged(town);
	}
}

} // namespace

WorkshopSystem::WorkshopSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	_connections.emplace_back(registry.OnDestroy<Scaffold>().connect<&WorkshopSystem::OnScaffoldGone>(*this));
	_connections.emplace_back(registry.OnDestroy<Workshop>().connect<&WorkshopSystem::OnWorkshopGone>(*this));
}

void WorkshopSystem::ProcessWorkshop(entt::entity workshopEntity)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Workshop>(workshopEntity);
	if (data == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const uint32_t wood = Locator::resourceStoreSystem::has_value()
	                          ? Locator::resourceStoreSystem::value().GetResource(workshopEntity, ResourceType::Wood)
	                          : 0;
	const workshop::TurnFacts facts {
	    .functional = ecs::villager_home::IsFunctional(workshopEntity),
	    .room = disciple_work::WorkshopRoom(data->makingTurns != 0, data->counted),
	    .wood = wood,
	    .scaffoldWood = info.scaffold.woodValue,
	};
	switch (workshop::NextEvent(data->makingTurns, facts))
	{
	case workshop::TurnEvent::Starts:
		StartScaffold(workshopEntity);
		break;
	case workshop::TurnEvent::Finishes:
		FinishScaffold(workshopEntity);
		break;
	case workshop::TurnEvent::None:
		break;
	}
	// The workshop looks only at the newest scaffold of its list: a scaffold names no next scaffold, thus the search
	// stops after the first one
	if (auto* after = registry.TryGet<Workshop>(workshopEntity); after != nullptr && !after->scaffolds.empty())
	{
		const auto newest = after->scaffolds.front();
		const auto* scaffold = registry.Valid(newest) ? registry.TryGet<const Scaffold>(newest) : nullptr;
		if (scaffold != nullptr &&
		    !workshop::KeepsScaffold(scaffold->putDownTurn, Turn(), info.scaffold.gameTurnsAfterPlacingCanStillPickUp,
		                             scaffold->scriptBuilding.has_value()))
		{
			ReleaseScaffold(newest);
		}
	}
	// TODO(scaffolds): the flag beside the workshop shows the wood that it still needs for a scaffold
}

void WorkshopSystem::StartScaffold(entt::entity workshopEntity)
{
	auto& registry = Entities();
	const auto& info = Locator::infoConstants::value();
	auto& stores = Locator::resourceStoreSystem::value();
	// The workshop asks again for the wood of a scaffold before it uses it
	if (stores.GetResource(workshopEntity, ResourceType::Wood) < info.scaffold.woodValue)
	{
		return;
	}
	stores.RemoveResource(workshopEntity, ResourceType::Wood, info.scaffold.woodValue);
	const auto* abodeInfo = ecs::world_objects::AbodeInfoOf(workshopEntity);
	registry.Get<Workshop>(workshopEntity).makingTurns =
	    abodeInfo != nullptr ? workshop::MakingTurns(abodeInfo->timeEachMobileObjectTakesToProduce) : 0;
	// The sound of the work loops at the workshop until the scaffold is finished
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().AttachObjectSound(workshopEntity,
		                                                   static_cast<entt::id_type>(audio::SoundId::G_Workshop), true);
	}
}

void WorkshopSystem::FinishScaffold(entt::entity workshopEntity)
{
	auto& registry = Entities();
	auto& data = registry.Get<Workshop>(workshopEntity);
	const auto place = workshop::FirstFreePlace(data.places);
	const auto special = SpecialPlaceOf(workshopEntity, workshop::k_PlacePoints.at(place));
	const auto coords =
	    special.has_value() ? special->coords : CoordsOf(registry.Get<const Transform>(workshopEntity).position);
	const float yAngle = special.has_value() ? special->angles.y : 0.0f;
	const auto town = ecs::town_homes::TownOfAbode(registry, workshopEntity);
	// A new scaffold is worth one scaffold
	const auto scaffold = CreateScaffold(OnMap(coords), town, workshopEntity, yAngle, workshop::ScaleOfValue(1));
	if (scaffold != entt::null)
	{
		auto& after = registry.Get<Workshop>(workshopEntity);
		workshop::AddScaffold(after, scaffold);
		registry.Get<Scaffold>(scaffold).place = place;
		after.places.at(place) = Workshop::Place::Holds;
		// A horn sounds at the workshop
		audio::StartGameSoundEffect(
		    static_cast<entt::id_type>(audio::SoundId::G_ScaffoldReady_01),
		    {.position = registry.Get<const Transform>(workshopEntity).position, .owner = workshopEntity});
	}
	registry.Get<Workshop>(workshopEntity).makingTurns = 0;
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().DetachObjectSound(workshopEntity,
		                                                   static_cast<entt::id_type>(audio::SoundId::G_Workshop));
	}
	registry.SetDirty();
}

int32_t WorkshopSystem::RoomOf(entt::entity workshopEntity) const
{
	const auto& registry = Entities();
	const auto* data = registry.Valid(workshopEntity) ? registry.TryGet<const Workshop>(workshopEntity) : nullptr;
	return data != nullptr ? disciple_work::WorkshopRoom(data->makingTurns != 0, data->counted) : 0;
}

entt::entity WorkshopSystem::CreateScaffold(glm::vec3 place, entt::entity town, entt::entity workshopEntity, float yAngle,
                                            float scale)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	return ecs::archetypes::ScaffoldArchetype::Create(place, town, workshopEntity, yAngle, scale);
}

bool WorkshopSystem::CanTake([[maybe_unused]] entt::entity scaffold) const
{
	// TODO(scaffolds): a scaffold on a building site can be taken only before the building starts, and only for a time
	// after it was put down; openblack puts no scaffold on a site yet
	return true;
}

void WorkshopSystem::TakenByHand(entt::entity scaffold, PlayerNames player)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Scaffold>(scaffold);
	if (data == nullptr)
	{
		return;
	}
	data->handPlayer = player;
	data->putDownTurn = 0;
	// Its workshop keeps the place of the scaffold for it
	if (auto* owner = registry.Valid(data->workshop) ? registry.TryGet<Workshop>(data->workshop) : nullptr)
	{
		owner->places.at(data->place % owner->places.size()) = Workshop::Place::Away;
	}
}

void WorkshopSystem::LetGoByHand(entt::entity scaffold, bool forced)
{
	// The scaffold comes down gently when the hand puts it on the land and does not drop it by force
	if (auto* data = Entities().TryGet<Scaffold>(scaffold))
	{
		data->gentle = !forced;
	}
}

void WorkshopSystem::NotPutDownByHand(entt::entity scaffold)
{
	if (auto* data = Entities().TryGet<Scaffold>(scaffold))
	{
		data->gentle = false;
	}
}

void WorkshopSystem::CameToRest(entt::entity scaffold, bool fromHand, bool inMap)
{
	auto& registry = Entities();
	if (!registry.Valid(scaffold) || !registry.AllOf<Scaffold, Transform>(scaffold))
	{
		return;
	}
	if (inMap)
	{
		// A scaffold near a place of a workshop of its town goes into that place
		bool snapped = false;
		const auto town = TownForBuilding(registry.Get<const Transform>(scaffold).position);
		for (const auto workshopEntity : WorkshopsOf(town))
		{
			snapped = SnapToPlace(workshopEntity, scaffold) || snapped;
		}
		if (fromHand && !snapped)
		{
			// TODO(scaffolds): a scaffold put down gently becomes the site of the building that it shows; openblack has no
			// building for a scaffold yet, thus every scaffold lies where it is put down, as one with no building does
			if (Locator::particleSystem::has_value())
			{
				Locator::particleSystem::value().StartSpotVisual(SpotVisualType::SpellFailCast,
				                                                 registry.Get<const Transform>(scaffold).position, std::nullopt,
				                                                 entt::null, 1.0f);
			}
		}
		auto& data = registry.Get<Scaffold>(scaffold);
		data.handPlayer.reset();
		data.putDownTurn = Turn();
	}
	registry.Get<Scaffold>(scaffold).gentle = false;
}

bool WorkshopSystem::CanMove(entt::entity scaffold) const
{
	// TODO(scaffolds): a scaffold on a building site does not move either
	return !workshop::StandsInYard(Entities(), scaffold);
}

entt::entity WorkshopSystem::TownForBuilding(glm::vec3 place) const
{
	const auto& registry = Entities();
	const auto from = CoordsOf(place);
	// The town of the nearest building in the cells about the place
	if (Locator::entitiesMap::has_value())
	{
		const auto& map = Locator::entitiesMap::value();
		const auto nearest = ecs::script_find::FindNearest(from, k_TownReach, [&](glm::ivec2 cell) {
			std::vector<ecs::script_find::Candidate> found;
			for (const auto thing : map.GetFixedInGridCell(ecs::MapInterface::CellId(cell)))
			{
				if (registry.Valid(thing) && registry.AllOf<Abode>(thing) && registry.AllOf<Transform>(thing))
				{
					found.push_back({.entity = thing, .at = CoordsOf(registry.Get<const Transform>(thing).position)});
				}
			}
			return found;
		});
		if (nearest != entt::null)
		{
			return ecs::town_homes::TownOfAbode(registry, nearest);
		}
	}
	// Else the nearest town, or the town of the nearest planned building, in the order of the players
	entt::entity found = entt::null;
	float best = k_TownReach;
	for (uint8_t index = 0; index < static_cast<uint8_t>(PlayerNames::_COUNT); ++index)
	{
		for (const auto town : ecs::town_turns::TownsInGainedOrder(registry, static_cast<PlayerNames>(index)))
		{
			const auto* transform = registry.TryGet<const Transform>(town);
			if (transform == nullptr)
			{
				continue;
			}
			if (const float distance = gutils::GetDistanceInMetres(from, CoordsOf(transform->position)); distance < best)
			{
				best = distance;
				found = town;
			}
			for (const auto plan : registry.Get<const Town>(town).planned)
			{
				const auto* planned = registry.Valid(plan) ? registry.TryGet<const Transform>(plan) : nullptr;
				if (planned == nullptr)
				{
					continue;
				}
				if (const float distance = gutils::GetDistanceInMetres(from, CoordsOf(planned->position)); distance < best)
				{
					best = distance;
					found = town;
				}
			}
		}
	}
	return found;
}

void WorkshopSystem::ReleaseScaffold(entt::entity scaffold)
{
	auto& registry = Entities();
	auto* data = registry.TryGet<Scaffold>(scaffold);
	if (data == nullptr)
	{
		return;
	}
	const auto owner = std::exchange(data->workshop, entt::null);
	auto* workshopData = registry.Valid(owner) ? registry.TryGet<Workshop>(owner) : nullptr;
	if (workshopData == nullptr)
	{
		return;
	}
	// When the yard stops being full, the town wants again what it needs
	if (workshop::RemoveScaffold(*workshopData, scaffold, data->place))
	{
		MarkDesiresChanged(ecs::town_homes::TownOfAbode(registry, owner));
	}
}

bool WorkshopSystem::SnapToPlace(entt::entity workshopEntity, entt::entity scaffold)
{
	auto& registry = Entities();
	const auto at = CoordsOf(registry.Get<const Transform>(scaffold).position);
	bool near = false;
	for (const auto point : workshop::k_PlacePoints)
	{
		if (const auto special = SpecialPlaceOf(workshopEntity, point);
		    special.has_value() && gutils::GetDistanceInMetres(at, special->coords) < workshop::k_SnapReach)
		{
			near = true;
		}
	}
	if (!near)
	{
		return false;
	}
	if (registry.Get<const Scaffold>(scaffold).workshop != workshopEntity)
	{
		if (RoomOf(workshopEntity) == 0)
		{
			return false;
		}
		ReleaseScaffold(scaffold);
		auto& owner = registry.Get<Workshop>(workshopEntity);
		workshop::AddScaffold(owner, scaffold);
		auto& data = registry.Get<Scaffold>(scaffold);
		data.workshop = workshopEntity;
		data.place = workshop::FirstFreePlace(owner.places);
	}
	MoveToPlace(workshopEntity, scaffold);
	return true;
}

void WorkshopSystem::MoveToPlace(entt::entity workshopEntity, entt::entity scaffold)
{
	auto& registry = Entities();
	const auto place = registry.Get<const Scaffold>(scaffold).place;
	if (const auto special = SpecialPlaceOf(workshopEntity, workshop::k_PlacePoints.at(place % workshop::k_PlacePoints.size())))
	{
		auto& transform = registry.Get<Transform>(scaffold);
		transform.position = OnMap(special->coords);
		transform.rotation = x87::GameTurnMatrix(special->angles.x, special->angles.y, special->angles.z);
		ecs::placed_turn::KeepAngles(registry, scaffold, special->angles);
		if (Locator::entitiesMap::has_value())
		{
			Locator::entitiesMap::value().Refile(scaffold);
		}
	}
	auto& owner = registry.Get<Workshop>(workshopEntity);
	owner.places.at(place % owner.places.size()) = Workshop::Place::Holds;
	registry.SetDirty();
}

void WorkshopSystem::OnScaffoldGone(entt::registry& /*registry*/, entt::entity scaffold)
{
	// A scaffold that goes from the world stops to count for its workshop
	ReleaseScaffold(scaffold);
}

void WorkshopSystem::OnWorkshopGone(entt::registry& /*registry*/, entt::entity workshopEntity)
{
	// A workshop that goes lets its scaffolds go: they stay where they are
	auto& registry = Entities();
	const auto& data = registry.Get<const Workshop>(workshopEntity);
	for (const auto scaffold : data.scaffolds)
	{
		if (auto* owned = registry.Valid(scaffold) ? registry.TryGet<Scaffold>(scaffold) : nullptr;
		    owned != nullptr && owned->workshop == workshopEntity)
		{
			owned->workshop = entt::null;
		}
	}
}
