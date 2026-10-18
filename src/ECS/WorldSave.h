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

#include <optional>
#include <set>
#include <vector>

#include <WorldFile.h>
#include <entt/meta/context.hpp>

#include "ECS/Reflection/WorldArchive.h"

/// The whole world of a saved game in openblack's own layout (deviation 83): the entities and their components, the
/// values that the registry keeps beside them, the game clock, the streams of random numbers, the camera, and the
/// scripts with the table of the objects that they hold
namespace openblack::ecs::world_save
{

/// What a save of the whole world did not keep
struct SaveSummary
{
	reflection::WorldSaveReport world;
	/// The values in the context of the registry that the archive cannot keep
	std::set<reflection::UnsavedValue> context;
};

/// The blocks of the whole world, from the services of the game
[[nodiscard]] std::vector<gamesave::Chunk> SaveWorld(const entt::meta_ctx& reflection, SaveSummary& summary);

/// Whether the file keeps the whole world, or only the creatures of an older save
[[nodiscard]] bool HasWholeWorld(const gamesave::WorldFile& file);

/// Puts back the entities, their components and the context of the registry. The registry must have no live entity.
/// Returns nothing when the blocks do not read.
[[nodiscard]] std::optional<reflection::WorldLoadReport> LoadEntities(const entt::meta_ctx& reflection,
                                                                      const gamesave::WorldFile& file);

/// After the land starts again: puts back the game clock at the turn, the streams of random numbers, the camera, the
/// scripts, the table of the objects that they hold and the running state of the systems. Returns whether every block
/// that the file has read.
bool LoadRunningState(const entt::meta_ctx& reflection, const gamesave::WorldFile& file, uint32_t turn);

} // namespace openblack::ecs::world_save
