/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownSystem.h"

#include <algorithm>
#include <optional>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

TownSystem::TownSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_registry = &Locator::entitiesRegistry::value();
	_connections.push_back(_registry->OnDestroy<Abode>().connect<&TownSystem::OnAbodeGone>(*this));
	_connections.push_back(_registry->OnDestroy<Villager>().connect<&TownSystem::OnVillagerGone>(*this));
}

TownSystem::~TownSystem()
{
	// The registry may have gone first, taking its signals with it
	if (Locator::entitiesRegistry::has_value() && &Locator::entitiesRegistry::value() == _registry)
	{
		for (auto& connection : _connections)
		{
			connection.release();
		}
	}
}

void TownSystem::OnAbodeGone(entt::registry& registry, entt::entity abode)
{
	for (auto [entity, town] : registry.view<Town>().each())
	{
		std::erase(town.abodes, abode);
	}
}

void TownSystem::OnVillagerGone(entt::registry& registry, entt::entity villager)
{
	for (auto [entity, town] : registry.view<Town>().each())
	{
		std::erase(town.homelessVillagers, villager);
	}
}

namespace
{
/// The room of the game's buildings, from their rows in the buildings' table, and their people coming out
town_homes::Homes GameHomes()
{
	return {.room = [](entt::entity abode) -> std::optional<town_homes::AbodeRoom> {
		        const auto& registry = Locator::entitiesRegistry::value();
		        const auto* building = registry.TryGet<const Abode>(abode);
		        if (building == nullptr || !Locator::infoConstants::has_value())
		        {
			        return std::nullopt;
		        }
		        const auto& rows = Locator::infoConstants::value().abode;
		        const auto row = static_cast<size_t>(building->type);
		        if (row >= rows.size())
		        {
			        return std::nullopt;
		        }
		        return town_homes::AbodeRoom {.maxAdults = rows.at(row).maxVillagersInAbode,
		                                      .maxChildren = rows.at(row).maxChildrenInAbode,
		                                      .functional = villager_home::IsFunctional(abode)};
	        },
	        .leaving = [](entt::entity villager) { villager_home::LeavingHome(villager); }};
}
} // namespace

entt::entity TownSystem::FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const
{
	return town_homes::FindAbodeWithSpace(Locator::entitiesRegistry::value(), GameHomes(), town, villager, leastScore);
}

entt::entity TownSystem::FindClosestTown(const glm::vec3& point) const
{
	const auto& registry = Locator::entitiesRegistry::value();

	entt::entity result = entt::null;
	auto closest = std::numeric_limits<float>::infinity();

	registry.Each<const Town, const Transform>(
	    [&point, &result, &closest](entt::entity entity, [[maybe_unused]] auto& town, [[maybe_unused]] auto& transform) {
		    const auto delta = point - transform.position;
		    const float distance2 = glm::dot(delta, delta);
		    if (distance2 < closest)
		    {
			    closest = distance2;
			    result = entity;
		    }
	    });

	return result;
}

bool TownSystem::AddVillagerToTown(entt::entity town, entt::entity villager)
{
	return town_homes::AddVillagerToTown(Locator::entitiesRegistry::value(), GameHomes(), town, villager);
}

void TownSystem::AddVillagerToAbode(entt::entity abode, entt::entity villager)
{
	town_homes::AddVillagerToAbode(Locator::entitiesRegistry::value(), GameHomes(), abode, villager);
}

bool TownSystem::MakeHomeless(entt::entity villager)
{
	return town_homes::MakeHomeless(Locator::entitiesRegistry::value(), GameHomes(), villager);
}

namespace
{
/// Whether a thing stands in the way of ground kept clear
bool Blocks(const Registry& registry, entt::entity thing, TownSystemInterface::ClearAreaFilter filter)
{
	if (filter == TownSystemInterface::ClearAreaFilter::AnyObject)
	{
		return true;
	}
	// Of the things that stay put, all but trees
	return registry.AllOf<Fixed>(thing) && !registry.AllOf<Tree>(thing);
}

/// A gathering place is looked for this far round the middle of a town's buildings, in steps, clear this far round
constexpr float k_CongregationSearch = 130.0f;
constexpr float k_CongregationStep = 3.0f;
constexpr float k_CongregationClear = 10.0f;
/// Failing that it is near a building, this far and up to as far again
constexpr float k_CongregationNearLeast = 10.0f;
constexpr float k_CongregationNearRange = 10.0f;
/// A new building this close to the gathering place, past its own size, moves it
constexpr float k_CongregationBuildingGap = 7.5f;
} // namespace

bool TownSystem::CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter, entt::entity ignore) const
{
	if (!Locator::entitiesMap::has_value())
	{
		return true;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto& registry = Locator::entitiesRegistry::value();
	auto cell = map_coords::CellOf(point);
	map_coords::Spiral spiral;
	for (auto count = map_coords::CellSpiralSize(radius); count != 0; --count)
	{
		if (map_coords::InBounds(cell))
		{
			for (const auto thing : map.GetAllInCell(cell))
			{
				const auto* transform = registry.TryGet<const Transform>(thing);
				if (transform == nullptr || thing == ignore)
				{
					continue;
				}
				const float gap = glm::distance(glm::vec2(transform->position.x, transform->position.z), point) -
				                  world_objects::SizeOf(thing).radius;
				if (gap < radius && Blocks(registry, thing, filter))
				{
					return false;
				}
			}
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
	}
	return true;
}

std::optional<glm::vec2> TownSystem::FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
                                                   ClearAreaFilter filter, entt::entity ignore) const
{
	auto coords = map_coords::FromMetres(point);
	map_coords::Spiral spiral;
	for (auto count = map_coords::IncrementSpiralSize(searchRadius, step); count != 0; --count)
	{
		const auto at = map_coords::ToMetres(coords);
		if (CheckForClearArea(at, radius, filter, ignore))
		{
			return at;
		}
		map_coords::SpiralIncrement(coords, spiral, step);
	}
	return std::nullopt;
}

glm::vec2 TownSystem::GetCongregationPos(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& town = registry.Get<Town>(townEntity);
	if (town.congregationPos.has_value())
	{
		return *town.congregationPos;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	// The town's buildings other than fields, in the order the town keeps them
	// TODO(villagers): a town with fewer than three counts its planned buildings too; openblack has none yet
	std::vector<glm::vec2> buildings;
	for (const auto abode : town.abodes)
	{
		const auto* data = registry.TryGet<const Abode>(abode);
		const auto* transform = registry.TryGet<const Transform>(abode);
		if (data == nullptr || transform == nullptr || infos.at(static_cast<size_t>(data->type)).abodeType == AbodeType::Field)
		{
			continue;
		}
		buildings.emplace_back(transform->position.x, transform->position.z);
	}
	std::optional<glm::vec2> found;
	const auto& townTransform = registry.Get<const Transform>(townEntity);
	glm::vec2 centre {townTransform.position.x, townTransform.position.z};
	if (buildings.size() > 1)
	{
		// The middle of the buildings, on the map's grid; all of them are used up finding it, so failing a clear area
		// there it is near the town itself
		uint32_t x = 0;
		uint32_t z = 0;
		for (const auto& building : buildings)
		{
			const auto coords = map_coords::FromMetres(building);
			x += static_cast<uint32_t>(coords.x);
			z += static_cast<uint32_t>(coords.z);
		}
		const auto count = static_cast<float>(buildings.size());
		const glm::vec2 middle {map_coords::ToMetres(map_coords::FtoL(static_cast<float>(x) / count)),
		                        map_coords::ToMetres(map_coords::FtoL(static_cast<float>(z) / count))};
		found = FindClearArea(middle, k_CongregationSearch, k_CongregationStep, k_CongregationClear,
		                      ClearAreaFilter::BlocksTown, entt::null);
	}
	else if (!buildings.empty())
	{
		centre = buildings.front();
	}
	if (!found.has_value())
	{
		auto& random = Locator::gameRandom::value();
		const float distance = random.GameFloatRand(k_CongregationNearRange) + k_CongregationNearLeast;
		const float angle = random.GameFloatRand(villager_routine::k_TwoPi);
		found = villager_routine::Polar(centre, angle, distance);
	}
	town.congregationPos = found;
	return *found;
}

void TownSystem::BuildingCreated(entt::entity townEntity, glm::vec2 position, float radius)
{
	auto& town = Locator::entitiesRegistry::value().Get<Town>(townEntity);
	if (town.congregationPos.has_value() && glm::distance(position, *town.congregationPos) - radius < k_CongregationBuildingGap)
	{
		town.congregationPos.reset();
	}
}

bool TownSystem::IsInStateOfEmergency(entt::entity townEntity) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* town = registry.TryGet<const Town>(townEntity);
	return town != nullptr &&
	       villager_routine::IsInStateOfEmergency(town->emergencyTurn, Locator::time::value().GetTurn(),
	                                              Locator::infoConstants::value().town.gameTurnsAfterEmergencyVillagersReact);
}

void TownSystem::SetInStateOfEmergency(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& town = registry.Get<Town>(townEntity);
	const auto turn = Locator::time::value().GetTurn();
	if (town.emergencyTurn != 0 || !Locator::livingActionSystem::has_value())
	{
		town.emergencyTurn = turn;
		return;
	}
	// Everyone living in its buildings who is about something an emergency breaks into goes to gather; those inside, in
	// bed or sitting about are left be
	auto& living = Locator::livingActionSystem::value();
	for (const auto abode : town.abodes)
	{
		const auto* data = registry.TryGet<const Abode>(abode);
		if (data == nullptr)
		{
			continue;
		}
		for (const auto villager : data->inhabitants)
		{
			auto* action = registry.TryGet<LivingAction>(villager);
			if (action != nullptr && villager_routine::AnswersTownEmergency(living.VillagerGetFinalState(*action)))
			{
				living.VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::GotoCongregateInTownAfterEmergency,
				                        false);
			}
		}
	}
	// The town counts as in an emergency only once its people have been called
	town.emergencyTurn = turn;
}

void TownSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> towns;
	registry.Each<const Town>([&towns](entt::entity town, const Town&) { towns.push_back(town); });
	const auto& infos = Locator::infoConstants::value().abode;
	const auto* fire = Locator::fireSystem::has_value() ? &Locator::fireSystem::value() : nullptr;
	for (const auto townEntity : towns)
	{
		if (IsInStateOfEmergency(townEntity))
		{
			// TODO(villagers): an attack on the town renews it, and its worship stops while it lasts
			continue;
		}
		// Its storage pit or village centre on fire calls one; otherwise one that ran its time is over
		bool burning = false;
		for (const auto abode : registry.Get<const Town>(townEntity).abodes)
		{
			const auto* data = registry.TryGet<const Abode>(abode);
			if (data == nullptr || fire == nullptr)
			{
				continue;
			}
			const auto type = infos.at(static_cast<size_t>(data->type)).abodeType;
			if ((type == AbodeType::StoragePit || type == AbodeType::TownCentre) && fire->IsOnFire(abode))
			{
				burning = true;
				break;
			}
		}
		if (burning)
		{
			SetInStateOfEmergency(townEntity);
			continue;
		}
		registry.Get<Town>(townEntity).emergencyTurn = 0;
	}
}
