/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeArchetype.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity TreeArchetype::Create(uint32_t forestId, const glm::vec3& position, TreeInfo type, bool isNonScenic,
                                   float yAngleRadians, float maxSize, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));

	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.normal, transform);
	registry.Assign<Fixed>(entity, point, radius);
	// a tree grows only when maxSize differs from the size it is created at, and its first growth step falls on a
	// random turn of [0, growTurns): GameRand(growsAfterNumGameTurns), 0 for 0 without a draw. The wind slot comes from
	// the angle, so that trees facing the same way sway together.
	const bool growing = maxSize != scale;
	const auto turns = static_cast<uint16_t>(growing ? game_random::GameRand(info.growsAfterNumGameTurns) : 0u);
	const auto slot =
	    static_cast<uint8_t>(static_cast<int>(std::floor(yAngleRadians * 16.0f / glm::two_pi<float>() + 0.5f)) & 0xF);
	registry.Assign<Tree>(entity, type, maxSize, forestId, isNonScenic, growing, turns, slot);
	const auto resourceId = resources::HashIdentifier(info.normal);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// at the head of its cell's fixed list
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
