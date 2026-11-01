/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <VanillaSaveFile.h>
#include <VanillaWorld.h>
#include <gtest/gtest.h>

using namespace openblack::gamesave;

namespace
{

// Records cut from world files of the game. No record has another new record in it.
constexpr StreamStart k_TreeStart {.objects = 224, .checksum = 136259};
constexpr std::string_view k_TreeHex =
    "e10000004f000000000000007f1502000000000000c315ce0000401a010000000001803d080000c315ce0000401a0100000000da04000000"
    "00803ff4fd0c40d2e4cc3f00000000000000000000f4fd0c40e00000001000";
constexpr StreamStart k_VillagerStart {.objects = 558, .checksum = 485758};
constexpr std::string_view k_VillagerHex =
    "2f0200000600000000000000bf6907000000000000d30efa00c18cfe00000000000100b9080000d30efa00c18cfe0000000000980000008c"
    "cb673fa4f3833f4113283f0000000000000401c902d6000400000000d30efa00c18cfe000000000000000000edfeffff0000000018ffffff"
    "2702000000000000f6000000f00000000000000000003850ffff000000000000000000080000000000000000002702000000000000000000"
    "00000000000000000000000000748b633fe418010000010000000000000025020000b4010000000000000000000000000000";
constexpr StreamStart k_AbodeStart {.objects = 299, .checksum = 250087};
constexpr std::string_view k_AbodeHex =
    "2c010000070000000000000026d1030000000000001884da00f027e600402120bf03805a0900001884da00f027e600000000006e00000000"
    "00803f0000803ff85323400000000000000000080000803f0000000000000000031884d600f027e3000000000000000000db000000000000"
    "000000000102000000000000000000000000000000";
constexpr StreamStart k_CreatureStart {.objects = 1817, .checksum = 1697839};
constexpr std::string_view k_CreatureHex =
    "1a0700006900000000000000bee8190000000000409562b400a3ccfb00000000000103d4090000264cb400cbd1fb00000000001b08000000"
    "00803fb81e853e000000000000000000001800c3050000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000d461ffff000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000210000000000000000000000000000000000000000000000000000000000000000000100000000000000000000"
    "0000000000000000000000000002000000000000000000000000000000000000000000000003000000000000000000000000000000000000"
    "0000000000040000000000000000000000000000000000000000000000050000000000000000000000000000000000000000000000060000"
    "0000000000000000000000000000000000000000000700000000000000000000000000000000000000000000000800000000000000000000"
    "000000000000000000000000000900000000000000000000000000000000000000000000000a000000000000000000000000000000000000"
    "00000000000b00000000000000000000000000000000000000000000000c00000000000000000000000000000000000000000000000d0000"
    "0000000000000000000000000000000000000000000e00000000000000000000000000000000000000000000000f00000000000000000000"
    "0000000000000000000000000000000000010000009a991940cdcc4c3e000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000014d5e04452b8a0413fe41f4500000000000000000000000000ea98bd001f13fc00000000000000803f"
    "000000000000000000000000010000000671323ededd873e000000000000803f000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000";

std::vector<uint8_t> FromHex(std::string_view hex)
{
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i + 1 < hex.size(); i += 2)
	{
		bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
	}
	return bytes;
}

/// Writes values as the game writes them, with the running sum
class Writer
{
public:
	Writer(std::vector<uint8_t> start, uint32_t sum)
	    : _bytes(std::move(start))
	    , _sum(sum)
	{
	}

	void Value(std::initializer_list<uint8_t> value)
	{
		_sum += *value.begin() + static_cast<uint32_t>(value.size());
		_bytes.insert(_bytes.end(), value.begin(), value.end());
	}
	void Byte() { Value({0}); }
	void Half() { Value({0, 0}); }
	void Word(uint32_t value = 0)
	{
		Value({static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value >> 16),
		       static_cast<uint8_t>(value >> 24)});
	}
	void Words(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Word();
		}
	}
	void Zeros(uint32_t size)
	{
		_sum += size;
		_bytes.insert(_bytes.end(), size, 0);
	}
	[[nodiscard]] uint32_t Sum() const { return _sum; }
	[[nodiscard]] std::vector<uint8_t> Take() { return std::move(_bytes); }

private:
	std::vector<uint8_t> _bytes;
	uint32_t _sum;
};

/// A world file with a game object that holds no objects, only the game data and the time of day
std::vector<uint8_t> SmallWorld(const std::array<uint32_t, 8>& data, float visualTime)
{
	const auto framing = WriteVanillaFraming(".\\DATA\\LANDSCAPE\\Land1.lnd");
	const auto read = ReadVanillaFraming(framing);
	Writer w(framing, read->checksum);
	w.Word(0x6A);
	w.Word();
	w.Word(w.Sum());
	w.Word();
	w.Byte();
	// The text of the revision, the flags, the land and the players
	w.Words(1 + 2 + 8);
	for (const auto value : data)
	{
		w.Word(value);
	}
	w.Byte();
	w.Byte();
	w.Byte();
	w.Word();
	// The lists of objects
	w.Words(44);
	w.Words(5);
	w.Word(std::bit_cast<uint32_t>(visualTime));
	w.Words(4);
	// The help
	w.Words(4 + 2 + 3);
	w.Half();
	w.Words(1 + 1 + 3 + 1 + 1 + 1 + 3);
	w.Half();
	w.Words(2);
	w.Half();
	w.Words(4 + 3 + 1 + 9 + 1);
	// The objects of the scripts
	for (int i = 0; i < 511; ++i)
	{
		w.Word();
		w.Word();
		w.Byte();
	}
	for (const uint32_t size : {4, 4, 4, 4, 4, 12, 12, 4, 4, 4, 4})
	{
		w.Zeros(size);
	}
	w.Words(4 + 2 + 7);
	// The start of the camera, the camera, the weather and the areas that the camera cannot go to
	w.Zeros(12);
	w.Words(2);
	w.Words(7 + 1 + 1 + 1);
	w.Words(3 + 1 + 1 + 1 + 1 + 1);
	w.Zeros(8);
	w.Word();
	w.Byte();
	w.Words(3 + 1 + 1);
	// The bookmarks
	for (int i = 0; i < 11; ++i)
	{
		w.Byte();
		w.Zeros(12);
		w.Word();
		w.Zeros(12);
		w.Zeros(12);
	}
	w.Words(3);
	w.Zeros(32);
	// The hand
	w.Words(1 + 1 + 4);
	w.Zeros(12);
	w.Words(2);
	w.Byte();
	w.Words(1 + 2 + 1 + 1 + 1 + 5 + 10 + 1);
	w.Zeros(12);
	return w.Take();
}

constexpr std::array<uint32_t, 8> k_Data {11, 12, 7000, 14, 2333, 16, 0, 51};

} // namespace

TEST(VanillaWorld, ReadsATreeRecord)
{
	std::string error;
	const auto world = ReadVanillaRecords(FromHex(k_TreeHex), k_TreeStart, &error);
	ASSERT_TRUE(world.has_value()) << error;
	ASSERT_EQ(world->Records().size(), 1);
	const auto& tree = world->Game();
	EXPECT_EQ(tree.number, 225);
	EXPECT_TRUE(tree.Is(VanillaType::Tree));
	EXPECT_EQ(tree.end, world->Bytes().size());
	EXPECT_EQ(world->Word(tree, "info"), 2109);
	EXPECT_EQ(world->Word(tree, "created"), 1242);
	EXPECT_EQ(world->Float(tree, "life"), 1.0F);
	EXPECT_FLOAT_EQ(*world->Float(tree, "scale"), 2.203F);
	EXPECT_EQ(world->Ref(tree, "fire"), 0);
	EXPECT_EQ(world->Record(225), &tree);
	EXPECT_EQ(world->Record(224), nullptr);
}

TEST(VanillaWorld, RefusesARecordWithAnotherSum)
{
	auto bytes = FromHex(k_TreeHex);
	// The sum that the record keeps comes after the number, the type and a word
	bytes[12] ^= 1;
	std::string error;
	EXPECT_FALSE(ReadVanillaRecords(bytes, k_TreeStart, &error).has_value());
	EXPECT_NE(error.find("sum"), std::string::npos) << error;
}

TEST(VanillaWorld, RefusesATypeWithNoLayout)
{
	auto bytes = FromHex(k_TreeHex);
	// The running sum adds only the first byte of the type, thus the sum stays correct
	bytes[5] = 1;
	EXPECT_FALSE(KnowsVanillaType(0x14F));
	EXPECT_TRUE(KnowsVanillaType(0x4F));
	EXPECT_FALSE(ReadVanillaRecords(bytes, k_TreeStart).has_value());
}

TEST(VanillaWorld, RefusesARecordThatStopsEarly)
{
	auto bytes = FromHex(k_TreeHex);
	bytes.pop_back();
	EXPECT_FALSE(ReadVanillaRecords(bytes, k_TreeStart).has_value());
	bytes = FromHex(k_TreeHex);
	bytes.push_back(0);
	EXPECT_FALSE(ReadVanillaRecords(bytes, k_TreeStart).has_value());
}

TEST(VanillaWorld, ReadsAVillagerRecord)
{
	std::string error;
	const auto world = ReadVanillaRecords(FromHex(k_VillagerHex), k_VillagerStart, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto& villager = world->Game();
	EXPECT_TRUE(villager.Is(VanillaType::Villager));
	EXPECT_EQ(villager.number, 559);
	EXPECT_EQ(world->Ref(villager, "home"), 549);
	EXPECT_EQ(world->Ref(villager, "town"), 436);
	EXPECT_EQ(world->Ref(villager, "mother"), 0);
	EXPECT_EQ(world->Ref(villager, "walkFootpath"), 551);
	EXPECT_EQ(world->Byte(villager, "discipleType"), 0);
	EXPECT_EQ(world->Byte(villager, "moveState"), 4);
	EXPECT_EQ(world->Half(villager, "speed"), 713);
	EXPECT_EQ(world->Half(villager, "status"), 8);
	EXPECT_EQ(world->Word(villager, "birthTurn"), 4294922296U);
}

TEST(VanillaWorld, ReadsAnAbodeRecord)
{
	std::string error;
	const auto world = ReadVanillaRecords(FromHex(k_AbodeHex), k_AbodeStart, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto& abode = world->Game();
	EXPECT_TRUE(abode.Is(VanillaType::Abode));
	EXPECT_EQ(world->Ref(abode, "town"), 219);
	EXPECT_EQ(world->Byte(abode, "index"), 1);
	EXPECT_EQ(world->Byte(abode, "abodeFlags"), 3);
	EXPECT_EQ(world->Float(abode, "built"), 1.0F);
	EXPECT_FLOAT_EQ(*world->Float(abode, "heading"), 2.552F);
	EXPECT_TRUE(world->Refs(abode, "villagers").empty());
	// Two resource values of 4 bytes each
	EXPECT_EQ(world->Raw(abode, "resources").size(), 8);
}

TEST(VanillaWorld, ReadsACreatureRecord)
{
	std::string error;
	const auto world = ReadVanillaRecords(FromHex(k_CreatureHex), k_CreatureStart, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto& creature = world->Game();
	EXPECT_TRUE(creature.Is(VanillaType::Creature));
	EXPECT_EQ(creature.flags, 64);
	EXPECT_EQ(world->Word(creature, "version"), 33);
	EXPECT_EQ(world->Byte(creature, "owner"), 0);
	EXPECT_EQ(world->Byte(creature, "index"), 0);
	EXPECT_EQ(world->Word(creature, "info"), 2516);
}

TEST(VanillaWorld, ReadsTheGameDataOfAWholeFile)
{
	const auto bytes = SmallWorld(k_Data, 12.5F);
	const auto result = ReadVanillaWorld(bytes);
	ASSERT_TRUE(result.world.has_value()) << result.error->reason << " at " << result.error->at;
	const auto& world = *result.world;
	EXPECT_EQ(world.Framing().landscape, ".\\DATA\\LANDSCAPE\\Land1.lnd");
	ASSERT_EQ(world.Records().size(), 1);
	EXPECT_TRUE(world.Game().Is(VanillaType::Game));
	const auto data = world.GameData();
	ASSERT_TRUE(data.has_value());
	EXPECT_EQ(data->randomSeed, 11);
	EXPECT_EQ(data->localSeed, 12);
	EXPECT_EQ(data->gameTurn, 7000);
	EXPECT_EQ(data->thingsMade, 2333);
	EXPECT_EQ(data->worldPopulation, 51);
	EXPECT_EQ(data->words, k_Data);
	EXPECT_EQ(world.VisualTime(), 12.5F);
	EXPECT_EQ(world.Fields(world.Game(), "lists").size(), 44);
	EXPECT_EQ(world.Refs(world.Game(), "players").size(), 8);
}

TEST(VanillaWorld, RefusesAFileWithMoreAfterTheWorld)
{
	auto bytes = SmallWorld(k_Data, 12.5F);
	bytes.push_back(0);
	const auto result = ReadVanillaWorld(bytes);
	EXPECT_FALSE(result.world.has_value());
	ASSERT_TRUE(result.error.has_value());
	EXPECT_EQ(result.error->at, bytes.size() - 1);
}

namespace
{

// The sizes of the base parts of a record when every value is 0: no physics, no lists, no broken mesh
constexpr uint32_t k_PositionedSize = 14;
constexpr uint32_t k_MultiMapFixedSize = 67;
constexpr uint32_t k_MobileObjectSize = 68;
constexpr uint32_t k_RockSize = 91;
constexpr uint32_t k_TownSpellIconSize = 91;
constexpr uint32_t k_AbodeSize = 104;
constexpr uint32_t k_LandscapeVortexSize = 111;
constexpr uint32_t k_SpellSize = 208;
constexpr uint32_t k_CollectionSize = 8;

/// The record of one new object of the type, whose values the function writes
std::vector<uint8_t> NewRecord(uint32_t type, void (*values)(Writer&))
{
	Writer w({}, 0);
	// The number of the new object, its type and a word, then the running sum, a word and the flags
	w.Word(1);
	w.Word(type);
	w.Word();
	w.Word(w.Sum());
	w.Word();
	w.Byte();
	values(w);
	return w.Take();
}

/// A count, then the words
void Counted(Writer& w, int count)
{
	w.Word(static_cast<uint32_t>(count));
	w.Words(count);
}

struct RareType
{
	uint32_t type;
	void (*values)(Writer&);
};

// Each type that no save of the tests has, with values that fill each list and each count
const std::array k_RareTypes = {
    RareType {0x04,
              [](Writer& w) {
	              w.Zeros(12);
	              w.Word();
              }},
    RareType {0x14,
              [](Writer& w) {
	              w.Zeros(k_SpellSize);
	              Counted(w, 2);
              }},
    RareType {0x16,
              [](Writer& w) {
	              w.Word();
	              w.Zeros(24);
	              w.Words(2);
	              w.Byte();
              }},
    RareType {0x1C,
              [](Writer& w) {
	              w.Zeros(k_LandscapeVortexSize);
	              w.Words(2);
	              w.Zeros(12);
	              w.Words(3);
	              Counted(w, 5);
	              Counted(w, 1);
	              w.Word();
              }},
    RareType {0x2C,
              [](Writer& w) {
	              w.Zeros(k_PositionedSize);
	              w.Words(4);
	              Counted(w, 6);
	              Counted(w, 6);
	              Counted(w, 4);
	              Counted(w, 4);
	              w.Words(3 + 4);
	              for (int i = 0; i < 3 * 3; ++i)
	              {
		              w.Byte();
		              w.Words(3);
	              }
	              w.Words(9 * 9 * 2 + 8 * 4);
	              w.Words(18);
	              Counted(w, 20);
	              w.Words(2);
	              w.Half();
	              w.Words(6 + 4 + 1);
	              w.Half();
	              w.Half();
              }},
    RareType {0x43,
              [](Writer& w) {
	              w.Zeros(k_PositionedSize);
	              w.Zeros(80);
	              w.Word(1);
	              w.Zeros(12);
	              w.Zeros(80);
	              w.Words(2);
	              w.Zeros(12);
	              w.Word();
              }},
    RareType {0x45,
              [](Writer& w) {
	              w.Zeros(k_AbodeSize);
	              w.Word();
	              Counted(w, 10);
	              Counted(w, 20);
	              w.Word();
	              w.Zeros(12 * 12);
	              w.Words(2);
	              w.Byte();
	              w.Word();
	              w.Byte();
	              w.Word();
	              w.Byte();
	              w.Words(6);
	              w.Byte();
	              w.Byte();
	              w.Words(3);
	              for (int i = 0; i < 4; ++i)
	              {
		              Counted(w, 1);
	              }
	              Counted(w, 2);
	              Counted(w, 2);
	              w.Word();
	              Counted(w, 40);
	              w.Zeros(12);
	              w.Word();
              }},
    RareType {0x52,
              [](Writer& w) {
	              w.Zeros(k_AbodeSize);
	              w.Words(4);
	              w.Word(3);
	              w.Byte();
	              w.Byte();
	              w.Byte();
	              Counted(w, 1);
              }},
    RareType {0x56,
              [](Writer& w) {
	              w.Zeros(k_TownSpellIconSize);
	              w.Word();
	              Counted(w, 3);
              }},
    RareType {0x58,
              [](Writer& w) {
	              w.Zeros(k_MobileObjectSize);
	              w.Words(3);
	              w.Byte();
              }},
    RareType {0xB6,
              [](Writer& w) {
	              w.Words(2 + 8);
	              w.Byte();
	              w.Word();
	              w.Word(2);
	              w.Zeros(2 * 28);
	              w.Word();
	              w.Byte();
              }},
    RareType {0xB7,
              [](Writer& w) {
	              w.Word();
	              w.Zeros(16);
              }},
    RareType {0xB9,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Word();
	              w.Word(2);
	              w.Zeros(2 * 28);
	              w.Words(2);
              }},
    RareType {0xBC,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Word();
	              w.Word(1);
	              w.Zeros(28);
	              w.Words(2);
              }},
    RareType {0xCF,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Byte();
	              w.Byte();
	              w.Zeros(12);
	              w.Word();
	              w.Word(2);
	              w.Words(2 * 2);
	              w.Words(2);
	              w.Byte();
	              w.Byte();
	              w.Word();
	              w.Byte();
              }},
    RareType {0xD5,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Words(2);
              }},
    RareType {0xD8,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Words(2);
	              w.Zeros(3 * 12);
	              w.Word();
	              w.Byte();
	              w.Byte();
	              w.Byte();
	              w.Words(4);
	              w.Word(1);
	              w.Words(2);
	              w.Zeros(12);
	              w.Byte();
	              w.Byte();
	              w.Byte();
	              w.Word();
	              // Two forks of 3 and 0 words
	              w.Word(2);
	              Counted(w, 3);
	              w.Word();
	              w.Word();
	              w.Word(1);
	              w.Word();
	              w.Byte();
	              w.Words(5);
	              w.Zeros(12);
              }},
    RareType {0xF1,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Byte();
              }},
    RareType {0xF3,
              [](Writer& w) {
	              w.Zeros(k_CollectionSize);
	              w.Byte();
	              w.Word(2);
	              w.Word(2);
	              w.Zeros(2 * 64);
              }},
    RareType {0xF7,
              [](Writer& w) {
	              w.Zeros(k_RockSize);
	              w.Words(3);
	              // A broken mesh of one part with one face
	              w.Word(1);
	              w.Words(3 + 3);
	              w.Word(1);
	              w.Zeros(88);
              }},
    RareType {0xF8,
              [](Writer& w) {
	              w.Zeros(k_MultiMapFixedSize);
	              w.Byte();
	              w.Zeros(12);
	              w.Words(2);
	              Counted(w, 1);
	              w.Zeros(4);
	              w.Word();
	              w.Word();
              }},
    RareType {0xFB,
              [](Writer& w) {
	              w.Words(2);
	              w.Word(64);
	              w.Zeros(64);
	              w.Words(3);
	              w.Zeros(12);
              }},
    RareType {0x10C,
              [](Writer& w) {
	              w.Word(260);
	              w.Zeros(260);
	              w.Words(3);
              }},
};

} // namespace

TEST(VanillaWorld, ReadsEachTypeThatNoTestSaveHas)
{
	for (const auto& rare : k_RareTypes)
	{
		SCOPED_TRACE(rare.type);
		EXPECT_TRUE(KnowsVanillaType(rare.type));
		const auto bytes = NewRecord(rare.type, rare.values);
		std::string error;
		const auto world = ReadVanillaRecords(bytes, {}, &error);
		ASSERT_TRUE(world.has_value()) << error;
		EXPECT_EQ(world->Game().type, rare.type);
		EXPECT_EQ(world->Game().end, bytes.size());
		// One byte less or one byte more does not read
		auto shorter = bytes;
		shorter.pop_back();
		EXPECT_FALSE(ReadVanillaRecords(shorter, {}).has_value());
		auto longer = bytes;
		longer.push_back(0);
		EXPECT_FALSE(ReadVanillaRecords(longer, {}).has_value());
	}
}

TEST(VanillaWorld, ReadsNoValuesForACountBelowOne)
{
	// A chain with a count of joints of -1
	const auto bytes = NewRecord(0xB6, [](Writer& w) {
		w.Words(2 + 8);
		w.Byte();
		w.Word();
		w.Word(0xFFFFFFFF);
		w.Word();
		w.Byte();
	});
	std::string error;
	EXPECT_TRUE(ReadVanillaRecords(bytes, {}, &error).has_value()) << error;
}

TEST(VanillaWorld, KnowsNoLayoutOfTheHandType)
{
	// The hand writes its values inside the interface status with no record of its own, thus its type never starts a
	// record
	EXPECT_FALSE(KnowsVanillaType(0xFD));
}
