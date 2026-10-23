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
#include <random>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <WorldFile.h>
#include <entt/entity/fwd.hpp>
#include <entt/meta/context.hpp>

#include "3D/TempleInteriorInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Reflection/WorldArchive.h"

/// The whole world of a saved game in openblack's own layout (deviation 83): the entities and their components, the
/// values that the registry keeps beside them, the game clock, the streams of random numbers, the camera, and the
/// scripts with the table of the objects that they hold
namespace openblack::ecs::world_save
{

/// What a save of the whole world did not keep
struct SaveSummary
{
	/// The values in the context of the registry that the archive cannot keep
	std::set<reflection::UnsavedValue> context;
};

/// A block of the whole world. The game's thread takes what the block needs now; the function makes the bytes of the
/// block later, on the thread of the writer.
struct PendingChunk
{
	gamesave::Tag tag {};
	std::function<std::vector<uint8_t>()> bytes;
};

/// The game clock and the places of the streams of random numbers, as a save keeps them
struct ClockState
{
	/// The speed of the clock, whether the player paused it, and the time after the last turn
	float speed = 1.0f;
	bool paused = false;
	int32_t remainderMs = 0;
	/// The two seeds of the game, the stream that the particle effects draw from, and the seed of the C library
	GameRandomSeeds seeds;
	ParticleRandomStream particles = ParticleRandomStream::None;
	std::optional<uint32_t> crtSeed;
	/// The seed of the run, each stream of the run, and the general stream of the game's thread
	uint32_t runSeed = 0;
	std::vector<std::mt19937> streams;
	std::optional<std::mt19937> general;
};

/// The bytes of the block of the clock
[[nodiscard]] std::vector<uint8_t> WriteClock(const ClockState& state);
/// The clock that the bytes keep. Nothing when the bytes do not read. A save of an older build has no general stream.
[[nodiscard]] std::optional<ClockState> ReadClock(std::span<const uint8_t> bytes);

/// The blocks of the whole world, from the services of the game
[[nodiscard]] std::vector<PendingChunk> SaveWorld(const entt::meta_ctx& reflection, SaveSummary& summary);

/// Whether the file keeps the whole world, or only the creatures of an older save
[[nodiscard]] bool HasWholeWorld(const gamesave::WorldFile& file);

/// What a load of the whole world is for
enum class LoadKind : uint8_t
{
	/// A saved game: the load forgets what the game keeps in no save, such as the sounds that played
	SavedGame,
	/// The world state of a game server: the load puts back the world exactly, so that it turns as the server's world.
	/// Only the parts that show the world on one computer, such as the emitters of the sounds, start again.
	WorldState,
};

/// Puts back the entities, their components and the context of the registry. The registry must have no live entity.
/// Returns nothing when the blocks do not read.
[[nodiscard]] std::optional<reflection::WorldLoadReport>
LoadEntities(const entt::meta_ctx& reflection, const gamesave::WorldFile& file, LoadKind kind = LoadKind::SavedGame);

/// The camera that a save keeps: the camera of the world. Inside the temple that is the camera that the player left
/// outside, not the camera of the temple's rooms.
[[nodiscard]] OutsideCamera CameraToSave(const std::optional<OutsideCamera>& outside, const OutsideCamera& current);

/// Forgets what the game does not keep in a save. After a load the creature wants again the actions that the area that
/// holds it refused. The rooms of the temple go, because the temple makes them when the player goes into it.
void ForgetUnsavedState(entt::registry& registry);

/// After the land starts again: puts back the game clock at the turn, the streams of random numbers, the camera, the
/// scripts, the table of the objects that they hold and the running state of the systems. Returns whether every block
/// that the file has read.
bool LoadRunningState(const entt::meta_ctx& reflection, const gamesave::WorldFile& file, uint32_t turn);

} // namespace openblack::ecs::world_save
