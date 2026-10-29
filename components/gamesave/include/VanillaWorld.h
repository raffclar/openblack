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

#include "ObjectStream.h"
#include "VanillaSaveFile.h"

// The whole world file of a saved game as the game writes it. The reader reads each object of the file with the
// layout that the game uses for the object's type, and makes sure that the running sum of each record agrees with the
// sum that the file keeps. It keeps the values that the game code of openblack uses, by name.
namespace openblack::gamesave
{

/// The types of the objects in the game's world file that openblack uses
enum class VanillaType : uint32_t
{
	Villager = 0x06,
	Abode = 0x07,
	StoragePit = 0x08,
	Town = 0x28,
	Field = 0x32,
	Citadel = 0x35,
	CitadelHeart = 0x3A,
	WorshipSite = 0x3C,
	ScriptHighlight = 0x3F,
	InfluenceRing = 0x49,
	BigForest = 0x4D,
	Forest = 0x4E,
	Tree = 0x4F,
	Creche = 0x50,
	Graveyard = 0x51,
	MobileObject = 0x53,
	MobileStatic = 0x54,
	FishFarm = 0x5B,
	StandardBuildingSite = 0x5C,
	TownCentre = 0x60,
	PlannedAbode = 0x61,
	PlannedTownCentre = 0x62,
	Player = 0x63,
	PlayerInterfaceStatus = 0x65,
	ComputerPlayer = 0x66,
	Creature = 0x69,
	Game = 0x6A,
	Poo = 0x6B,
	Rock = 0x6F,
	DeadTree = 0x70,
	FelledTree = 0x71,
	Bonfire = 0x72,
	Feature = 0x74,
	AnimatedStatic = 0x75,
	SpecialVillager = 0x76,
	Camera = 0x77,
	ScriptMarker = 0x7C,
	ScriptTimer = 0x7D,
	Flock = 0x7E,
	BaseOnly = 0xF6,
	Climate = 0xFC,
	StreetLantern = 0xFE,
};

/// How a named value of a record is kept in the file
enum class VanillaValue : uint8_t
{
	/// The raw bytes of one value
	Bytes,
	/// The number of one object, or 0 for no object
	Reference,
	/// A list of object numbers
	References,
	/// A count, then the values one after the other
	Array,
};

/// A named value of a record
struct VanillaField
{
	std::string_view name;
	VanillaValue kind {VanillaValue::Bytes};
	/// Where the value starts in the file. For an array, where its first value starts.
	size_t at {0};
	/// The size of the value, or of each value of an array
	uint32_t size {0};
	/// The number of values of an array or of a list
	uint32_t count {0};
	/// The object number of a reference, or for a list the index of its first number in the world's numbers
	uint32_t number {0};
};

/// The record of one object in the file
struct VanillaRecord
{
	/// The number that the file gives the object. Each new object takes the next number, from 1.
	uint32_t number {0};
	uint32_t type {0};
	uint32_t extra {0};
	uint32_t destroyed {0};
	uint8_t flags {0};
	/// Where the reference that holds the record starts, and where the record stops
	size_t at {0};
	size_t end {0};
	/// The number of the object whose record holds this record, or 0 for the game object
	uint32_t parent {0};
	std::vector<VanillaField> fields;

	[[nodiscard]] bool Is(VanillaType kind) const { return type == static_cast<uint32_t>(kind); }
};

/// The 8 values of the game's general data, in the order of the file
struct VanillaGameData
{
	/// The seed of the draws that every computer of a game makes the same
	uint32_t randomSeed {0};
	/// The seed of the draws that each computer makes on its own
	uint32_t localSeed {0};
	uint32_t gameTurn {0};
	/// The number of objects that the game made before the save
	uint32_t thingsMade {0};
	uint32_t worldPopulation {0};
	/// All 8 values, also the values that openblack does not use
	std::array<uint32_t, 8> words {};
};

/// The world file read as a whole
class VanillaWorld
{
public:
	VanillaWorld(std::vector<uint8_t> bytes, VanillaFraming framing, std::vector<VanillaRecord> records,
	             std::vector<uint32_t> numbers);

	[[nodiscard]] const VanillaFraming& Framing() const { return _framing; }
	[[nodiscard]] std::span<const uint8_t> Bytes() const { return _bytes; }
	[[nodiscard]] std::span<const VanillaRecord> Records() const { return _records; }
	/// The record of an object number, or nothing for a number that the world does not have
	[[nodiscard]] const VanillaRecord* Record(uint32_t number) const;
	/// The first record. In a whole file, this is the game object.
	[[nodiscard]] const VanillaRecord& Game() const { return _records.front(); }
	[[nodiscard]] std::vector<const VanillaRecord*> OfType(VanillaType type) const;

	/// The first value of the record with the name, or nothing
	[[nodiscard]] const VanillaField* Field(const VanillaRecord& record, std::string_view name) const;
	/// Each value of the record with the name, in the order of the file
	[[nodiscard]] std::vector<const VanillaField*> Fields(const VanillaRecord& record, std::string_view name) const;

	[[nodiscard]] std::optional<uint8_t> Byte(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::optional<uint16_t> Half(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::optional<uint32_t> Word(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::optional<float> Float(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::optional<std::array<float, 3>> Point(const VanillaRecord& record, std::string_view name) const;
	/// The object number of a reference, 0 for no object
	[[nodiscard]] std::optional<uint32_t> Ref(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::span<const uint32_t> Refs(const VanillaRecord& record, std::string_view name) const;
	/// The bytes of a value, or of all values of an array
	[[nodiscard]] std::span<const uint8_t> Raw(const VanillaRecord& record, std::string_view name) const;
	[[nodiscard]] std::span<const uint32_t> Refs(const VanillaField& field) const;

	/// The game's general data from the game object
	[[nodiscard]] std::optional<VanillaGameData> GameData() const;
	/// The time of day that the game shows, in hours from 0 to 24
	[[nodiscard]] std::optional<float> VisualTime() const;

private:
	[[nodiscard]] std::span<const uint8_t> Value(const VanillaRecord& record, std::string_view name, uint32_t size) const;

	std::vector<uint8_t> _bytes;
	VanillaFraming _framing;
	std::vector<VanillaRecord> _records;
	std::vector<uint32_t> _numbers;
};

/// Why a file does not read, and where
struct VanillaWorldError
{
	size_t at {0};
	std::string reason;
};

struct VanillaWorldResult
{
	std::optional<VanillaWorld> world;
	std::optional<VanillaWorldError> error;
};

/// Reads the whole file. Each record must read with the layout of its type and have the running sum that the file
/// keeps, and the read must stop at the end of the file.
[[nodiscard]] VanillaWorldResult ReadVanillaWorld(std::span<const uint8_t> bytes);

/// Reads the record of one new object that starts at the start of the bytes, as a part of a file whose stream had
/// the objects and the running sum of the start there. The world has this record first, then the records that it
/// holds. This lets tests read a record that they cut from a file.
[[nodiscard]] std::optional<VanillaWorld> ReadVanillaRecords(std::span<const uint8_t> bytes, StreamStart start,
                                                             std::string* error = nullptr);

/// True when openblack knows the layout of the type
[[nodiscard]] bool KnowsVanillaType(uint32_t type);

/// True when the file can keep more values for a villager in the state, after the values of the villager. The state of
/// a disciple that has nothing to do is one of these states, because a disciple that trades keeps more values.
[[nodiscard]] bool VillagerStateKeepsValues(uint8_t state);

} // namespace openblack::gamesave
