/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <unordered_map>

#include <CreatureRecord.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

// The parts that the imports of a world file of the game share
namespace openblack::ecs::systems::vanilla_import
{

/// The game's map position as a world point, on the ground plus the altitude
[[nodiscard]] glm::vec3 GroundPosition(const gamesave::MapCoords& position);

/// The entity that an object number of the file became, or no entity
[[nodiscard]] inline entt::entity EntityOf(const std::unordered_map<uint32_t, entt::entity>& made, uint32_t number)
{
	const auto found = made.find(number);
	return found != made.end() ? found->second : entt::null;
}

} // namespace openblack::ecs::systems::vanilla_import
