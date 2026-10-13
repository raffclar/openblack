/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalArchetype.h"

#include <array>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "Animals/BirdRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity AnimalArchetype::Create(AnimalInfo type, const glm::vec3& position, float heading, float scale, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, animals::Orientation(heading, 0.0f), glm::vec3(scale));
	// The rest of the game knows a bird by its kind's middle model and any other animal by its high one
	const auto mesh = animals::birds::IsBird(type) ? info.std : info.high;
	registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// Drawn in less detail further off, by its kind's high, middle and low models (a bird's low model is its middle one)
	registry.Assign<DetailMeshes>(entity,
	                              std::array {resources::HashIdentifier(info.high), resources::HashIdentifier(info.std),
	                                          resources::HashIdentifier(info.low)},
	                              0.0f);
	auto& animal = registry.Assign<Animal>(entity);
	animal.type = type;
	animal.owner = owner;
	animal.heading = heading;
	animal.previousHeading = heading;
	animal.position = position;
	animal.previousPosition = position;
	// Its walk starts where it stands, whoever made it
	animal.move.position = {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z)};
	animal.move.goal = animal.move.position;
	animal.animation = info.defaultAnim;
	return entity;
}
