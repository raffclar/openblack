/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaWorld.h"

#include <cstring>

#include <algorithm>
#include <bit>
#include <ranges>
#include <utility>

namespace openblack::gamesave
{

namespace
{

/// The deepest that records go in the saves of the tests is 25 records
constexpr size_t k_MaxDepth = 256;
/// A count above this is not a count that the game writes
constexpr uint32_t k_MaxCount = 0x100000;

/// Reads the values of the stream, one at a time, as the game wrote them. After the first value that does not read,
/// each read gives 0 and does not move.
class WorldReader
{
public:
	WorldReader(std::span<const uint8_t> bytes, size_t position, StreamStart start)
	    : _bytes(bytes)
	    , _position(position)
	    , _sum(start.checksum)
	    , _objects(start.objects)
	{
	}

	[[nodiscard]] bool Failed() const { return _error.has_value(); }
	[[nodiscard]] size_t Position() const { return _position; }
	[[nodiscard]] const std::optional<VanillaWorldError>& Error() const { return _error; }

	void Fail(std::string reason)
	{
		if (!_error.has_value())
		{
			_error = VanillaWorldError {.at = _position, .reason = std::move(reason)};
		}
	}

	std::span<const uint8_t> Bytes(uint32_t size, std::string_view name = {})
	{
		const auto at = _position;
		const auto value = Take(size);
		if (!name.empty() && !value.empty())
		{
			AddField({.name = name, .kind = VanillaValue::Bytes, .at = at, .size = size, .count = 1});
		}
		return value;
	}

	uint8_t Byte(std::string_view name = {})
	{
		const auto value = Bytes(1, name);
		return value.empty() ? uint8_t {0} : value[0];
	}

	uint16_t Half(std::string_view name = {})
	{
		const auto value = Bytes(2, name);
		return value.empty() ? uint16_t {0} : static_cast<uint16_t>(value[0] | (value[1] << 8));
	}

	uint32_t Word(std::string_view name = {})
	{
		const auto value = Bytes(4, name);
		if (value.empty())
		{
			return 0;
		}
		return static_cast<uint32_t>(value[0]) | (static_cast<uint32_t>(value[1]) << 8) |
		       (static_cast<uint32_t>(value[2]) << 16) | (static_cast<uint32_t>(value[3]) << 24);
	}

	float Float(std::string_view name = {}) { return std::bit_cast<float>(Word(name)); }

	/// Two whole numbers and a number with a fraction, written as one value
	void Coords(std::string_view name = {}) { Bytes(12, name); }
	/// Three numbers with a fraction, written as one value
	void Point(std::string_view name = {}) { Bytes(12, name); }

	/// A shared resource that the file keeps as two words
	void Persistent()
	{
		Word();
		Word();
	}

	/// A count, then each value on its own. Gives the count.
	uint32_t Array(uint32_t size, std::string_view name = {})
	{
		const auto count = Count();
		const auto at = _position;
		for (uint32_t i = 0; i < count && !Failed(); ++i)
		{
			Take(size);
		}
		if (!name.empty() && !Failed())
		{
			AddField({.name = name, .kind = VanillaValue::Array, .at = at, .size = size, .count = count});
		}
		return count;
	}

	/// A reference to an object. A new object has its whole record after the number.
	uint32_t Ref(std::string_view name = {})
	{
		const auto at = _position;
		const auto number = Word();
		if (Failed() || number == 0)
		{
			AddReference(name, at, 0);
			return 0;
		}
		if (number <= _objects)
		{
			AddReference(name, at, number);
			return number;
		}
		if (number != _objects + 1)
		{
			Fail("The reference " + std::to_string(number) + " is not an object of the file");
			return 0;
		}
		const auto read = Record(at);
		AddReference(name, at, read);
		return read;
	}

	/// A count, then a reference for each object
	void Refs(std::string_view name = {}) { References(Count(), name); }

	/// References with a count that the layout gives
	void References(uint32_t count, std::string_view name)
	{
		const auto at = _position;
		std::vector<uint32_t> numbers;
		numbers.reserve(count);
		for (uint32_t i = 0; i < count && !Failed(); ++i)
		{
			numbers.push_back(Ref());
		}
		if (!name.empty() && !Failed())
		{
			AddField({.name = name,
			          .kind = VanillaValue::References,
			          .at = at,
			          .size = 4,
			          .count = count,
			          .number = static_cast<uint32_t>(_numbers.size())});
			_numbers.insert(_numbers.end(), numbers.begin(), numbers.end());
		}
	}

	/// The flags of the record that the reader reads now
	[[nodiscard]] uint8_t Flags() const { return _open.empty() ? uint8_t {0} : _records[_open.back()].flags; }

	/// A record: the type, a word, the running sum, a word and the flags, then the values of the type
	uint32_t Record(size_t at);

	[[nodiscard]] std::vector<VanillaRecord> TakeRecords() { return std::move(_records); }
	[[nodiscard]] std::vector<uint32_t> TakeNumbers() { return std::move(_numbers); }

private:
	std::span<const uint8_t> Take(uint32_t size)
	{
		if (Failed())
		{
			return {};
		}
		if (size == 0 || size > _bytes.size() - _position)
		{
			Fail("The file stops in a value of " + std::to_string(size) + " bytes");
			return {};
		}
		const auto value = _bytes.subspan(_position, size);
		// The game adds the first byte and the size of each value that it writes
		_sum += value[0] + size;
		_position += size;
		return value;
	}

	uint32_t Count()
	{
		const auto count = Word();
		if (count > k_MaxCount)
		{
			Fail("The count " + std::to_string(count) + " is too large");
			return 0;
		}
		return count;
	}

	void AddField(VanillaField field)
	{
		if (!_open.empty())
		{
			_records[_open.back()].fields.push_back(field);
		}
	}

	void AddReference(std::string_view name, size_t at, uint32_t number)
	{
		if (!name.empty() && !Failed())
		{
			AddField({.name = name, .kind = VanillaValue::Reference, .at = at, .size = 4, .count = 1, .number = number});
		}
	}

	std::span<const uint8_t> _bytes;
	size_t _position;
	uint32_t _sum;
	uint32_t _objects;
	std::optional<VanillaWorldError> _error;
	std::vector<VanillaRecord> _records;
	std::vector<uint32_t> _numbers;
	/// The index of each record that the reader has not read to its end, the deepest last
	std::vector<size_t> _open;
};

// The types whose layout changes with the values, or that have parts that more than one type uses

void ReadPositioned(WorldReader& r);
void ReadObject(WorldReader& r);
void ReadFixed(WorldReader& r);
void ReadMultiMapFixed(WorldReader& r);
void ReadMobile(WorldReader& r);
void ReadWallHug(WorldReader& r);
void ReadContainer(WorldReader& r);
void ReadGame(WorldReader& r);
void ReadPlayer(WorldReader& r);
void ReadComputerPlayer(WorldReader& r);
void ReadCitadel(WorldReader& r);
void ReadTownDesire(WorldReader& r);
void ReadTown(WorldReader& r);
void ReadFragMesh(WorldReader& r);
void ReadAbode(WorldReader& r);
std::array<uint8_t, 2> ReadLiving(WorldReader& r);
void ReadVillager(WorldReader& r);
void ReadAnimal(WorldReader& r);
void ReadInterfaceStatus(WorldReader& r);
void ReadSpellTargets(WorldReader& r);
void ReadAtomCollection(WorldReader& r);
void ReadLandscapeVortex(WorldReader& r);
void ReadClimate(WorldReader& r);
void ReadCamera(WorldReader& r);
void ReadCreatureStep(WorldReader& r);
void ReadCreature(WorldReader& r);
void ReadVortexSave(WorldReader& r);

struct ClassReader
{
	uint32_t type;
	void (*read)(WorldReader&);
};

#include "VanillaWorldClasses.inl"

static_assert(std::ranges::is_sorted(k_Readers, {}, &ClassReader::type));

const ClassReader* ReaderOf(uint32_t type)
{
	const auto found = std::ranges::lower_bound(k_Readers, type, {}, &ClassReader::type);
	return found != k_Readers.end() && found->type == type ? &*found : nullptr;
}

uint32_t WorldReader::Record(size_t at)
{
	if (_open.size() >= k_MaxDepth)
	{
		Fail("The records go too deep");
		return 0;
	}
	const auto number = ++_objects;
	const auto index = _records.size();
	_records.push_back({.number = number, .at = at, .parent = _open.empty() ? 0 : _records[_open.back()].number, .fields = {}});
	const auto type = Word();
	const auto extra = Word();
	// The record keeps the running sum after its type and its word
	const auto expected = _sum;
	const auto stored = Word();
	if (Failed())
	{
		return 0;
	}
	if (stored != expected)
	{
		Fail("The record of object " + std::to_string(number) + " keeps the sum " + std::to_string(stored) + ", not " +
		     std::to_string(expected));
		return 0;
	}
	const auto destroyed = Word();
	const auto flags = Byte();
	auto& record = _records[index];
	record.type = type;
	record.extra = extra;
	record.destroyed = destroyed;
	record.flags = flags;
	const auto* reader = ReaderOf(type);
	if (reader == nullptr)
	{
		Fail("openblack does not know the layout of the type " + std::to_string(type));
		return 0;
	}
	_open.push_back(index);
	reader->read(*this);
	_open.pop_back();
	_records[index].end = _position;
	return number;
}

/// The position of a thing. Gives the flags of the position.
uint16_t ReadPosition(WorldReader& r)
{
	r.Coords("position");
	return r.Half("positionFlags");
}

void ReadPositioned(WorldReader& r)
{
	ReadPosition(r);
}

void ReadObject(WorldReader& r)
{
	const auto positionFlags = ReadPosition(r);
	r.Word("info");
	r.Coords("coords");
	r.Word("created");
	r.Float("life");
	r.Float("scale");
	r.Float("heading");
	r.Ref("fire");
	// An object that moves with the physics keeps its motion, but not when its record has the flag of no physics
	if ((positionFlags & 0x40) != 0 && (r.Flags() & 0x10) == 0)
	{
		r.Bytes(48);
		r.Point();
		r.Point();
	}
}

void ReadFixed(WorldReader& r)
{
	ReadObject(r);
	r.Ref("townArtifact");
}

void ReadMultiMapFixed(WorldReader& r)
{
	ReadFixed(r);
	r.Byte("fixedFlags");
	r.Float("built");
	r.Ref("footpathLink");
	r.Ref("buildingSite");
}

void ReadMobile(WorldReader& r)
{
	ReadObject(r);
	r.Half();
}

void ReadWallHug(WorldReader& r)
{
	ReadMobile(r);
	r.Half("turnsToStateChange");
	r.Half("speed");
	r.Half("angle");
	const auto state = r.Byte("moveState");
	// Some states of the move keep the values of their path round an obstacle
	if (state >= 0x0C && state <= 0x12)
	{
		r.Byte("hugTurnsToObject");
		r.Word("hugEntryDistance");
		r.Ref("hugObject");
		r.Word("hugIndex");
		r.Coords("hugCoords");
	}
	else if (state == 0x0B)
	{
		r.Byte("turnsUntilStepRebuild");
	}
	r.Ref("footpath");
	r.Coords("goal");
	r.Ref("target");
	r.Bytes(12, "step");
}

void ReadContainer(WorldReader& r)
{
	ReadPositioned(r);
	r.Word("info");
	r.Ref("owner");
}

void ReadHelp(WorldReader& r)
{
	for (int i = 0; i < 4; ++i)
	{
		r.Array(4);
	}
	r.Word();
	r.Word();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Half();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Array(4);
	r.Word();
	r.Array(4);
	r.Word();
	r.Word();
	r.Word();
	r.Half();
	r.Word();
	r.Word();
	r.Half();
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	r.Word();
	r.Word();
	r.Word();
	r.Float();
	for (int i = 0; i < 9; ++i)
	{
		r.Word();
	}
	r.Array(16);
}

void ReadScriptThings(WorldReader& r)
{
	// The objects that the scripts use, by their place in the table: the object, whether a script made it, and the
	// number of script references that hold it
	for (int i = 0; i < 511 && !r.Failed(); ++i)
	{
		r.Ref("scriptObject");
		r.Word("scriptObjectMade");
		r.Byte("scriptObjectCount");
	}
	// The place where the next search for a free place starts, then other values of the scripts
	r.Word("scriptNextPlace");
	for (const uint32_t size : {4, 4, 4, 4, 12, 12, 4, 4, 4, 4})
	{
		r.Bytes(size);
	}
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	r.Ref();
	r.Ref();
	for (int i = 0; i < 7; ++i)
	{
		r.Word();
	}
}

void ReadCameraExclusions(WorldReader& r)
{
	for (int i = 0; i < 7; ++i)
	{
		r.Word();
	}
	const auto points = r.Word();
	for (uint32_t i = 0; i < points && !r.Failed(); ++i)
	{
		r.Point();
	}
	const auto zones = r.Word();
	r.Word();
	for (uint32_t i = 0; i < zones && !r.Failed(); ++i)
	{
		r.Array(1);
	}
}

void ReadGameTail(WorldReader& r)
{
	for (int i = 0; i < 5; ++i)
	{
		r.Word();
	}
	r.Float("visualTime");
	// The cycle of day and night: the seconds of a day, the part of it that changes, the part that is night, and the
	// rate that the clock runs at
	r.Float("dayDuration");
	r.Float("dayChange");
	r.Float("dayNight");
	r.Float("visualTimeScale");
	ReadHelp(r);
	ReadScriptThings(r);
	r.Point("startCamera");
	r.Ref("camera");
	r.Ref("climate");
	ReadCameraExclusions(r);
	for (int i = 0; i < 3; ++i)
	{
		r.Word();
	}
	r.Word("creatureInTemple");
	r.Array(4, "snow");
	r.Array(4);
	r.Float();
	r.Word();
	r.Bytes(8, "weather");
	r.Word();
	r.Byte();
	for (int i = 0; i < 3; ++i)
	{
		r.Word();
	}
	r.Word("lastAutosave");
	r.Word();
	// The bookmarks of the camera
	for (int i = 0; i < 11 && !r.Failed(); ++i)
	{
		r.Byte();
		r.Point();
		r.Ref();
		r.Point();
		r.Point();
	}
	for (int i = 0; i < 3; ++i)
	{
		r.Word();
	}
	r.Bytes(32);
	ReadHand(r);
	r.Refs("music");
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
	r.Word("rooms");
	for (int i = 0; i < 5; ++i)
	{
		r.Word();
	}
	for (int i = 0; i < 5; ++i)
	{
		r.Array(4);
		r.Word();
	}
	const auto challenges = r.Word();
	for (uint32_t i = 0; i < challenges && !r.Failed(); ++i)
	{
		ReadChallenge(r);
	}
	r.Bytes(12);
}

void ReadGame(WorldReader& r)
{
	r.Array(1, "revision");
	r.Word("gameFlags");
	r.Word("land");
	r.References(8, "players");
	for (int i = 0; i < 8; ++i)
	{
		r.Word("data");
	}
	r.Byte("playerIndex");
	r.Byte();
	r.Byte();
	r.Word();
	// The lists of the game's objects of each kind
	for (int i = 0; i < 44 && !r.Failed(); ++i)
	{
		r.Refs("lists");
	}
	ReadGameTail(r);
}

void ReadPlayer(WorldReader& r)
{
	r.Byte("number");
	r.Bytes(16);
	r.Word("info");
	r.Array(4);
	r.Array(4);
	r.Word("type");
	r.Array(2, "name");
	for (int i = 0; i < 6; ++i)
	{
		r.Word();
	}
	r.Ref("computer");
	r.Word();
	r.Bytes(20);
	r.Array(4);
	r.Array(1);
	r.Ref("stats");
	r.Ref("citadel");
	r.Ref("creature");
	r.Refs("towns");
	r.Refs("teleports");
	r.Float();
	r.Float();
	r.Array(4);
	r.Array(4);
	r.Refs("interfaces");
	r.Word();
	r.Word();
}

void ReadComputerPlayer(WorldReader& r)
{
	ReadPositioned(r);
	r.Ref("player");
	r.Ref("actionState");
	r.Refs();
	r.Refs();
	r.Refs();
	for (int i = 0; i < 5; ++i)
	{
		r.Word();
	}
	r.Array(4);
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	r.Byte();
	r.Refs();
	// The decision tree of the computer player. Each save of the tests has 396 words here.
	for (int i = 0; i < 396; ++i)
	{
		r.Word();
	}
}

void ReadCitadel(WorldReader& r)
{
	ReadContainer(r);
	r.Ref("heart");
	r.Refs("rooms");
	for (int i = 0; i < 4 && !r.Failed(); ++i)
	{
		r.Refs("parts");
	}
	r.Word();
	r.Word();
	r.Word();
}

void ReadTownDesire(WorldReader& r)
{
	// Two lists that openblack does not use, the boosts of a cheat and of the scripts, and the desires
	for (const std::string_view name : {"", "", "desireCheat", "desireBoost", "desire"})
	{
		r.Array(4, name);
	}
	r.Word();
	r.Ref();
	r.Word("desirePopulation");
	// The raw desires, then three lists that the town makes again each turn
	for (const std::string_view name : {"desireRaw", "", "", ""})
	{
		r.Array(4, name);
	}
	r.Array(12, "desireSorted");
	r.Array(12, "desireSortedRaw");
	for (const std::string_view name :
	     {"desireTurnsOverAlignment", "desireDoingAtStart", "desireDoingCountAtStart", "desireDoing", "desireDoingCount"})
	{
		r.Array(4, name);
	}
}

/// A grid: the number of rows, the number of columns, then each value on its own
void ReadGrid(WorldReader& r, uint32_t size)
{
	const auto rows = r.Word();
	const auto columns = r.Word();
	if (columns != 0 && rows > k_MaxCount / columns)
	{
		r.Fail("The grid is too large");
		return;
	}
	for (uint32_t i = 0; i < rows * columns && !r.Failed(); ++i)
	{
		r.Bytes(size);
	}
}

void ReadTown(WorldReader& r)
{
	ReadContainer(r);
	r.Ref("storagePit");
	ReadTownDesire(r);
	r.Bytes(24);
	r.Word("id");
	r.Word("tribe");
	r.Byte("player");
	for (const std::string_view name : {"worshipShare", "worshipCount", "influence", "onWayToWorshipCount", "turnsToWorship",
	                                    "promiscuity", "beliefInNeutral", "raining", "buildingRequested", "desireDirty",
	                                    "desireDirtyTimer", "cannotBuildWorshipSite", "uninhabitable"})
	{
		r.Word(name);
	}
	r.Refs("pots");
	r.Refs("forests");
	r.Bytes(280, "stats");
	r.Bytes(12, "areaMin");
	r.Bytes(12, "areaMax");
	r.Ref("creche");
	r.Ref("graveyard");
	for (const std::string_view name : {"workshops", "abodes", "homeless", "extraVillagers", "spellIcons", "fields",
	                                    "fishFarms", "buildingSites", "artifacts"})
	{
		r.Refs(name);
	}
	r.Bytes(464, "belief");
	r.Refs("villagers");
	r.Ref("nearestTown");
	r.Refs("playthings");
	r.Refs("animals");
	r.Ref("worshipSite");
	r.Ref("townCentre");
	r.Refs("planned");
	r.Bytes(1024);
	r.Refs("onWayToWorship");
	r.Bytes(168);
	r.Ref("footballPitch");
	r.Ref("lastAttacker");
	for (int i = 0; i < 6; ++i)
	{
		r.Word();
	}
	ReadGrid(r, 4);
	r.Refs("flocks");
	r.Refs("desireFlags");
	r.Refs("missionaries");
	r.Word("balanceBeliefScale");
	r.Word("emptyTownTimer");
}

void ReadFragMesh(WorldReader& r)
{
	// The broken mesh of a building: its parts, each with faces of 88 bytes
	const auto parts = r.Word();
	r.Word();
	r.Word();
	r.Word();
	for (uint32_t part = 0; part < parts && !r.Failed(); ++part)
	{
		r.Word();
		r.Word();
		r.Word();
		const auto faces = r.Word();
		for (uint32_t face = 0; face < faces && !r.Failed(); ++face)
		{
			r.Bytes(88);
		}
	}
}

void ReadAbode(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Byte("abodeFlags");
	r.Bytes(12);
	r.Word();
	r.Ref("town");
	r.Refs("villagers");
	r.Byte("adults");
	r.Byte("present");
	r.Byte("children");
	r.Byte("index");
	r.Array(4, "resources");
	if (r.Word() != 0)
	{
		ReadFragMesh(r);
	}
}

std::array<uint8_t, 2> ReadLiving(WorldReader& r)
{
	ReadWallHug(r);
	r.Ref("footpathLink");
	r.Ref("footpathNode");
	// The three states, a spare byte and the turns since the last change of state
	const auto action = r.Bytes(6, "action");
	r.Ref("reaction");
	r.Word();
	r.Word("birthTurn");
	r.Byte();
	r.Ref("dataPath");
	r.Ref("scriptReminder");
	r.Half("status");
	r.Ref("flock");
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
	r.Byte();
	r.Ref("danceGroup");
	r.Byte();
	const auto pairs = r.Word();
	for (uint32_t i = 0; i < pairs && !r.Failed(); ++i)
	{
		r.Word();
		r.Word();
	}
	// The state and the next state of the living thing
	return action.size() >= 2 ? std::array {action[0], action[1]} : std::array<uint8_t, 2> {};
}

/// The values that a villager keeps for one of its states. A size of 0 is a reference.
struct StateLayout
{
	std::array<uint8_t, 4> values;
	uint8_t count;
};

constexpr std::array k_StateLayouts = {
    StateLayout {.values = {}, .count = 0},      StateLayout {.values = {8, 4, 4, 4}, .count = 4},
    StateLayout {.values = {4, 4}, .count = 2},  StateLayout {.values = {1}, .count = 1},
    StateLayout {.values = {4}, .count = 1},     StateLayout {.values = {0, 0}, .count = 2},
    StateLayout {.values = {0, 4}, .count = 2},  StateLayout {.values = {0}, .count = 1},
    StateLayout {.values = {12, 0}, .count = 2}, StateLayout {.values = {1, 1, 1, 0}, .count = 4},
    StateLayout {.values = {2, 2}, .count = 2},  StateLayout {.values = {8}, .count = 1},
    StateLayout {.values = {8, 4}, .count = 2},  StateLayout {.values = {8, 0}, .count = 2},
    StateLayout {.values = {4, 0}, .count = 2},  StateLayout {.values = {0, 8}, .count = 2},
    StateLayout {.values = {4, 8}, .count = 2},
};

/// The state of a disciple that has nothing to do. Only a trading disciple keeps more values.
constexpr uint8_t k_DiscipleLayout = 255;
constexpr uint8_t k_TradingDisciple = 9;

/// The layout of each villager state
constexpr std::array<uint8_t, 255> k_VillagerStateLayouts = {
    0,  0,  0,  0,  1,  2, 0, 0, 0,  2, 0, 0, 0,  3,  3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 4,  0,  0,  0,  5,  5,   0,  0,
    0,  0,  0,  0,  0,  0, 0, 6, 6,  6, 0, 0, 0,  0,  0, 0, 0, 0, 7, 6, 0, 0, 6, 7, 7,  0,  0,  0,  4,  0,   0,  0,
    0,  0,  0,  8,  8,  8, 0, 9, 9,  9, 9, 9, 9,  9,  9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0,  0,  2,  10, 10, 10,  10, 10,
    10, 10, 10, 10, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,   0,  0,
    0,  0,  0,  0,  2,  2, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0,  2,  2,  0,  0,  0,   0,  0,
    0,  0,  0,  0,  0,  0, 0, 0, 11, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,  12, 0,  0,  6,  6,   0,  0,
    0,  2,  0,  0,  0,  0, 0, 1, 1,  0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 13, 13, 13, 13, 13, 255, 9,  14,
    14, 15, 0,  0,  10, 0, 2, 0, 0,  7, 0, 0, 16, 16, 0, 0, 0, 4, 0, 0, 1, 0, 0, 0, 0,  0,  0,  0,  0,  2,   0,
};

void ReadVillager(WorldReader& r)
{
	const auto states = ReadLiving(r);
	r.Half("villagerFlags");
	r.Float("food");
	r.Word("lastCheckTurn");
	r.Byte("foodSpeedUp");
	r.Byte();
	const auto discipleType = r.Byte("discipleType");
	r.Half("foodHeld");
	r.Half("woodHeld");
	r.Half("pregnancy");
	r.Ref("home");
	r.Ref("town");
	r.Ref("mother");
	r.Word();
	r.Word();
	// The values of the top state, then the values of the final state
	constexpr std::array<std::string_view, 2> k_ValueNames = {"topStateValue", "finalStateValue"};
	for (size_t index = 0; index < states.size(); ++index)
	{
		const auto state = states.at(index);
		const auto name = k_ValueNames.at(index);
		if (state >= k_VillagerStateLayouts.size())
		{
			r.Fail("The villager state " + std::to_string(state) + " is not a state of the game");
			return;
		}
		const auto layout = k_VillagerStateLayouts.at(state);
		if (layout == k_DiscipleLayout)
		{
			if (discipleType == k_TradingDisciple)
			{
				r.Word(name);
				r.Ref(name);
			}
			continue;
		}
		const auto& values = k_StateLayouts.at(layout);
		for (const auto size : std::span(values.values).first(values.count))
		{
			if (size == 0)
			{
				r.Ref(name);
			}
			else
			{
				r.Bytes(size, name);
			}
		}
	}
}

void ReadAnimal(WorldReader& r)
{
	// No state of an animal keeps more values
	ReadLiving(r);
	r.Ref();
	r.Half();
	r.Half();
	r.Half();
	r.Half();
	r.Bytes(8);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Bytes(48);
}

void ReadInterfaceStatus(WorldReader& r)
{
	ReadPositioned(r);
	r.Word();
	r.Byte();
	r.Word();
	r.Bytes(48);
	// The hand keeps its values in this record: a byte and the objects that it holds
	r.Byte();
	r.Refs("handHeld");
	r.Byte();
	r.Bytes(8);
	r.Word();
	r.Word();
	for (int i = 0; i < 5; ++i)
	{
		r.Bytes(12);
	}
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Ref("interface");
	r.Ref("influence");
	r.Word();
	r.Word();
}

void ReadSpellTargets(WorldReader& r)
{
	const auto objects = r.Word();
	for (uint32_t i = 0; i < objects && !r.Failed(); ++i)
	{
		r.Word();
		r.Ref();
	}
	const auto points = r.Word();
	for (uint32_t i = 0; i < points && !r.Failed(); ++i)
	{
		r.Bytes(12);
	}
	r.Word();
}

void ReadAtomCollection(WorldReader& r)
{
	const auto resources = r.Word();
	for (uint32_t i = 0; i < resources && !r.Failed(); ++i)
	{
		r.Persistent();
	}
	r.Refs("atoms");
	r.Word();
	r.Ref();
	r.Word();
	r.Byte();
	r.Ref();
	r.Refs("children");
	r.Ref();
	r.Word();
	r.Byte();
}

void ReadLandscapeVortex(WorldReader& r)
{
	ReadMobileStatic(r);
	r.Bytes(12);
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	const auto count = r.Word();
	if (count > 4096)
	{
		r.Fail("The vortex has too many bytes");
		return;
	}
	for (uint32_t i = 0; i < count && !r.Failed(); ++i)
	{
		r.Byte();
	}
}

void ReadClimate(WorldReader& r)
{
	r.Bytes(12);
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	r.Word("info");
	for (int i = 0; i < 3; ++i)
	{
		r.Word();
	}
	r.Bytes(16);
	r.Bytes(8);
	r.Bytes(12);
	r.Word();
	r.Byte();
	// The weather systems
	const auto systems = r.Word();
	for (uint32_t i = 0; i < systems && !r.Failed(); ++i)
	{
		r.Bytes(12);
		r.Bytes(80);
		r.Word();
		r.Word();
		r.Bytes(12);
	}
	for (int i = 0; i < 7; ++i)
	{
		r.Word();
	}
	r.Byte();
}

void ReadCamera(WorldReader& r)
{
	ReadPositioned(r);
	r.Bytes(12);
	for (int i = 0; i < 4; ++i)
	{
		r.Word();
	}
	r.Bytes(8);
	r.Bytes(144);
	r.Bytes(144);
	r.Bytes(48);
	r.Byte();
	// The kind of the camera's mode, then the values of the mode
	const auto mode = r.Word("mode");
	switch (mode)
	{
	case 0:
		break;
	case 0x46:
		for (int i = 0; i < 6; ++i)
		{
			r.Word();
		}
		r.Bytes(12);
		r.Bytes(12);
		r.Ref();
		r.Word();
		break;
	case 0x48:
		for (int i = 0; i < 4; ++i)
		{
			r.Word();
		}
		r.Ref();
		break;
	case 0x65:
		for (int i = 0; i < 4; ++i)
		{
			r.Word();
		}
		break;
	default:
		r.Fail("The camera mode " + std::to_string(mode) + " is not a mode that openblack knows");
		break;
	}
}

void ReadCreatureStep(WorldReader& r)
{
	r.Word();
	r.Ref();
	r.Word();
	r.Point();
	r.Float();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

void ReadCreature(WorldReader& r)
{
	ReadLiving(r);
	r.Word("version");
	r.Word("savedInTemple");
	r.Byte("index");
	r.Byte("owner");
	// The spell slots
	for (int i = 0; i < 16 && !r.Failed(); ++i)
	{
		r.Word();
		r.Word();
		r.Word();
		r.Float();
		r.Word();
		r.Ref();
	}
	const auto queued = r.Word();
	for (uint32_t i = 0; i < queued && !r.Failed(); ++i)
	{
		r.Word();
		r.Float();
		r.Ref();
	}
	r.Word();
	r.Float();
	r.Float();
	r.Byte();
	r.Byte();
	for (int i = 0; i < 4; ++i)
	{
		r.Float();
	}
	r.Word();
	for (int i = 0; i < 5; ++i)
	{
		r.Ref();
	}
	r.Point();
	r.Ref();
	r.Ref();
	r.Byte();
	r.Ref();
	r.Coords("home");
	r.Float();
	r.Word();
	r.Refs("partners");
	r.Ref("dance");
	r.Word();
	for (int i = 0; i < 4; ++i)
	{
		r.Float();
	}
	r.Word();
	r.Float();
	// The plan that the creature keeps, when it keeps one
	if (r.Word() != 0)
	{
		r.Word();
		r.Word();
		for (int i = 0; i < 3; ++i)
		{
			r.Ref();
		}
		r.Word();
		const auto current = r.Word();
		r.Word();
		const auto count = r.Word();
		r.Word();
		r.Word();
		r.Word();
		for (uint32_t step = current; step < count && !r.Failed(); ++step)
		{
			ReadCreatureStep(r);
		}
		r.Ref();
		r.Ref();
	}
	for (int i = 0; i < 8; ++i)
	{
		r.Float();
	}
}

void ReadVortexSave(WorldReader& r)
{
	r.Byte();
	r.Word();
	r.Array(1);
	r.Array(1, "file");
}

} // namespace

VanillaWorld::VanillaWorld(std::vector<uint8_t> bytes, VanillaFraming framing, std::vector<VanillaRecord> records,
                           std::vector<uint32_t> numbers)
    : _bytes(std::move(bytes))
    , _framing(std::move(framing))
    , _records(std::move(records))
    , _numbers(std::move(numbers))
{
}

const VanillaRecord* VanillaWorld::Record(uint32_t number) const
{
	// The records have the numbers in order, from the number of the first record
	if (_records.empty() || number < _records.front().number || number - _records.front().number >= _records.size())
	{
		return nullptr;
	}
	return &_records[number - _records.front().number];
}

std::vector<const VanillaRecord*> VanillaWorld::OfType(VanillaType type) const
{
	std::vector<const VanillaRecord*> found;
	for (const auto& record : _records)
	{
		if (record.Is(type))
		{
			found.push_back(&record);
		}
	}
	return found;
}

const VanillaField* VanillaWorld::Field(const VanillaRecord& record, std::string_view name) const
{
	const auto found = std::ranges::find(record.fields, name, &VanillaField::name);
	return found != record.fields.end() ? &*found : nullptr;
}

std::vector<const VanillaField*> VanillaWorld::Fields(const VanillaRecord& record, std::string_view name) const
{
	std::vector<const VanillaField*> found;
	for (const auto& field : record.fields)
	{
		if (field.name == name)
		{
			found.push_back(&field);
		}
	}
	return found;
}

std::span<const uint8_t> VanillaWorld::Value(const VanillaRecord& record, std::string_view name, uint32_t size) const
{
	const auto* field = Field(record, name);
	if (field == nullptr || field->kind != VanillaValue::Bytes || field->size != size)
	{
		return {};
	}
	return std::span(_bytes).subspan(field->at, size);
}

namespace
{

uint32_t WordAt(std::span<const uint8_t> bytes)
{
	return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) | (static_cast<uint32_t>(bytes[2]) << 16) |
	       (static_cast<uint32_t>(bytes[3]) << 24);
}

} // namespace

std::optional<uint8_t> VanillaWorld::Byte(const VanillaRecord& record, std::string_view name) const
{
	const auto value = Value(record, name, 1);
	return value.empty() ? std::nullopt : std::optional(value[0]);
}

std::optional<uint16_t> VanillaWorld::Half(const VanillaRecord& record, std::string_view name) const
{
	const auto value = Value(record, name, 2);
	return value.empty() ? std::nullopt : std::optional(static_cast<uint16_t>(value[0] | (value[1] << 8)));
}

std::optional<uint32_t> VanillaWorld::Word(const VanillaRecord& record, std::string_view name) const
{
	const auto value = Value(record, name, 4);
	return value.empty() ? std::nullopt : std::optional(WordAt(value));
}

std::optional<float> VanillaWorld::Float(const VanillaRecord& record, std::string_view name) const
{
	const auto value = Word(record, name);
	return value.has_value() ? std::optional(std::bit_cast<float>(*value)) : std::nullopt;
}

std::optional<std::array<float, 3>> VanillaWorld::Point(const VanillaRecord& record, std::string_view name) const
{
	const auto value = Value(record, name, 12);
	if (value.empty())
	{
		return std::nullopt;
	}
	return std::array {std::bit_cast<float>(WordAt(value.subspan(0, 4))), std::bit_cast<float>(WordAt(value.subspan(4, 4))),
	                   std::bit_cast<float>(WordAt(value.subspan(8, 4)))};
}

std::optional<uint32_t> VanillaWorld::Ref(const VanillaRecord& record, std::string_view name) const
{
	const auto* field = Field(record, name);
	if (field == nullptr || field->kind != VanillaValue::Reference)
	{
		return std::nullopt;
	}
	return field->number;
}

std::span<const uint32_t> VanillaWorld::Refs(const VanillaField& field) const
{
	if (field.kind != VanillaValue::References)
	{
		return {};
	}
	return std::span(_numbers).subspan(field.number, field.count);
}

std::span<const uint32_t> VanillaWorld::Refs(const VanillaRecord& record, std::string_view name) const
{
	const auto* field = Field(record, name);
	return field != nullptr ? Refs(*field) : std::span<const uint32_t> {};
}

std::span<const uint8_t> VanillaWorld::Raw(const VanillaRecord& record, std::string_view name) const
{
	const auto* field = Field(record, name);
	if (field == nullptr || (field->kind != VanillaValue::Bytes && field->kind != VanillaValue::Array))
	{
		return {};
	}
	const auto count = field->kind == VanillaValue::Array ? field->count : 1;
	return std::span(_bytes).subspan(field->at, static_cast<size_t>(field->size) * count);
}

std::optional<VanillaGameData> VanillaWorld::GameData() const
{
	const auto fields = Fields(Game(), "data");
	if (fields.size() != 8)
	{
		return std::nullopt;
	}
	VanillaGameData data;
	for (size_t i = 0; i < fields.size(); ++i)
	{
		data.words.at(i) = WordAt(std::span(_bytes).subspan(fields[i]->at, 4));
	}
	data.randomSeed = data.words[0];
	data.localSeed = data.words[1];
	data.gameTurn = data.words[2];
	data.thingsMade = data.words[4];
	data.worldPopulation = data.words[7];
	return data;
}

std::optional<float> VanillaWorld::VisualTime() const
{
	return Float(Game(), "visualTime");
}

VanillaWorldResult ReadVanillaWorld(std::span<const uint8_t> bytes)
{
	auto framing = ReadVanillaFraming(bytes);
	if (!framing.has_value())
	{
		return {.world = std::nullopt, .error = VanillaWorldError {.at = 0, .reason = "The file does not start as the game's"}};
	}
	WorldReader reader(bytes, framing->body, StreamStart {.objects = 0, .checksum = framing->checksum});
	// The game object has no reference before its record
	reader.Record(framing->body);
	if (!reader.Failed() && reader.Position() != bytes.size())
	{
		reader.Fail("The world stops before the end of the file");
	}
	if (reader.Failed())
	{
		return {.world = std::nullopt, .error = reader.Error()};
	}
	return {.world = VanillaWorld(std::vector<uint8_t>(bytes.begin(), bytes.end()), std::move(*framing), reader.TakeRecords(),
	                              reader.TakeNumbers()),
	        .error = std::nullopt};
}

std::optional<VanillaWorld> ReadVanillaRecords(std::span<const uint8_t> bytes, StreamStart start, std::string* error)
{
	WorldReader reader(bytes, 0, start);
	const auto number = reader.Ref();
	if (!reader.Failed() && number != start.objects + 1)
	{
		reader.Fail("The bytes do not start with a new object");
	}
	if (!reader.Failed() && reader.Position() != bytes.size())
	{
		reader.Fail("The record stops before the end of the bytes");
	}
	if (reader.Failed())
	{
		if (error != nullptr)
		{
			*error = reader.Error()->reason;
		}
		return std::nullopt;
	}
	return VanillaWorld(std::vector<uint8_t>(bytes.begin(), bytes.end()), VanillaFraming {}, reader.TakeRecords(),
	                    reader.TakeNumbers());
}

bool VillagerStateKeepsValues(uint8_t state)
{
	return state < k_VillagerStateLayouts.size() && k_VillagerStateLayouts.at(state) != 0;
}

bool KnowsVanillaType(uint32_t type)
{
	return ReaderOf(type) != nullptr;
}

} // namespace openblack::gamesave
