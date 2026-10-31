/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CreatureRecord.h"
#include "SavedObjects.h"

// openblack's own world file of a saved game. The original game writes the whole world as one stream of objects
// without lengths. openblack writes the world in a layout of its own (deviations 46 and 83): a version and then a list
// of blocks. Each block has a name of four letters and a length, so that a reader can skip a block that it does not
// know. The creatures and their leashes also go in one block as a stream in the game's layout.
namespace openblack::gamesave
{

inline constexpr std::array<char, 4> k_WorldMagic {'O', 'B', 'S', 'V'};
inline constexpr uint32_t k_WorldVersion = 4;
/// The first version whose files keep the count of the loads that did not end
inline constexpr uint32_t k_AttemptsVersion = 4;
/// Where the count of the loads that did not end is in a file, from its start
inline constexpr size_t k_AttemptsOffset = 8;
/// A file whose loads did not end this many times does not load again: the game stopped two times as it loaded it
inline constexpr uint32_t k_MostAttempts = 2;

using Tag = std::array<char, 4>;
inline constexpr Tag k_GameTag {'G', 'A', 'M', 'E'};
/// The records of the creatures and of their leashes, as a stream in the game's layout
inline constexpr Tag k_ObjectsTag {'O', 'B', 'J', 'S'};
/// The keys of the numbers in the stream, and the numbers that have no record
inline constexpr Tag k_KeysTag {'K', 'E', 'Y', 'S'};
/// Every entity of the world and every registered component of each entity
inline constexpr Tag k_EntitiesTag {'E', 'C', 'S', 'W'};
/// The values of the land that the registry keeps beside its entities
inline constexpr Tag k_ContextTag {'R', 'C', 'T', 'X'};
/// The game clock and the streams of random numbers
inline constexpr Tag k_ClockTag {'T', 'I', 'M', 'E'};
/// The camera
inline constexpr Tag k_CameraTag {'C', 'A', 'M', 'R'};
/// The scripts' program and its running state, as a script file of the game has them
inline constexpr Tag k_ScriptTag {'L', 'H', 'V', 'M'};
/// The table of the objects that the scripts hold
inline constexpr Tag k_ScriptObjectsTag {'S', 'O', 'B', 'J'};
/// The running state that the systems of the game keep beside the entities, one section for each system by its name
inline constexpr Tag k_SystemsTag {'S', 'Y', 'S', 'T'};
/// The altitudes of the land that the world changed. A saved game of openblack and a world state keep them. A saved game
/// of the game itself does not keep them, so its land comes back from the landscape file.
inline constexpr Tag k_AltitudesTag {'L', 'A', 'L', 'T'};

struct Chunk
{
	Tag tag {};
	std::vector<uint8_t> bytes;

	bool operator==(const Chunk&) const = default;
};

struct WorldFile
{
	uint32_t version {k_WorldVersion};
	/// The loads of the file that started and did not end. A complete save has none. Each load adds one to the file
	/// before it reads the world, and puts it back to none when it has read the world.
	uint32_t attempts {0};
	std::vector<Chunk> chunks;

	bool operator==(const WorldFile&) const = default;
};

[[nodiscard]] std::vector<uint8_t> Write(const WorldFile& file);
/// Reads the file, or nothing when it is not a world file or a block goes past the end
[[nodiscard]] std::optional<WorldFile> ReadWorldFile(std::span<const uint8_t> bytes);
/// The count of the loads that did not end, from the bytes of a file; none when the bytes are not a world file. A file of
/// a version before the count has none.
[[nodiscard]] std::optional<uint32_t> ReadAttempts(std::span<const uint8_t> bytes);
/// Whether a load of a file may start with the count of its loads that did not end
[[nodiscard]] constexpr bool MayLoad(uint32_t attempts)
{
	return attempts < k_MostAttempts;
}
/// The bytes to write at the count's place in a file
[[nodiscard]] std::array<uint8_t, sizeof(uint32_t)> AttemptsBytes(uint32_t attempts);
/// The bytes of the first block with the tag, or none
[[nodiscard]] std::optional<std::span<const uint8_t>> FindChunk(const WorldFile& file, const Tag& tag);

/// The game's block: the land, the turn and what the game keeps of the local player's creature
struct GameChunk
{
	uint32_t land {0};
	uint32_t turn {0};
	/// The land's file, as the game opened it
	std::string landscape;
	/// The leash types that the local player's creature knows
	uint8_t knownLeashes {0};
	/// 1 when the local player's creature is in the temple's Creature Cave
	uint32_t creatureInTemple {0};
	/// The turn of the last autosave and the turn of the last quick save
	uint32_t lastAutosaveTurn {0};
	uint32_t lastQuickSaveTurn {0};
	/// The land's landscape file, which the game opened for the land; empty in a block from before the whole world
	std::string landscapeFile;

	bool operator==(const GameChunk&) const = default;
};

[[nodiscard]] Chunk ToChunk(const GameChunk& game);
[[nodiscard]] std::optional<GameChunk> ReadGameChunk(std::span<const uint8_t> bytes);

/// A creature that the world file keeps: its whole record, its player's leash on it, and the step of its agenda
struct SavedCreature
{
	/// The key of the creature in the stream: an entity of openblack, or a number of a file of the game
	ObjectKey key {k_NoObject};
	CreatureObject creature;
	std::optional<LeashStatusObject> leash;
	/// The step of its agenda that it does, for the plan that the record keeps
	uint32_t agendaStep {0};
	/// Where it faces, which openblack keeps beside the record
	float heading {0.0f};
	/// The number that a file of the game gave the creature when openblack loaded it from that file, else 0
	uint32_t vanillaNumber {0};

	bool operator==(const SavedCreature&) const = default;
};

/// The key of the leash of the creature with the key
[[nodiscard]] constexpr ObjectKey LeashKey(ObjectKey creature)
{
	return creature | (ObjectKey {1} << 33);
}

/// The blocks of the creatures: the stream of their records and the keys of its numbers
[[nodiscard]] std::vector<Chunk> ToChunks(std::span<const SavedCreature> creatures);
/// The creatures of the file, or nothing when the file has no stream of creatures that reads
[[nodiscard]] std::optional<std::vector<SavedCreature>> ReadCreatures(const WorldFile& file);

} // namespace openblack::gamesave
