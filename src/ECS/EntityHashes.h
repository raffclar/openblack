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

/// The hash of each entity in a part of the synced world. When the worlds of a network game part, the server and the
/// player's computer hash the parts that differ entity by entity at the same turn. The player's computer first sends a
/// hash for each block of entities, the server asks for the entities of the first blocks that differ, and then names
/// the first entities that differ. This keeps the frames small for a world of many entities.
namespace openblack::synced_state
{

/// The entities in one block, in the order of their numbers
inline constexpr size_t k_EntitiesInBlock = 256;
/// The most blocks of one part that the server asks for
inline constexpr size_t k_MostAskedBlocks = 4;
/// The most parts of one capture
inline constexpr size_t k_MostCapturedParts = 8;
/// The most entities of one part that a difference names
inline constexpr size_t k_MostDifferentEntities = 8;
/// The most component names in the kind of an entity
inline constexpr size_t k_MostKindNames = 4;
/// The most blocks of one part: a part with more entities sends the first blocks only
inline constexpr size_t k_MostBlocks = 1024;

/// The hash of one entity in one part, with what the entity is and where it is
struct EntityHash
{
	/// The number of the entity (its index and its version)
	uint32_t entity {0};
	uint64_t hash {0};
	/// The names of the first components of the entity, except Transform, with "+" between them
	std::string kind;
	/// The position of the entity, when it has a Transform
	std::optional<std::array<float, 3>> position;

	[[nodiscard]] bool operator==(const EntityHash&) const = default;
};

/// The hashes of the entities of one part, in the order of their numbers
struct PartEntities
{
	std::string part;
	std::vector<EntityHash> entities;
};

/// The hash of each block of the entities of a part, at most k_MostBlocks. Each block mixes the number and the hash of
/// its entities.
[[nodiscard]] std::vector<uint64_t> BlocksOf(std::span<const EntityHash> entities);

/// The first blocks that differ, at most k_MostAskedBlocks, in order. A block that only one side has differs.
[[nodiscard]] std::vector<uint32_t> DifferentBlocks(std::span<const uint64_t> own, std::span<const uint64_t> other);

/// The entities of the blocks, in order. A block past the end gives nothing.
[[nodiscard]] std::vector<EntityHash> EntitiesOfBlocks(std::span<const EntityHash> entities, std::span<const uint32_t> blocks);

/// One entity that differs: its hash on the server and on the player's computer. A side that does not have the entity
/// has none.
struct EntityDifference
{
	uint32_t entity {0};
	std::optional<EntityHash> server;
	std::optional<EntityHash> player;
};

/// The first entities that differ, at most k_MostDifferentEntities. Each list is in the order of the numbers.
[[nodiscard]] std::vector<EntityDifference> DifferentEntities(std::span<const EntityHash> server,
                                                              std::span<const EntityHash> player);

/// The differences in words, for the logs and the reports, such as
/// "Transform: entity 1234 (Villager, server at 1.0 2.0 3.0 hash ..., player at ...)"
[[nodiscard]] std::string Describe(std::string_view part, std::span<const EntityDifference> differences);

/// The kind of an entity from the names of its components, in the order that the hash found them
[[nodiscard]] std::string KindOf(std::span<const std::string_view> components);

/// FNV-1a: mixes bytes into a hash. The start is the offset basis.
[[nodiscard]] uint64_t MixHash(uint64_t hash, std::span<const uint8_t> bytes) noexcept;
inline constexpr uint64_t k_EntityHashStart = 14695981039346656037ull;

} // namespace openblack::synced_state
