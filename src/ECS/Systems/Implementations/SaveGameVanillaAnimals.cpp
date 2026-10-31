/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <VanillaWorld.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/VanillaSceneryImport.h"
#include "ECS/VanillaScriptImport.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace world_import = openblack::ecs::vanilla_world_import;
namespace script_import = openblack::ecs::vanilla_script_import;
using openblack::ecs::systems::vanilla_import::EntityOf;

namespace
{
std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

map_coords::MapCoords CoordsOf(const gamesave::MapCoords& coords)
{
	return {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
}

/// The game clears the map before it reads the world of a file. The import removes every animal of the land and each
/// flock that has no member left, but it keeps the flocks of the temples, which the file's flocks of the temples become.
void RemoveAnimals(ecs::Registry& registry)
{
	std::vector<entt::entity> animals;
	registry.Each<const Animal>([&animals](entt::entity entity, const Animal&) { animals.push_back(entity); });
	for (const auto entity : animals)
	{
		ecs::world_objects::Remove(entity);
	}
	std::vector<entt::entity> flocks;
	registry.Each<Flock>([&registry, &flocks](entt::entity entity, Flock& flock) {
		std::erase_if(flock.members, [&registry](entt::entity member) { return !registry.Valid(member); });
		if (flock.temple == entt::null && flock.members.empty())
		{
			flocks.push_back(entity);
		}
	});
	for (const auto entity : flocks)
	{
		registry.Destroy(entity);
	}
}

/// The temple of a player, none when the player has no temple
entt::entity TempleOf(ecs::Registry& registry, PlayerNames player)
{
	entt::entity found = entt::null;
	registry.Each<const Temple>([&found, player](entt::entity entity, const Temple& temple) {
		if (found == entt::null && temple.owner == player)
		{
			found = entity;
		}
	});
	return found;
}
} // namespace

void SaveGameSystem::ImportVanillaAnimals(const gamesave::VanillaWorld& world, VanillaMade& made)
{
	if (!Locator::animalSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& system = Locator::animalSystem::value();
	const auto imported = world_import::ReadAnimals(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not make the animal thing {}", why);
	}
	RemoveAnimals(registry);
	// The flocks first, so that the animals can join them
	uint32_t flocks = 0;
	uint32_t templeFlocks = 0;
	for (const auto& file : imported.flocks)
	{
		entt::entity entity = entt::null;
		if (file.heart != 0)
		{
			const auto player = script_import::HeartPlayer(world, file.heart);
			entity = player.has_value() ? system.TempleFlock(TempleOf(registry, *player)) : entt::null;
			if (entity == entt::null)
			{
				SPDLOG_LOGGER_WARN(ImportLog(), "The flock {} flies round a temple that the land does not have", file.number);
				continue;
			}
			++templeFlocks;
		}
		else
		{
			entity = system.CreateFlock(glm::vec2(map_coords::ToMetres(file.place.x), map_coords::ToMetres(file.place.z)),
			                            static_cast<float>(file.domainRadius), static_cast<float>(file.flockDistance));
			registry.Get<Flock>(entity).town = EntityOf(made, file.town);
		}
		auto& flock = registry.Get<Flock>(entity);
		flock.place = CoordsOf(file.place);
		flock.savedPlace = CoordsOf(file.savedPlace);
		flock.domainRadius = file.domainRadius;
		flock.flockDistance = file.flockDistance;
		flock.shepherd = EntityOf(made, file.shepherd);
		flock.turnsOnLeg = file.turnsOnLeg;
		flock.keepsTogether = file.keepsTogether;
		flock.calm = file.calm;
		flock.scriptId = file.scriptId;
		flock.height = file.height;
		flock.members.clear();
		made.emplace(file.number, entity);
		++flocks;
	}
	// The animals join their flocks in the order of the lines of the flocks, so that each line keeps its leader last
	std::unordered_map<uint32_t, const world_import::ImportedAnimal*> animals;
	for (const auto& animal : imported.animals)
	{
		animals.emplace(animal.number, &animal);
	}
	std::vector<const world_import::ImportedAnimal*> order;
	for (const auto& file : imported.flocks)
	{
		for (const auto member : file.members)
		{
			if (const auto found = animals.find(member); found != animals.end())
			{
				order.push_back(found->second);
				animals.erase(found);
			}
		}
	}
	for (const auto& animal : imported.animals)
	{
		if (animals.contains(animal.number))
		{
			order.push_back(&animal);
		}
	}
	uint32_t lost = 0;
	for (const auto* animal : order)
	{
		const auto entity = system.CreateLoadedAnimal({.type = animal->row,
		                                               .position = {animal->position.x, animal->position.z},
		                                               .altitude = animal->position.altitude,
		                                               .goal = {animal->goal.x, animal->goal.z},
		                                               .step = {animal->stepX, animal->stepZ},
		                                               .angle = animal->angle,
		                                               .speed = animal->speed,
		                                               .moveState = animal->moveState,
		                                               .scale = animal->scale,
		                                               .life = animal->life,
		                                               .birthTurn = animal->birthTurn,
		                                               .state = animal->state,
		                                               .finalState = animal->finalState,
		                                               .flock = EntityOf(made, animal->flock),
		                                               .town = EntityOf(made, animal->town),
		                                               .hunger = animal->hunger,
		                                               .sleep = animal->sleep,
		                                               .breed = animal->breed,
		                                               .sleepCell = animal->sleepCell,
		                                               .bank = animal->bank});
		if (entity == entt::null)
		{
			++lost;
			continue;
		}
		made.emplace(animal->number, entity);
	}
	// Each flock keeps the most members that it had in the file
	for (const auto& file : imported.flocks)
	{
		if (const auto entity = EntityOf(made, file.number); entity != entt::null && registry.AllOf<Flock>(entity))
		{
			registry.Get<Flock>(entity).most = file.mostMembers;
		}
	}
	SPDLOG_LOGGER_INFO(ImportLog(), "Made {} animals ({} not made) and {} flocks ({} of temples)", order.size() - lost, lost,
	                   flocks, templeFlocks);
}
