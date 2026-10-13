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

#include <functional>
#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "Magic/VortexCrossingRules.h"

namespace openblack::ecs
{
class Registry;
}

/// The crossing file's line for a thing a vortex takes, read from its components
namespace openblack::ecs::vortex_crossing
{

/// What the lines need from the rest of the game
struct World
{
	/// The game turn now, for the ages
	uint32_t turn {0};
	/// The land's height at a point across it
	std::function<float(glm::vec2)> ground;
	/// The row of the villagers' table of a tribe's job
	std::function<int32_t(Tribe, VillagerNumber)> villagerRow;
	/// The kind of resource a kind of pot holds
	std::function<int32_t(PotInfo)> potResource;
	/// Its life, 0 to 1
	std::function<float(entt::entity)> life;
	/// The player it belongs to, none for nobody
	std::function<std::optional<PlayerNames>(entt::entity)> player;
	/// The name of the miracle a seed casts at a power-up level
	std::function<std::string(SpellSeedType, int)> magicName;
};

/// Where a thing stands as a map position: across the land, and its height above the land there
[[nodiscard]] map_coords::MapCoords MapPlace(glm::vec3 position, float ground);

/// The line making the thing again relative to the vortex's place: a villager, an animal, a pot (not one of a storage
/// pit's piles), a tree, a rock or other static thing, a thing lying about, a dead tree or a globe holding a miracle. None
/// for anything else.
[[nodiscard]] std::optional<vortex::crossing::Line> LineFor(const Registry& registry, entt::entity thing,
                                                            const map_coords::MapCoords& vortex, const World& world);

} // namespace openblack::ecs::vortex_crossing
