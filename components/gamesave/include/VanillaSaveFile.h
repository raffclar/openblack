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

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ObjectStream.h"
#include "SavedObjects.h"

// The world file of a saved game as the game writes it (<slot>.sav). openblack does not read the whole world of the
// file yet. It finds the records of the creatures and of the leashes in the file, reads them and writes them again
// in the same layout.
namespace openblack::gamesave
{

/// The text that the game writes after the first word of its world file
inline constexpr std::string_view k_VanillaRevision = "$Revision: 183 $";

/// The start of the game's world file
struct VanillaFraming
{
	/// 1 while the game writes the file, 0 when the write completed. A load adds 1 and refuses a file at 2 or more.
	uint32_t attempts {0};
	std::string revision;
	/// The land's file, as the game opened it
	std::string landscape;
	/// Where the game object starts, and the running sum there
	size_t body {0};
	uint32_t checksum {0};

	bool operator==(const VanillaFraming&) const = default;
};

[[nodiscard]] std::optional<VanillaFraming> ReadVanillaFraming(std::span<const uint8_t> bytes);
/// The start of a file as the game writes it: the first word 0, then the revision and the land's file
[[nodiscard]] std::vector<uint8_t> WriteVanillaFraming(std::string_view landscape);

/// A record of an object that openblack found in the game's world file
template <typename Object>
struct FoundRecord
{
	/// Where the reference to the object starts, before its record
	size_t at {0};
	/// Where the record stops
	size_t end {0};
	/// The number that the file gives the object
	uint32_t number {0};
	/// The objects before the reference and the running sum there, so that a write of the record gives the same bytes
	StreamStart start {};
	Object object {};
	/// The objects that the file has after the record
	uint32_t objectsAfter {0};
};

using FoundCreature = FoundRecord<CreatureObject>;
using FoundLeash = FoundRecord<LeashStatusObject>;

/// The running sum before a reference to a new object, from the sum that the object's record has
[[nodiscard]] std::optional<StreamStart> StartOfRecordAt(std::span<const uint8_t> bytes, size_t at);

/// Finds every creature record in the file. Each record must read to its end with the game's layout, and its spell
/// slots must have the spells in their order.
[[nodiscard]] std::vector<FoundCreature> FindCreatures(std::span<const uint8_t> bytes);
/// Finds every leash record in the file
[[nodiscard]] std::vector<FoundLeash> FindLeashes(std::span<const uint8_t> bytes);

/// Writes the reference and the record as the file had them before the record. Gives nothing when the record holds
/// an object that the stream cannot write in the game's layout.
[[nodiscard]] std::optional<std::vector<uint8_t>> WriteRecordAt(StreamStart start, const ObjectRef& ref);

/// Puts new bytes of a record into the place of the record that the file had. Gives nothing when the new bytes make
/// a different number of new objects, because then each object after them has another number.
[[nodiscard]] std::optional<std::vector<uint8_t>> Replace(std::span<const uint8_t> file, size_t at, size_t end,
                                                          StreamStart start, uint32_t objectsAfter, const ObjectRef& ref);

template <typename Object>
[[nodiscard]] std::optional<std::vector<uint8_t>> Replace(std::span<const uint8_t> file, const FoundRecord<Object>& found,
                                                          const ObjectRef& ref)
{
	return Replace(file, found.at, found.end, found.start, found.objectsAfter, ref);
}

} // namespace openblack::gamesave
