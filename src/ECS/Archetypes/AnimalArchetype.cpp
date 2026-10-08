/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalArchetype.h"

#include <algorithm>

#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Flocks.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
enum class AnimalClass
{
	None,
	Ground,
	/// made by the grazing animals' constructor: they can be shepherded
	Grazing,
	/// the flying animals' constructor (doves, crows, swallows, pigeons, seagulls, bats and the spell ones)
	Flying,
};

/// Each species' class factory, chosen by info.animalInfo (27 cases). 5 (goat), 7 (zebra), 17-19 and anything above
/// 26 (the puzzle horse, cow, tortoise and pig) make nothing.
AnimalClass ClassOf(const GAnimalInfo& info)
{
	switch (static_cast<int>(info.animalInfo))
	{
	case 4:  // Sheep
	case 6:  // Tortoise
	case 8:  // Cow
	case 9:  // Horse
	case 10: // Pig
	case 24: // PieceSheep
		return AnimalClass::Grazing;
	case 11: // Crow
	case 12: // Dove
	case 13: // Swallow
	case 14: // Pigeon
	case 15: // Seagull
	case 16: // Bat
	case 20: // SpellDove
	case 21: // SpellDove (the spell bat)
		return AnimalClass::Flying;
	case 0:  // Lion
	case 1:  // Tiger
	case 2:  // Wolf
	case 3:  // Leopard
	case 22: // SpellWolf
	case 23: // PieceLion
	case 25: // PieceWolf
	case 26: // PieceVillager
		return AnimalClass::Ground;
	default:
		return AnimalClass::None;
	}
}

/// GameRand(range) + 5 (range 40 alone, 20 in a flock)
uint32_t RandomAge(uint32_t range)
{
	return game_random::GameRand(range) + 5;
}

/// The class factory and CallVirtualFunctionsForCreation of an animal
entt::entity MakeAnimal(const glm::vec3& position, AnimalInfo type, const GAnimalInfo& info, uint32_t age)
{
	// the initial scale, then the scale for its age (animal_ai: the script's SET_PROPERTY Age too)
	const float scale = ecs::animal_ai::ScaleForAge(info, age, ecs::animal_ai::InitialScaleForAge(info, age));

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// no start angle in the Object / Living constructors; standing on the land
	glm::vec3 ground = position;
	if (Locator::terrainSystem::has_value())
	{
		ground.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	// a flying animal: its altitude = info.altitudeNormal, drawn at the land height + it
	if (ClassOf(info) == AnimalClass::Flying)
	{
		ground.y += info.altitudeNormal;
	}
	registry.Assign<Transform>(entity, ground, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<Animal>(entity, type, age, entt::null, true);
	// the object gets the detail mesh (high, std, low), and the level-of-detail loads are disabled (always LOD 1): the
	// std mesh, which is also its mesh
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.std), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// at the head of its cell's mobile list
	ecs::map_cells::InsertMapObject(entity);
	// a predator's flee-from-predator reaction, spread once to the animals already around it
	ecs::animal_ai::SpreadPredatorReaction(entity);
	return entity;
}

/// Joins the flock (flocks::AddMember: its ordering, the member's flock)
void JoinFlock(entt::entity flockEntity, [[maybe_unused]] Flock& flock, entt::entity animal)
{
	ecs::flocks::AddMember(flockEntity, animal);
}

/// No flock given
entt::entity CreateAlone(const glm::vec3& position, AnimalInfo type, const GAnimalInfo& info, entt::entity town, uint32_t age)
{
	if (age == 0)
	{
		age = RandomAge(40);
	}
	const auto animalClass = ClassOf(info);
	if (animalClass == AnimalClass::None)
	{
		return entt::null;
	}
	const auto entity = MakeAnimal(position, type, info, age);
	auto& registry = Locator::entitiesRegistry::value();
	// a flock of its own at the animal's position, with the species' domain radius and flock distance
	const auto flockEntity = registry.Create();
	auto& flock = registry.Assign<Flock>(flockEntity);
	flock.domainCentre = registry.Get<Transform>(entity).position;
	flock.savedDomainCentre = flock.domainCentre;
	flock.domainRadius = static_cast<uint16_t>(info.domainRadius);
	flock.flockDistance = static_cast<uint16_t>(static_cast<int32_t>(info.flockDistance));
	JoinFlock(flockEntity, flock, entity);
	flock.maxMembers = std::max(flock.maxMembers, static_cast<uint32_t>(flock.members.size()));
	// a grazing animal takes the town, any other none
	ecs::animal_ai::SetTown(entity, animalClass == AnimalClass::Grazing ? town : entt::null);
	return entity;
}
} // namespace

entt::entity AnimalArchetype::Create(const glm::vec3& position, AnimalInfo type, entt::entity town, entt::entity flock,
                                     uint32_t age)
{
	if (type == AnimalInfo::None || type >= AnimalInfo::_COUNT)
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
	auto& registry = Locator::entitiesRegistry::value();
	if (flock == entt::null || !registry.Valid(flock) || !registry.AllOf<Flock>(flock))
	{
		return CreateAlone(position, type, info, town, age);
	}

	if (age == 0)
	{
		age = RandomAge(20);
	}
	const auto animalClass = ClassOf(info);
	if (animalClass == AnimalClass::None)
	{
		// (the original doesn't check the class factory's result here)
		return entt::null;
	}
	auto& flockData = registry.Get<Flock>(flock);
	const bool grazing = animalClass == AnimalClass::Grazing;
	// an animal that can't be shepherded takes the flock off its town's list
	if (!grazing && flockData.town != entt::null)
	{
		if (auto* flockTown = registry.TryGet<Town>(flockData.town))
		{
			std::erase(flockTown->flocks, flock);
		}
		flockData.town = entt::null;
	}
	const auto entity = MakeAnimal(position, type, info, age);
	JoinFlock(flock, flockData, entity);
	flockData.maxMembers = std::max(flockData.maxMembers, static_cast<uint32_t>(flockData.members.size()));
	// a grazing animal takes the town, any other none
	ecs::animal_ai::SetTown(entity, grazing ? town : entt::null);
	return entity;
}

entt::entity AnimalArchetype::Create(const glm::vec3& position, AnimalInfo type, int32_t, uint32_t age)
{
	return Create(position, type, entt::null, entt::null, age);
}
