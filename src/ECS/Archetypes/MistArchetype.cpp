/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MistArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"
#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MistArchetype::Create(const glm::vec3& position, float altitude, uint32_t colour, float size, float k)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	// the 3D object at (x, the land's height + altitude, z)
	glm::vec3 world = position;
	world.y = altitude;
	if (Locator::terrainSystem::has_value())
	{
		world.y += Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	registry.Assign<Transform>(entity, world, glm::mat3(1.0f), glm::vec3(size));

	Mist mist {};
	mist.size = size;
	mist.colour = colour;
	mist.edgeShrink = k != 1.0f;
	mist.k = mist.edgeShrink ? k : 3.0f;
	// the animation counter starts at truncate(Random(0, 16)) & 15, on the CRT rand() stream, not the game's synced
	// one
	mist.counter = graphics::frame_anim::MistStartCounter(game_random::crt::Random(0.0f, 16.0f));
	mist.counterRemainder = 0.0f;
	registry.Assign<Mist>(entity, mist);
	return entity;
}
