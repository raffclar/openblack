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
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <VanillaWorld.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/VanillaSceneryImport.h"
#include "ECS/VanillaWorldImport.h"
#include "ECS/WallHugRules.h"

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
	// A villager that walks straight or steps through keeps its walk and its states
	constexpr auto k_ToField = static_cast<uint8_t>(VillagerStates::FarmerArrivesAtFarm);
	EXPECT_EQ(world_import::StartStates({1, k_ToField, 0}, 0x0B), (std::array<uint8_t, 3> {1, k_ToField, 0}));
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x0C), (std::array<uint8_t, 3> {1, k_Chillout, 0}));
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x0E), (std::array<uint8_t, 3> {1, k_Chillout, 0}));
	// A villager that heads for a circle or goes round one decides again
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x0C, true), world_import::DecideAgain());
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x0F), world_import::DecideAgain());
	EXPECT_EQ(world_import::DecideAgain(), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	// A state with values that openblack does not keep, at the top or at the end, makes the villager decide again
	EXPECT_EQ(world_import::StartStates({k_OnPath, 0, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_EQ(world_import::StartStates({1, k_OnPath, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::Fishing)));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::FarmerPlantsCrop)));
	EXPECT_FALSE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::InScript)));
}

TEST(VanillaWorldImport, TheValuesOfAStateGiveTheWorkOfAFarmerOrAFisherman)
{
	using world_import::ImportedStateValue;
	// A farmer keeps the spot in its field, then its field
	std::vector<uint8_t> spot(sizeof(MapCoords));
	const MapCoords coords {.x = 11950536, .z = 18509796, .altitude = 0.0f};
	std::memcpy(spot.data(), &coords, sizeof(coords));
	const auto farmer = world_import::WorkOf(
	    static_cast<uint8_t>(VillagerStates::FarmerArrivesAtFarm),
	    {ImportedStateValue {.object = std::nullopt, .bytes = spot}, ImportedStateValue {.object = 1048, .bytes = {}}});
	ASSERT_TRUE(farmer.has_value());
	EXPECT_EQ(farmer->target, 1048U);
	ASSERT_TRUE(farmer->spot.has_value());
	EXPECT_EQ(farmer->spot->x, 11950536);
	EXPECT_EQ(farmer->spot->z, 18509796);
	// A fisherman keeps its fish farm
	const auto fisherman =
	    world_import::WorkOf(static_cast<uint8_t>(VillagerStates::Fishing), {ImportedStateValue {.object = 85, .bytes = {}}});
	ASSERT_TRUE(fisherman.has_value());
	EXPECT_EQ(fisherman->target, 85U);
	EXPECT_FALSE(fisherman->spot.has_value());
	// Another state gives no work
	EXPECT_FALSE(
	    world_import::WorkOf(static_cast<uint8_t>(VillagerStates::MoveOnPath),
	                         {ImportedStateValue {.object = 1, .bytes = {}}, ImportedStateValue {.object = 2, .bytes = {}}})
	        .has_value());
}

TEST(VanillaWorldImport, MetresAndSpeedsGiveTheWholeUnitsOfTheFileAgain)
{
	// Every speed of a walk in whole units a turn
	for (uint32_t whole = 0; whole <= 0xFFFF; ++whole)
	{
		ASSERT_EQ(openblack::ecs::wall_hug::WholeSpeed(world_import::WalkSpeedOf(static_cast<uint16_t>(whole))),
		          static_cast<int32_t>(whole))
		    << whole;
	}
	// Map positions across the land, and the positions of the walks of Land 1
	for (const int32_t whole : {0, 1, 65535, 65536, 8090858, 11950536, 12295289, 16737624, 18509796})
	{
		const auto metres = world_import::MetresOfWhole(whole);
		ASSERT_TRUE(metres.has_value()) << whole;
		EXPECT_EQ(openblack::map_coords::ToFixed(*metres), whole);
	}
	// The product of a float with the metres in a whole unit rounds to an even number above 2^24, so no metre value
	// gives an odd number there
	EXPECT_FALSE(world_import::MetresOfWhole(16777217).has_value());
	EXPECT_FALSE(world_import::MetresOfWhole(33554431).has_value());
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

namespace
{

/// Calls the function with each world file of Land 1 in the copy of a Profiles folder that OPENBLACK_VANILLA_SAVES
/// gives. Gives the number of files, or nothing when there is no such folder.
template <typename Check>
std::optional<uint32_t> ForEachLandOneSave(Check&& check)
{
	const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (saves == nullptr || !std::filesystem::is_directory(saves, error))
	{
		return std::nullopt;
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
		EXPECT_TRUE(read.world.has_value()) << entry.path();
		if (!read.world.has_value() || read.world->Framing().landscape.find("Land1.lnd") == std::string::npos)
		{
			continue;
		}
		++files;
		check(entry.path(), *read.world);
	}
	return files;
}

/// The record types of the animals of Land 1 and the row of the table of animals that each has
constexpr std::array<std::pair<uint32_t, AnimalInfo>, 9> k_LandOneAnimals = {{
    {0x82, AnimalInfo::Cow},
    {0x83, AnimalInfo::Sheep},
    {0x85, AnimalInfo::Horse},
    {0x87, AnimalInfo::Pig},
    {0x88, AnimalInfo::Tortoise},
    {0x8E, AnimalInfo::Dove},
    {0x91, AnimalInfo::Swallow},
    {0x93, AnimalInfo::Seagull},
    {0x94, AnimalInfo::Bat},
}};

} // namespace

TEST(VanillaSceneryImport, RowNumbersCountBackFromTheFirstRowOfEachTable)
{
	EXPECT_EQ(world_import::TreeRowOf(2115), TreeInfo::Beech);
	EXPECT_EQ(world_import::TreeRowOf(2093), TreeInfo::Burnt);
	EXPECT_EQ(world_import::TreeRowOf(2092), std::nullopt);
	EXPECT_EQ(world_import::TreeRowOf(2116), std::nullopt);
	EXPECT_EQ(world_import::FeatureRowOf(2064), FeatureInfo::AztcOlmechead);
	EXPECT_EQ(world_import::AnimatedStaticRowOf(1986), AnimatedStaticInfo::NorseGate);
	EXPECT_EQ(world_import::MobileObjectRowOf(1970), MobileObjectInfo::EgyptBarrel);
	EXPECT_EQ(world_import::MobileStaticRowOf(1937), MobileStaticInfo::CeltFenceShort);
	EXPECT_EQ(world_import::PotRowOf(1874), PotInfo::StoragePitFoodPile);
	EXPECT_EQ(world_import::FieldTypeRowOf(1765), FieldTypeInfo::Wheat);
	EXPECT_EQ(world_import::AnimalRowOf(2548), AnimalInfo::Lion);
	EXPECT_EQ(world_import::AnimalRowOf(2538), AnimalInfo::Pig);
	EXPECT_EQ(world_import::BigForestRowOf(369), BigForestInfo::Info1);
	EXPECT_EQ(world_import::BigForestRowOf(365), std::nullopt);
	EXPECT_EQ(world_import::TotemStatueRowOf(378), 0U);
	EXPECT_EQ(world_import::TotemStatueRowOf(370), 8U);
	EXPECT_EQ(world_import::TotemStatueRowOf(369), std::nullopt);
}

TEST(VanillaSceneryImport, ReadsTheForestsInTheOrderOfTheGameList)
{
	FakeWorld fake;
	// 1: the game object, with five lists. The fifth list has the forests.
	fake.Record(VanillaType::Game)
	    .Refs("lists", {})
	    .Refs("lists", {})
	    .Refs("lists", {})
	    .Refs("lists", {})
	    .Refs("lists", {5, 2});
	// 2: a forest with a big forest
	fake.Record(VanillaType::Forest)
	    .Position(100, 200)
	    .Value<uint32_t>("scenic", 0)
	    .Value<uint32_t>("forestId", 1)
	    .Refs("grownTrees", {})
	    .Refs("growingTrees", {})
	    .Ref("bigForest", 3)
	    .Value<uint32_t>("lastTreeAddedTurn", 0)
	    .Value<uint32_t>("nextForestId", 10);
	// 3: the big forest
	fake.Record(VanillaType::BigForest)
	    .Position(110, 210)
	    .Value("info", world_import::k_FirstBigForestRowNumber)
	    .Value("life", 0.5f)
	    .Value("scale", 1.0f)
	    .Value("heading", 0.25f)
	    .Value("wood", 15000.0f);
	// 4: a tree of the forest of the town, which is full grown
	fake.Record(VanillaType::Tree)
	    .Position(300, 400)
	    .Value("info", world_import::k_FirstTreeRowNumber - static_cast<uint32_t>(TreeInfo::Pine))
	    .Value("life", 1.0f)
	    .Value("scale", 0.9f)
	    .Value("heading", 1.5f)
	    .Value<uint16_t>("growthCountdown", 0)
	    .Value("maxSize", 0.9f);
	// 5: the forest of the lone trees of a town, with one tree that grows and one that is grown
	fake.Record(VanillaType::Forest)
	    .Position(500, 600)
	    .Value<uint32_t>("scenic", 1)
	    .Value<uint32_t>("forestId", 9)
	    .Refs("grownTrees", {4})
	    .Refs("growingTrees", {6})
	    .Ref("bigForest", 0)
	    .Value<uint32_t>("lastTreeAddedTurn", 1234)
	    .Value<uint32_t>("nextForestId", 10);
	// 6: a tree that grows
	fake.Record(VanillaType::Tree)
	    .Position(310, 410)
	    .Value("info", world_import::k_FirstTreeRowNumber - static_cast<uint32_t>(TreeInfo::Oak))
	    .Value("life", 1.0f)
	    .Value("scale", 0.5f)
	    .Value("heading", 0.0f)
	    .Value<uint16_t>("growthCountdown", 17)
	    .Value("maxSize", 1.25f);
	// 7: a tree of a row that is not a tree
	fake.Record(VanillaType::Tree).Position(0, 0).Value<uint32_t>("info", 1);

	const auto imported = world_import::ReadForests(fake.Make());
	ASSERT_EQ(imported.forests.size(), 2U);
	// The game list has the forest of the town first
	EXPECT_EQ(imported.forests[0].number, 5U);
	EXPECT_EQ(imported.forests[0].id, 9U);
	EXPECT_TRUE(imported.forests[0].scenic);
	EXPECT_EQ(imported.forests[0].grown, std::vector<uint32_t> {4});
	EXPECT_EQ(imported.forests[0].growing, std::vector<uint32_t> {6});
	EXPECT_EQ(imported.forests[0].position.x, 500);
	EXPECT_EQ(imported.forests[1].number, 2U);
	EXPECT_FALSE(imported.forests[1].scenic);
	EXPECT_EQ(imported.forests[1].bigForest, 3U);
	EXPECT_EQ(imported.lastTreeAddedTurn, 1234U);
	EXPECT_EQ(imported.nextForestId, 10U);

	ASSERT_EQ(imported.bigForests.size(), 1U);
	EXPECT_EQ(imported.bigForests[0].row, BigForestInfo::Info1);
	EXPECT_FLOAT_EQ(imported.bigForests[0].wood, 15000.0f);
	EXPECT_FLOAT_EQ(imported.bigForests[0].life, 0.5f);
	EXPECT_FLOAT_EQ(imported.bigForests[0].heading, 0.25f);

	ASSERT_EQ(imported.trees.size(), 2U);
	EXPECT_EQ(imported.trees[0].row, TreeInfo::Pine);
	EXPECT_FLOAT_EQ(imported.trees[0].heading, 1.5f);
	EXPECT_EQ(imported.trees[1].row, TreeInfo::Oak);
	EXPECT_FLOAT_EQ(imported.trees[1].scale, 0.5f);
	EXPECT_FLOAT_EQ(imported.trees[1].maxSize, 1.25f);
	EXPECT_EQ(imported.trees[1].growthCountdown, 17U);
	ASSERT_EQ(imported.skipped.size(), 1U);
	EXPECT_NE(imported.skipped[0].find("object 7"), std::string::npos);
}

/// Each info number of the things of Land 1 is a row of the table of its kind, and each animal has the row of its
/// record type
TEST(VanillaSceneryImport, SavedGamesOfLandOneHaveRowsOfTheTables)
{
	const auto files = ForEachLandOneSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& record : world.Records())
		{
			const auto info = world.Word(record, "info");
			if (!info.has_value())
			{
				continue;
			}
			switch (record.type)
			{
			case 0x4F:
				EXPECT_TRUE(world_import::TreeRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x74:
				EXPECT_TRUE(world_import::FeatureRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x75:
				EXPECT_TRUE(world_import::AnimatedStaticRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x53:
				EXPECT_TRUE(world_import::MobileObjectRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x54:
			case 0x6F:
			case 0x70:
			case 0x72:
			case 0xFE:
				EXPECT_TRUE(world_import::MobileStaticRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x1F:
			case 0x59:
			case 0x5A:
				EXPECT_TRUE(world_import::PotRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x5B:
				EXPECT_EQ(*info, world_import::k_FirstFishFarmRowNumber) << path << " " << record.number;
				break;
			case 0x4D:
				EXPECT_TRUE(world_import::BigForestRowOf(*info).has_value()) << path << " " << record.number;
				break;
			case 0x7F:
				EXPECT_TRUE(world_import::TotemStatueRowOf(*info).has_value()) << path << " " << record.number;
				break;
			default:
			{
				const auto animal = std::ranges::find(k_LandOneAnimals, record.type, &std::pair<uint32_t, AnimalInfo>::first);
				if (animal != k_LandOneAnimals.end())
				{
					EXPECT_EQ(world_import::AnimalRowOf(*info), animal->second) << path << " " << record.number;
				}
			}
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0U);
}

/// Each tree of a saved game of Land 1 is in one forest at most, and the import takes every forest and tree
TEST(VanillaSceneryImport, SavedGamesOfLandOneImportTheirForests)
{
	const auto files = ForEachLandOneSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadForests(world);
		EXPECT_TRUE(imported.skipped.empty()) << path;
		EXPECT_EQ(imported.trees.size(), world.OfType(VanillaType::Tree).size()) << path;
		EXPECT_EQ(imported.forests.size(), world.OfType(VanillaType::Forest).size()) << path;
		EXPECT_EQ(imported.bigForests.size(), world.OfType(VanillaType::BigForest).size()) << path;
		std::vector<uint32_t> members;
		for (const auto& forest : imported.forests)
		{
			members.insert(members.end(), forest.grown.begin(), forest.grown.end());
			members.insert(members.end(), forest.growing.begin(), forest.growing.end());
		}
		std::ranges::sort(members);
		EXPECT_EQ(std::ranges::adjacent_find(members), members.end()) << path;
		for (const auto number : members)
		{
			EXPECT_TRUE(std::ranges::any_of(imported.trees,
			                                [number](const world_import::ImportedTree& tree) { return tree.number == number; }))
			    << path << " " << number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0U);
}

TEST(VanillaSceneryImport, ReadsTheFieldsAndTheFishFarmsOfTheFile)
{
	FakeWorld fake;
	fake.Record(VanillaType::Game);
	// 2: a field of the town 9, with its crop and two farmers
	fake.Record(VanillaType::Field)
	    .Position(100, 200)
	    .Value("info", AbodeRowNumber(AbodeInfo::CelticField))
	    .Value("heading", 0.5f)
	    .Ref("town", 9)
	    .Value("cropInfo", world_import::k_FirstFieldTypeRowNumber - static_cast<uint32_t>(FieldTypeInfo::Corn))
	    .Value("food", 200.5f)
	    .Ref("fieldTown", 9)
	    .Value<uint32_t>("growthTurn", 7)
	    .Value<uint8_t>("timesSown", 30)
	    .Value("age", 1202.5f)
	    .Refs("farmers", {11, 12});
	// 3: a fish farm of the town 9 with one fisherman
	fake.Record(VanillaType::FishFarm).Position(300, 400).Refs("fishermen", {11}).Ref("town", 9).Value("fish", 1400.0f);
	// 4: a field with no row of the table of crops
	fake.Record(VanillaType::Field)
	    .Position(0, 0)
	    .Value("info", AbodeRowNumber(AbodeInfo::CelticField))
	    .Value<uint32_t>("cropInfo", 1)
	    .Ref("fieldTown", 9);

	const auto imported = world_import::ReadFarms(fake.Make());
	ASSERT_EQ(imported.fields.size(), 1U);
	const auto& field = imported.fields[0];
	EXPECT_EQ(field.number, 2U);
	EXPECT_EQ(field.row, AbodeInfo::CelticField);
	EXPECT_EQ(field.type, FieldTypeInfo::Corn);
	EXPECT_EQ(field.town, 9U);
	EXPECT_FLOAT_EQ(field.heading, 0.5f);
	EXPECT_FLOAT_EQ(field.food, 200.5f);
	EXPECT_FLOAT_EQ(field.age, 1202.5f);
	EXPECT_EQ(field.timesSown, 30);
	EXPECT_EQ(field.growthTurn, 7U);
	EXPECT_EQ(field.farmers, (std::vector<uint32_t> {11, 12}));
	ASSERT_EQ(imported.fishFarms.size(), 1U);
	EXPECT_EQ(imported.fishFarms[0].town, 9U);
	EXPECT_FLOAT_EQ(imported.fishFarms[0].fish, 1400.0f);
	EXPECT_EQ(imported.fishFarms[0].fishermen, std::vector<uint32_t> {11});
	EXPECT_EQ(imported.fishFarms[0].position.z, 400);
	ASSERT_EQ(imported.skipped.size(), 1U);
	EXPECT_NE(imported.skipped[0].find("object 4"), std::string::npos);
}

/// The import takes each field and fish farm of a saved game of Land 1, and each crop grows on one of ten turns
TEST(VanillaSceneryImport, SavedGamesOfLandOneImportTheirFarms)
{
	const auto files = ForEachLandOneSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadFarms(world);
		EXPECT_TRUE(imported.skipped.empty()) << path;
		EXPECT_EQ(imported.fields.size(), world.OfType(VanillaType::Field).size()) << path;
		EXPECT_EQ(imported.fishFarms.size(), world.OfType(VanillaType::FishFarm).size()) << path;
		for (const auto& field : imported.fields)
		{
			EXPECT_LT(field.growthTurn, 10U) << path << " " << field.number;
			EXPECT_GE(field.food, 0.0f) << path << " " << field.number;
		}
		for (const auto& farm : imported.fishFarms)
		{
			EXPECT_NE(farm.town, 0U) << path << " " << farm.number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0U);
}

/// Each villager of a saved game of Land 1 that keeps its states keeps a goal that metres give again, and its work is a
/// field or a fish farm of the file
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepWalksAndWork)
{
	size_t kept = 0;
	size_t walking = 0;
	size_t working = 0;
	size_t lostGoals = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		for (const auto& villager : imported.villagers)
		{
			if (world_import::StartStates(villager.states, villager.moveState, villager.walk.hugsCircle) != villager.states)
			{
				continue;
			}
			++kept;
			walking += villager.moveState == 0x0B || villager.moveState == 0x0C ? 1 : 0;
			// The import makes a villager that walks decide again when no metre value gives its goal
			if (world_import::UsesWalkGoal(villager.states) &&
			    (!world_import::MetresOfWhole(villager.walk.goal.x).has_value() ||
			     !world_import::MetresOfWhole(villager.walk.goal.z).has_value()))
			{
				++lostGoals;
			}
			auto work = world_import::WorkOf(villager.states[0], villager.topValues);
			if (!work.has_value())
			{
				work = world_import::WorkOf(villager.states[1], villager.finalValues);
			}
			if (work.has_value())
			{
				++working;
				const auto* target = world.Record(work->target);
				ASSERT_NE(target, nullptr) << path << " " << villager.number;
				EXPECT_TRUE(target->Is(VanillaType::Field) || target->Is(VanillaType::FishFarm))
				    << path << " " << villager.number;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(kept, 0U);
	EXPECT_GT(walking, 0U);
	EXPECT_GT(working, 0U);
	EXPECT_LT(lostGoals * 10, kept);
	std::cout << "Kept " << kept << " villagers, " << walking << " walking and " << working << " working, " << lostGoals
	          << " goals that metres do not give" << std::endl;
}
