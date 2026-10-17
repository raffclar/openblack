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

// openblack's own world file of a saved game. The original game writes the world as one stream of objects without
// lengths, which openblack does not read or write (deviation 46). openblack writes a version and then a list of
// blocks. Each block has a name of four letters and a length, so that a reader can skip a block that it does not know.
namespace openblack::gamesave
{

inline constexpr std::array<char, 4> k_WorldMagic {'O', 'B', 'S', 'V'};
inline constexpr uint32_t k_WorldVersion = 1;

using Tag = std::array<char, 4>;
inline constexpr Tag k_GameTag {'G', 'A', 'M', 'E'};
inline constexpr Tag k_CreatureTag {'C', 'R', 'E', 'A'};

struct Chunk
{
	Tag tag {};
	std::vector<uint8_t> bytes;

	bool operator==(const Chunk&) const = default;
};

struct WorldFile
{
	uint32_t version {k_WorldVersion};
	std::vector<Chunk> chunks;

	bool operator==(const WorldFile&) const = default;
};

[[nodiscard]] std::vector<uint8_t> Write(const WorldFile& file);
/// Reads the file, or nothing when it is not a world file or a block goes past the end
[[nodiscard]] std::optional<WorldFile> ReadWorldFile(std::span<const uint8_t> bytes);

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

	bool operator==(const GameChunk&) const = default;
};

[[nodiscard]] Chunk ToChunk(const GameChunk& game);
[[nodiscard]] std::optional<GameChunk> ReadGameChunk(std::span<const uint8_t> bytes);

/// A creature's block: its own reference, where it is, the creature part of its record in the original layout, and its
/// player's leash on it
struct CreatureChunk
{
	/// The reference that other blocks use for this creature
	Ref self {0};
	/// The parts that every object writes and that openblack keeps for the creature
	Point position {};
	float heading {0.0f};
	float life {1.0f};
	CreatureRecord record;
	std::optional<LeashRecord> leash;
	/// The step of its agenda that it does, for the plan that the record keeps
	uint32_t agendaStep {0};

	bool operator==(const CreatureChunk&) const = default;
};

[[nodiscard]] Chunk ToChunk(const CreatureChunk& creature);
[[nodiscard]] std::optional<CreatureChunk> ReadCreatureChunk(std::span<const uint8_t> bytes);

} // namespace openblack::gamesave
