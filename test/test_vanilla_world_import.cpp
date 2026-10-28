/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include <VanillaWorld.h>
#include <gtest/gtest.h>

#include "ECS/VanillaWorldImport.h"

using namespace openblack;
using namespace openblack::gamesave;
namespace world_import = openblack::ecs::vanilla_world_import;

namespace
{

/// Makes a small world of records with named values, as the reader keeps them
class FakeWorld
{
public:
	/// Starts a new record. The first record is the game object.
	FakeWorld& Record(VanillaType type)
	{
		_records.push_back({.number = static_cast<uint32_t>(_records.size() + 1), .type = static_cast<uint32_t>(type)});
		return *this;
	}

	template <typename T>
	FakeWorld& Value(std::string_view name, T value)
	{
		const auto at = _bytes.size();
		_bytes.resize(at + sizeof(T));
		std::memcpy(_bytes.data() + at, &value, sizeof(T));
		_records.back().fields.push_back({.name = name, .kind = VanillaValue::Bytes, .at = at, .size = sizeof(T)});
		return *this;
	}

	FakeWorld& Position(int32_t x, int32_t z) { return Value("position", MapCoords {.x = x, .z = z, .altitude = 0.0f}); }

	FakeWorld& Ref(std::string_view name, uint32_t number)
	{
		_records.back().fields.push_back({.name = name, .kind = VanillaValue::Reference, .number = number});
		return *this;
	}

	FakeWorld& Refs(std::string_view name, std::vector<uint32_t> numbers)
	{
		_records.back().fields.push_back({.name = name,
		                                  .kind = VanillaValue::References,
		                                  .count = static_cast<uint32_t>(numbers.size()),
		                                  .number = static_cast<uint32_t>(_numbers.size())});
		_numbers.insert(_numbers.end(), numbers.begin(), numbers.end());
		return *this;
	}

	FakeWorld& Words(std::string_view name, std::vector<uint32_t> words)
	{
		const auto at = _bytes.size();
		for (const auto word : words)
		{
			Value(name, word);
		}
		// The reader keeps an array as one value, with the place of its first value
		auto& fields = _records.back().fields;
		fields.resize(fields.size() - words.size());
		fields.push_back({.name = name,
		                  .kind = VanillaValue::Array,
		                  .at = at,
		                  .size = sizeof(uint32_t),
		                  .count = static_cast<uint32_t>(words.size())});
		return *this;
	}

	[[nodiscard]] VanillaWorld Make() const { return {_bytes, VanillaFraming {}, _records, _numbers}; }

private:
	std::vector<uint8_t> _bytes;
	std::vector<VanillaRecord> _records;
	std::vector<uint32_t> _numbers;
};

/// A game object with the turn in its game data
FakeWorld GameAtTurn(uint32_t turn)
{
	FakeWorld world;
	world.Record(VanillaType::Game);
	for (const uint32_t word : std::array<uint32_t, 8> {11, 22, turn, 0, 40, 0, 0, 3})
	{
		world.Value("data", word);
	}
	return world;
}

constexpr uint32_t AbodeRowNumber(AbodeInfo row)
{
	return world_import::k_FirstAbodeRowNumber - static_cast<uint32_t>(row);
}

constexpr uint32_t VillagerRowNumber(VillagerInfo row)
{
	return world_import::k_FirstVillagerRowNumber - static_cast<uint32_t>(row);
}

} // namespace

TEST(VanillaWorldImport, RowNumbersCountBackFromTheFirstRowOfEachTable)
{
	EXPECT_EQ(world_import::AbodeRowOf(2394), AbodeInfo::CelticHut);
	EXPECT_EQ(world_import::AbodeRowOf(2270), AbodeInfo::NorseTownCentre);
	EXPECT_EQ(world_import::AbodeRowOf(2275), AbodeInfo::NorseStoragePit);
	EXPECT_EQ(world_import::AbodeRowOf(2350), AbodeInfo::AztecTownCentre);
	// The last building row, then the first villager row
	EXPECT_EQ(world_import::AbodeRowOf(2248), AbodeInfo::Belltower);
	EXPECT_EQ(world_import::AbodeRowOf(2247), std::nullopt);
	EXPECT_EQ(world_import::AbodeRowOf(2395), std::nullopt);
	EXPECT_EQ(world_import::VillagerRowOf(2247), VillagerInfo::CelticHousewifeFemale);
	EXPECT_EQ(world_import::VillagerRowOf(2198), VillagerInfo::NorseHousewifeFemale);
	EXPECT_EQ(world_import::VillagerRowOf(2233), VillagerInfo::AztecHousewifeFemale);
	EXPECT_EQ(world_import::VillagerRowOf(2170), VillagerInfo::CreatureTrainer);
	EXPECT_EQ(world_import::VillagerRowOf(2248), std::nullopt);
}

TEST(VanillaWorldImport, ImportsTheBuildingsOfTheTownOnly)
{
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseHut));
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseTempleY));
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseStoragePit));
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseCreche));
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseGraveyard));
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseTownCentre));
	EXPECT_FALSE(world_import::IsImportedBuilding(AbodeInfo::NorseTotem));
	EXPECT_FALSE(world_import::IsImportedBuilding(AbodeInfo::NorseWorkshop));
	EXPECT_FALSE(world_import::IsImportedBuilding(AbodeInfo::NorseField));
	EXPECT_FALSE(world_import::IsImportedBuilding(AbodeInfo::Belltower));
	EXPECT_FALSE(world_import::IsImportedBuilding(AbodeInfo::None));
}

TEST(VanillaWorldImport, KeepsTheStatesOnlyWhenTheFileHasAllTheyNeed)
{
	constexpr auto k_Sleeping = static_cast<uint8_t>(VillagerStates::SleepingAtHome);
	constexpr auto k_Chillout = static_cast<uint8_t>(VillagerStates::SitAndChillout);
	constexpr auto k_OnPath = static_cast<uint8_t>(VillagerStates::MoveOnPath);
	constexpr auto k_Decide = static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
	constexpr auto k_Invalid = static_cast<uint8_t>(VillagerStates::InvalidState);
	// A villager that stands still in a state with no more values keeps its states
	EXPECT_EQ(world_import::StartStates({k_Sleeping, 0, k_Chillout}, 0x04),
	          (std::array<uint8_t, 3> {k_Sleeping, 0, k_Chillout}));
	// A villager that walks decides again
	EXPECT_EQ(world_import::StartStates({k_Chillout, 0, 0}, 0x0C), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	// A state with more values in the file, at the top or at the end, makes the villager decide again
	EXPECT_EQ(world_import::StartStates({k_OnPath, 0, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_EQ(world_import::StartStates({1, k_OnPath, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
}

TEST(VanillaWorldImport, VillagersInTheseStatesAreInTheirHomes)
{
	EXPECT_TRUE(world_import::IsInsideHome(VillagerStates::AtHome));
	EXPECT_TRUE(world_import::IsInsideHome(VillagerStates::GotoBedAtHome));
	EXPECT_TRUE(world_import::IsInsideHome(VillagerStates::SleepingAtHome));
	EXPECT_FALSE(world_import::IsInsideHome(VillagerStates::HavingSex));
	EXPECT_FALSE(world_import::IsInsideHome(VillagerStates::SitAndChillout));
}

TEST(VanillaWorldImport, ReadsTheTownsBuildingsAndVillagersOfTheFile)
{
	auto fake = GameAtTurn(64999);
	// 2: the town
	fake.Record(VanillaType::Town)
	    .Position(12226462, 17309630)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
	    .Value<uint8_t>("player", 0)
	    .Value<uint32_t>("cannotBuildWorshipSite", 1)
	    .Value<uint32_t>("uninhabitable", 0)
	    .Refs("abodes", {3, 4})
	    .Refs("homeless", {6})
	    .Value<uint32_t>("emptyTownTimer", 7);
	// 3: a storage pit with food and wood
	fake.Record(VanillaType::StoragePit)
	    .Position(11972051, 17310418)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseStoragePit))
	    .Value("life", 1.0f)
	    .Value("scale", 0.888f)
	    .Value("heading", 3.73f)
	    .Value("built", 1.0f)
	    .Ref("town", 2)
	    .Refs("villagers", {})
	    .Value<uint8_t>("present", 0)
	    .Words("resources", {19054, 28890});
	// 4: an abode with one villager in it
	fake.Record(VanillaType::Abode)
	    .Position(12129665, 17652252)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseTempleY))
	    .Value("life", 0.5f)
	    .Value("scale", 1.0f)
	    .Value("heading", 1.963f)
	    .Value("built", 0.25f)
	    .Ref("town", 2)
	    .Refs("villagers", {5})
	    .Value<uint8_t>("present", 1)
	    .Words("resources", {0, 0});
	// 5: a named villager asleep at home
	fake.Record(VanillaType::SpecialVillager)
	    .Position(11751603, 17540633)
	    .Value("info", VillagerRowNumber(VillagerInfo::NorseHousewifeFemale))
	    .Value<uint32_t>("created", 57)
	    .Value("life", 0.903f)
	    .Value("scale", 0.97f)
	    .Value("heading", 2.47f)
	    .Value<uint16_t>("turnsToStateChange", 300)
	    .Value<uint8_t>("moveState", 4)
	    .Value("action", std::array<uint8_t, 6> {120, 0, 246, 0, 0x34, 0x12})
	    .Value<uint32_t>("birthTurn", 4294947798U)
	    .Value("food", 0.584f)
	    .Value<uint32_t>("lastCheckTurn", 64990)
	    .Value<uint8_t>("foodSpeedUp", 2)
	    .Value<int16_t>("foodHeld", -1)
	    .Value<int16_t>("woodHeld", 30)
	    .Value<int16_t>("pregnancy", 9)
	    .Ref("home", 4)
	    .Ref("town", 2)
	    .Ref("mother", 0)
	    .Value<uint32_t>("specialRow", 18);
	// 6: a homeless villager, and its mother
	fake.Record(VanillaType::Villager)
	    .Position(1, 2)
	    .Value("info", VillagerRowNumber(VillagerInfo::NorseForesterMale))
	    .Value("action", std::array<uint8_t, 6> {})
	    .Ref("town", 2)
	    .Ref("mother", 5);
	// 7: a building of a row that the import does not make
	fake.Record(VanillaType::Abode).Position(0, 0).Value("info", AbodeRowNumber(AbodeInfo::NorseTotem));
	const auto world = fake.Make();

	const auto imported = world_import::ReadTowns(world);
	EXPECT_EQ(imported.turn, 64999U);

	ASSERT_EQ(imported.towns.size(), 1U);
	const auto& town = imported.towns[0];
	EXPECT_EQ(town.number, 2U);
	EXPECT_EQ(town.id, 0U);
	EXPECT_EQ(town.tribe, Tribe::NORSE);
	EXPECT_EQ(town.owner, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(town.position, (MapCoords {.x = 12226462, .z = 17309630, .altitude = 0.0f}));
	EXPECT_TRUE(town.cannotBuildWorshipSite);
	EXPECT_FALSE(town.uninhabitable);
	EXPECT_EQ(town.emptyTownTimer, 7U);
	EXPECT_EQ(town.abodes, (std::vector<uint32_t> {3, 4}));
	EXPECT_EQ(town.homeless, (std::vector<uint32_t> {6}));

	ASSERT_EQ(imported.abodes.size(), 2U);
	const auto& pit = imported.abodes[0];
	EXPECT_EQ(pit.row, AbodeInfo::NorseStoragePit);
	EXPECT_EQ(pit.town, 2U);
	EXPECT_EQ(pit.food, 19054U);
	EXPECT_EQ(pit.wood, 28890U);
	EXPECT_EQ(pit.scale, 0.888f);
	EXPECT_EQ(pit.heading, 3.73f);
	const auto& abode = imported.abodes[1];
	EXPECT_EQ(abode.row, AbodeInfo::NorseTempleY);
	EXPECT_EQ(abode.life, 0.5f);
	EXPECT_EQ(abode.built, 0.25f);
	EXPECT_EQ(abode.villagers, (std::vector<uint32_t> {5}));
	EXPECT_EQ(abode.present, 1U);

	ASSERT_EQ(imported.villagers.size(), 2U);
	const auto& named = imported.villagers[0];
	EXPECT_EQ(named.number, 5U);
	EXPECT_EQ(named.row, VillagerInfo::NorseHousewifeFemale);
	EXPECT_EQ(named.nameRow, std::optional<uint8_t> {18});
	EXPECT_EQ(named.made, 57U);
	EXPECT_EQ(named.life, 0.903f);
	EXPECT_EQ(named.scale, 0.97f);
	EXPECT_EQ(named.heading, 2.47f);
	EXPECT_EQ(named.birthTurn, 4294947798U);
	EXPECT_EQ(named.food, 0.584f);
	EXPECT_EQ(named.lastCheckTurn, 64990U);
	EXPECT_EQ(named.foodSpeedUp, 2U);
	EXPECT_EQ(named.foodHeld, -1);
	EXPECT_EQ(named.woodHeld, 30);
	EXPECT_EQ(named.pregnancy, 9);
	EXPECT_EQ(named.home, 4U);
	EXPECT_EQ(named.town, 2U);
	EXPECT_EQ(named.mother, 0U);
	EXPECT_EQ(named.states, (std::array<uint8_t, 3> {120, 0, 246}));
	EXPECT_EQ(named.moveState, 4U);
	EXPECT_EQ(named.turnsUntilStateChange, 300U);
	EXPECT_EQ(named.turnsSinceStateChange, 0x1234U);
	const auto& homeless = imported.villagers[1];
	EXPECT_EQ(homeless.row, VillagerInfo::NorseForesterMale);
	EXPECT_EQ(homeless.nameRow, std::nullopt);
	EXPECT_EQ(homeless.home, 0U);
	EXPECT_EQ(homeless.mother, 5U);

	// The building of the other row does not come in, and the import says why
	ASSERT_EQ(imported.skipped.size(), 1U);
	EXPECT_NE(imported.skipped[0].find("object 7"), std::string::npos);
}

/// The world files of the players' saved games of Land 1, in a copy of a Profiles folder that OPENBLACK_VANILLA_SAVES
/// gives. Each file gives all its towns, buildings and villagers to the import, and the villagers in their homes agree
/// with the count of each building.
TEST(VanillaWorldImport, SavedGamesOfLandOneImportWhole)
{
	const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (saves == nullptr || !std::filesystem::is_directory(saves, error))
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	uint32_t files = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(saves))
	{
		const auto name = entry.path().filename().string();
		if (!entry.is_regular_file() || entry.path().extension() != ".sav" || name.starts_with("Script"))
		{
			continue;
		}
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		const auto read = ReadVanillaWorld(bytes);
		ASSERT_TRUE(read.world.has_value()) << entry.path();
		if (read.world->Framing().landscape.find("Land1.lnd") == std::string::npos)
		{
			continue;
		}
		++files;
		const auto imported = world_import::ReadTowns(*read.world);
		EXPECT_TRUE(imported.skipped.empty()) << entry.path();
		EXPECT_EQ(imported.towns.size(), read.world->OfType(VanillaType::Town).size()) << entry.path();
		EXPECT_EQ(imported.villagers.size(),
		          read.world->OfType(VanillaType::Villager).size() + read.world->OfType(VanillaType::SpecialVillager).size())
		    << entry.path();
		uint32_t present = 0;
		for (const auto& abode : imported.abodes)
		{
			present += abode.present;
		}
		const auto inside =
		    std::ranges::count_if(imported.villagers, [&imported](const world_import::ImportedVillager& villager) {
			    const bool home = std::ranges::any_of(imported.abodes, [&villager](const world_import::ImportedAbode& abode) {
				    return abode.number == villager.home;
			    });
			    return home && world_import::IsInsideHome(static_cast<VillagerStates>(villager.states[0]));
		    });
		EXPECT_EQ(static_cast<uint32_t>(inside), present) << entry.path();
	}
	EXPECT_GT(files, 0U);
}
