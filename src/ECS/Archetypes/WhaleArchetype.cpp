/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WhaleArchetype.h"

#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/AnimalRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Whale.h"
#include "ECS/Registry.h"
#include "ECS/WhaleRules.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity WhaleArchetype::Create(const glm::vec3& position, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	const glm::vec2 xz(map_coords::Quantise(position.x), map_coords::Quantise(position.z));
	const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
	const glm::vec3 at(xz.x, height, xz.y);
	// Twice the size it is made at, facing +x
	registry.Assign<Transform>(entity, at, animals::Orientation(0.0f, 0.0f), glm::vec3(scale * 2.0f));
	// The boned shark, not the mobile object table's model for a whale
	registry.Assign<Mesh>(entity, resources::HashIdentifier(MeshId::SharkBoned), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	registry.Assign<Whale>(entity, Whale {.position = at, .turnStart = at});
	// Posed by its swimming clip as the animals are, in its own dark blue lit by the game's light, and cut at the sea
	registry.Assign<AnimalPose>(
	    entity, AnimalPose {.light = AnimalLight::Own, .colour = whale_rules::k_Colour, .cutBelow = whale_rules::k_SeaLevel});
	return entity;
}
