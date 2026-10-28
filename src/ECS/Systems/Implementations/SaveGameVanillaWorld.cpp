/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <iterator>
#include <ranges>
#include <unordered_map>
#include <vector>

#include <VanillaWorld.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VanillaWorldImport.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace world_import = openblack::ecs::vanilla_world_import;
using openblack::ecs::systems::vanilla_import::EntityOf;
using openblack::ecs::systems::vanilla_import::GroundPosition;

namespace
{
std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

/// The game clears the map before it reads the world of a file. The import removes the villagers of the land, and the
/// buildings of the kinds that it makes again and the fields, before it makes them from the file.
void RemoveLandTownsContents(ecs::Registry& registry)
{
	std::vector<entt::entity> villagers;
	registry.Each<const Villager>([&villagers](entt::entity entity, const Villager&) { villagers.push_back(entity); });
	for (const auto villager : villagers)
	{
		if (Locator::townSystem::has_value())
		{
			Locator::townSystem::value().RemoveVillager(villager);
		}
		ecs::world_objects::Remove(villager);
	}
	registry.Context().namedVillagersUsed = {};

	// A building that goes puts a plan of itself into its town. The import keeps only the plans that the towns had.
	std::unordered_map<entt::entity, std::vector<entt::entity>> plans;
	registry.Each<const Town>([&plans](entt::entity entity, const Town& town) { plans[entity] = town.planned; });
	std::vector<entt::entity> buildings;
	registry.Each<const Abode>([&registry, &buildings](entt::entity entity, const Abode& abode) {
		if (world_import::IsImportedBuilding(abode.info) || registry.AllOf<Field>(entity))
		{
			buildings.push_back(entity);
		}
	});
	for (const auto building : buildings)
	{
		ecs::world_objects::Remove(building);
	}
	registry.Each<Town>([&registry, &plans](entt::entity entity, Town& town) {
		const auto& kept = plans[entity];
		std::vector<entt::entity> added;
		std::ranges::copy_if(town.planned, std::back_inserter(added),
		                     [&kept](entt::entity plan) { return std::ranges::find(kept, plan) == kept.end(); });
		std::erase_if(town.planned, [&added](entt::entity plan) { return std::ranges::find(added, plan) != added.end(); });
		for (const auto plan : added)
		{
			if (registry.Valid(plan))
			{
				registry.Destroy(plan);
			}
		}
		town.homelessVillagers.clear();
	});
	registry.SetDirty();
}

/// The town of the file: the town of the land with the same number, or a new town
entt::entity PutTown(ecs::Registry& registry, const world_import::ImportedTown& imported)
{
	const auto position = GroundPosition(imported.position);
	auto& towns = registry.Context().towns;
	auto found = towns.find(static_cast<int>(imported.id));
	const auto entity = found != towns.end() ? found->second
	                                         : ecs::archetypes::TownArchetype::Create(static_cast<int>(imported.id), position,
	                                                                                  imported.owner, imported.tribe);
	auto& town = registry.Get<Town>(entity);
	town.owner = imported.owner;
	town.uninhabitable = imported.uninhabitable;
	town.cannotHaveWorshipSite = imported.cannotBuildWorshipSite;
	town.emptyTownTimer = imported.emptyTownTimer;
	registry.AssignOrReplace<Tribe>(entity, imported.tribe);
	if (auto* transform = registry.TryGet<Transform>(entity))
	{
		transform->position = position;
	}
	return entity;
}

/// The tag of the move state of the file, for a walk that the import keeps
void PutMoveTag(ecs::Registry& registry, entt::entity entity, uint8_t moveState, glm::vec2 at)
{
	switch (moveState)
	{
	case 0x01:
		registry.Assign<MoveStateArrivedTag>(entity, MoveStateClockwise::Undefined, at);
		break;
	case 0x04:
		registry.Assign<MoveStateFinalStepTag>(entity, MoveStateClockwise::Undefined, at);
		break;
	case 0x0B:
		registry.Assign<MoveStateStepThroughTag>(entity, MoveStateClockwise::Undefined, at);
		break;
	case 0x0C:
		registry.Assign<MoveStateLinearTag>(entity, MoveStateClockwise::Undefined, at);
		break;
	case 0x0D:
		registry.Assign<MoveStateLinearTag>(entity, MoveStateClockwise::Clockwise, at);
		break;
	case 0x0E:
		registry.Assign<MoveStateLinearTag>(entity, MoveStateClockwise::CounterClockwise, at);
		break;
	default:
		break;
	}
}

/// The villager keeps the walk of the file: its goal, its step, its speed, the way it faces and its move state
void PutWalk(ecs::Registry& registry, entt::entity entity, const world_import::ImportedVillager& villager, glm::vec2 goal)
{
	const auto& walk = villager.walk;
	auto& wallHug = registry.Get<WallHug>(entity);
	wallHug.goal = goal;
	wallHug.step = {walk.stepX, walk.stepZ};
	wallHug.stepAltitude = walk.stepAltitude;
	wallHug.yAngle = villager.heading;
	wallHug.speed = world_import::WalkSpeedOf(walk.speed);
	wallHug.gameAngle = walk.angle;
	wallHug.turnsUntilStepRebuild = walk.turnsUntilStepRebuild;
	// The walk holds the villager at the map position of the file
	wallHug.position = {villager.position.x, villager.position.z};
	wallHug.placedAt = glm::vec2(registry.Get<Transform>(entity).position.x, registry.Get<Transform>(entity).position.z);
	PutMoveTag(registry, entity, villager.moveState, ecs::wall_hug::ToPoint(wallHug.position));
}

} // namespace

glm::vec3 openblack::ecs::systems::vanilla_import::GroundPosition(const gamesave::MapCoords& position)
{
	const map_coords::MapCoords coords {position.x, position.z, position.altitude};
	if (Locator::terrainSystem::has_value())
	{
		return map_coords::ToWorld(Locator::terrainSystem::value(), coords);
	}
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

bool SaveGameSystem::ImportVanillaTowns(const gamesave::VanillaWorld& world, VanillaMade& made, VanillaWorks& works)
{
	const auto imported = world_import::ReadTowns(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take {}", why);
	}
	auto& registry = Locator::entitiesRegistry::value();

	// The ages and the timers of the villagers count from the turn of the file
	if (Locator::time::has_value())
	{
		Locator::time::value().ResumeAtTurn(imported.turn, 0);
	}
	_lastAutosaveTurn = imported.turn;
	_lastLoadTurn = imported.turn;

	RemoveLandTownsContents(registry);

	std::unordered_map<uint32_t, uint32_t> townIds;
	for (const auto& town : imported.towns)
	{
		made.emplace(town.number, PutTown(registry, town));
		townIds.emplace(town.number, town.id);
	}

	for (const auto& abode : imported.abodes)
	{
		const auto townId = townIds.find(abode.town);
		if (townId == townIds.end())
		{
			SPDLOG_LOGGER_WARN(ImportLog(), "The building {} of the file has no town of the file, so it is not made",
			                   abode.number);
			continue;
		}
		const auto entity =
		    ecs::archetypes::AbodeArchetype::Create(townId->second, GroundPosition(abode.position), abode.row, abode.heading,
		                                            abode.scale, abode.food, abode.wood, abode.built);
		if (entity == entt::null)
		{
			continue;
		}
		ecs::world_objects::SetLife(entity, abode.life);
		made.emplace(abode.number, entity);
	}

	uint32_t present = 0;
	for (const auto& villager : imported.villagers)
	{
		auto states = world_import::StartStates(villager.states, villager.moveState, villager.walk.hugsCircle);
		const auto goalX = world_import::MetresOfWhole(villager.walk.goal.x);
		const auto goalZ = world_import::MetresOfWhole(villager.walk.goal.z);
		const bool keeps = states == villager.states;
		// A villager that walks, or that goes back to a walk, to a goal that no metre value gives again decides again.
		// A villager in another state makes a new walk before it walks again, so its goal is not used.
		const bool exactGoal = goalX.has_value() && goalZ.has_value();
		if (keeps && !exactGoal && world_import::UsesWalkGoal(states))
		{
			states = world_import::DecideAgain();
		}
		const auto entity =
		    ecs::archetypes::VillagerArchetype::CreateLoaded({.position = GroundPosition(villager.position),
		                                                      .heading = villager.heading,
		                                                      .scale = villager.scale,
		                                                      .type = villager.row,
		                                                      .nameRow = villager.nameRow,
		                                                      .life = villager.life,
		                                                      .birthTurn = villager.birthTurn,
		                                                      .food = villager.food,
		                                                      .lastCheckTurn = villager.lastCheckTurn,
		                                                      .made = villager.made,
		                                                      .foodSpeedUp = villager.foodSpeedUp,
		                                                      .foodHeld = villager.foodHeld,
		                                                      .woodHeld = villager.woodHeld,
		                                                      .pregnancy = villager.pregnancy,
		                                                      .states = states,
		                                                      .turnsUntilStateChange = villager.turnsUntilStateChange,
		                                                      .turnsSinceStateChange = villager.turnsSinceStateChange});
		made.emplace(villager.number, entity);
		if (states == villager.states)
		{
			PutWalk(registry, entity, villager,
			        {goalX.value_or(map_coords::ToMetres(villager.walk.goal.x)),
			         goalZ.value_or(map_coords::ToMetres(villager.walk.goal.z))});
			if (auto work = world_import::WorkOf(states[0], villager.topValues); work.has_value())
			{
				works.emplace_back(entity, *work);
			}
			else if (auto finalWork = world_import::WorkOf(states[1], villager.finalValues); finalWork.has_value())
			{
				works.emplace_back(entity, *finalWork);
			}
		}
		auto& person = registry.Get<Villager>(entity);
		person.town = EntityOf(made, villager.town);
		person.abode = EntityOf(made, villager.home);
		if (person.abode != entt::null && world_import::IsInsideHome(static_cast<VillagerStates>(states[0])))
		{
			registry.Assign<AtHome>(entity,
			                        AtHome {.beenToBed = states[0] == static_cast<uint8_t>(VillagerStates::SleepingAtHome)});
			++present;
		}
	}
	// The mothers come after all the villagers, because a mother can come later in the file
	for (const auto& villager : imported.villagers)
	{
		registry.Get<Villager>(EntityOf(made, villager.number)).mother = EntityOf(made, villager.mother);
	}

	// Each building takes its villagers in the order of the file, and the villagers that are in it
	uint32_t presentInFile = 0;
	for (const auto& abode : imported.abodes)
	{
		const auto entity = EntityOf(made, abode.number);
		if (entity == entt::null)
		{
			continue;
		}
		auto& building = registry.Get<Abode>(entity);
		building.inhabitants.clear();
		for (const auto number : abode.villagers)
		{
			if (const auto villager = EntityOf(made, number); villager != entt::null)
			{
				building.inhabitants.push_back(villager);
			}
		}
		building.presentAtHome = static_cast<uint32_t>(std::ranges::count_if(
		    building.inhabitants, [&registry](entt::entity villager) { return registry.AllOf<AtHome>(villager); }));
		presentInFile += abode.present;
	}
	if (present != presentInFile)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "{} villagers of the file are in their homes, but the buildings count {}", present,
		                   presentInFile);
	}
	// Each town takes its homeless villagers and the order of its buildings from the file
	for (const auto& town : imported.towns)
	{
		auto& component = registry.Get<Town>(EntityOf(made, town.number));
		for (const auto number : town.homeless)
		{
			if (const auto villager = EntityOf(made, number); villager != entt::null)
			{
				component.homelessVillagers.push_back(villager);
			}
		}
		std::vector<entt::entity> ordered;
		for (const auto number : town.abodes)
		{
			if (const auto abode = EntityOf(made, number);
			    abode != entt::null && std::ranges::find(component.abodes, abode) != component.abodes.end())
			{
				ordered.push_back(abode);
			}
		}
		// The buildings that the import does not make stay after them, in their order
		std::ranges::copy_if(component.abodes, std::back_inserter(ordered),
		                     [&ordered](entt::entity abode) { return std::ranges::find(ordered, abode) == ordered.end(); });
		component.abodes = std::move(ordered);
	}
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(ImportLog(), "Imported {} towns, {} buildings and {} villagers of the game's file at turn {}",
	                   imported.towns.size(), imported.abodes.size(), imported.villagers.size(), imported.turn);
	return true;
}
