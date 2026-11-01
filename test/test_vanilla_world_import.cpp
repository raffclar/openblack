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
#include <bit>
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
#include "ECS/VanillaScriptImport.h"
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
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseWonder));
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
	// A villager that goes round a circle or goes out of a circle keeps its walk and its states
	for (const uint8_t moveState : {0x0F, 0x10, 0x11, 0x12})
	{
		EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, moveState), (std::array<uint8_t, 3> {1, k_Chillout, 0}));
	}
	// A move state that the import does not keep makes the villager decide again
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x05), world_import::DecideAgain());
	EXPECT_EQ(world_import::StartStates({1, k_Chillout, 0}, 0x13), world_import::DecideAgain());
	EXPECT_EQ(world_import::DecideAgain(), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	// A state with values that openblack does not keep, at the top or at the end, makes the villager decide again
	EXPECT_EQ(world_import::StartStates({k_OnPath, 0, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_EQ(world_import::StartStates({1, k_OnPath, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::Fishing)));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::FarmerPlantsCrop)));
	EXPECT_FALSE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::InScript)));
}

TEST(VanillaWorldImport, TheCircleOfAWalkIsOnAThingOnTheLandscapeOrOnNothing)
{
	using world_import::CircleKind;
	EXPECT_EQ(world_import::KindOf({.object = 12, .index = -1}), CircleKind::Thing);
	EXPECT_EQ(world_import::KindOf({.object = 12, .index = 1}), CircleKind::Thing);
	EXPECT_EQ(world_import::KindOf({.object = 0, .index = 1}), CircleKind::Landscape);
	EXPECT_EQ(world_import::KindOf({.object = 0, .index = 0}), CircleKind::None);
}

TEST(VanillaWorldImport, AWalkerTakesTheLastCircleOfItsThingAfterALoad)
{
	EXPECT_EQ(world_import::CircleAfterLoad(0), std::nullopt);
	EXPECT_EQ(world_import::CircleAfterLoad(1), 0U);
	// The thing of a temple has 33 circles
	EXPECT_EQ(world_import::CircleAfterLoad(33), 32U);
}

TEST(VanillaWorldImport, TheCircleOfTheLandscapeIsAtTheMiddleOfTheCell)
{
	// A map position in the cell 3, 7, near its far corner
	const auto circle = world_import::LandscapeCircleOf({.x = 3 * 65536 + 65000, .z = 7 * 65536 + 10, .altitude = 4.0f});
	EXPECT_EQ(circle.centre, glm::vec2(35.0f, 75.0f));
	EXPECT_EQ(circle.radius, 7.2f);
	EXPECT_TRUE(circle.landscape);
	EXPECT_TRUE(circle.landscapeOrFence);
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

TEST(VanillaWorldImport, SpeedsGiveTheWholeUnitsOfTheFileAgain)
{
	// Every speed of a walk in whole units a turn
	for (uint32_t whole = 0; whole <= 0xFFFF; ++whole)
	{
		ASSERT_EQ(openblack::ecs::wall_hug::WholeSpeed(world_import::WalkSpeedOf(static_cast<uint16_t>(whole))),
		          static_cast<int32_t>(whole))
		    << whole;
	}
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
		// Each town is in one list of the players
		for (const auto& town : imported.towns)
		{
			EXPECT_EQ(std::ranges::count(imported.townLists, town.number), 1) << entry.path() << " town " << town.id;
			EXPECT_EQ(std::ranges::count(imported.gameTownList, town.number), 1) << entry.path() << " town " << town.id;
		}
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
		// Each player keeps the power of each tribe
		EXPECT_EQ(imported.tribalPowers.size(), read.world->OfType(VanillaType::Player).size()) << entry.path();
	}
	EXPECT_GT(files, 0U);
}

TEST(VanillaWorldImport, ReadsTheGamesListOfAllTowns)
{
	auto fake = GameAtTurn(100);
	// The game object keeps its lists of objects; the list of all the towns holds the town made last first
	for (size_t list = 0; list < world_import::k_GameObjectLists; ++list)
	{
		fake.Refs("lists", list == world_import::k_GameTownList ? std::vector<uint32_t> {4, 2, 3} : std::vector<uint32_t> {});
	}
	for (const uint32_t id : {0U, 1U, 2U})
	{
		fake.Record(VanillaType::Town)
		    .Position(1000 * (id + 1), 1000)
		    .Value<uint32_t>("id", id)
		    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
		    .Value<uint8_t>("player", 0);
	}
	const auto imported = world_import::ReadTowns(fake.Make());
	EXPECT_EQ(imported.gameTownList, (std::vector<uint32_t> {4, 2, 3}));
}

TEST(VanillaWorldImport, AWorldWithoutTheGamesListsHasNoListOfAllTowns)
{
	auto fake = GameAtTurn(100);
	fake.Refs("lists", {2});
	fake.Record(VanillaType::Town)
	    .Position(1000, 1000)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
	    .Value<uint8_t>("player", 0);
	EXPECT_TRUE(world_import::ReadTowns(fake.Make()).gameTownList.empty());
}

TEST(VanillaWorldImport, ReadsTheOrderOfThePlayersTownLists)
{
	auto fake = GameAtTurn(100);
	// 2 and 3: two towns of the first player, 4: a town of the second player
	for (const uint32_t id : {0U, 1U, 2U})
	{
		fake.Record(VanillaType::Town)
		    .Position(1000 * (id + 1), 1000)
		    .Value<uint32_t>("id", id)
		    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
		    .Value<uint8_t>("player", id == 2 ? 1 : 0);
	}
	// 5 and 6: the players. The first player got the town with the higher number first.
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0).Refs("towns", {3, 2});
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Refs("towns", {4});
	const auto world = fake.Make();

	const auto imported = world_import::ReadTowns(world);
	EXPECT_TRUE(imported.skipped.empty());
	EXPECT_EQ(imported.towns.size(), 3U);
	EXPECT_EQ(imported.townLists, (std::vector<uint32_t> {3, 2, 4}));
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

/// Each villager of a saved game of Land 1 that keeps its states keeps its walk, and its work is a field or a fish farm
/// of the file
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepWalksAndWork)
{
	size_t kept = 0;
	size_t walking = 0;
	size_t working = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		for (const auto& villager : imported.villagers)
		{
			if (world_import::StartStates(villager.states, villager.moveState) != villager.states)
			{
				continue;
			}
			++kept;
			walking += villager.moveState >= 0x0B && villager.moveState <= 0x12 ? 1 : 0;
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
	std::cout << "Kept " << kept << " villagers, " << walking << " walking and " << working << " working" << std::endl;
}

/// Each walk of a villager of a saved game of Land 1 that goes straight, goes round a circle or goes out of a circle has
/// its circle, and the thing of a circle is an object of the file
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepTheCirclesOfTheWalks)
{
	size_t onThings = 0;
	size_t round = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& villager : world_import::ReadTowns(world).villagers)
		{
			const bool keepsCircle = villager.moveState >= 0x0C && villager.moveState <= 0x12;
			ASSERT_EQ(villager.walk.circle.has_value(), keepsCircle) << path << " " << villager.number;
			if (!keepsCircle)
			{
				continue;
			}
			round += villager.moveState >= 0x0F ? 1 : 0;
			if (world_import::KindOf(*villager.walk.circle) == world_import::CircleKind::Thing)
			{
				++onThings;
				EXPECT_NE(world.Record(villager.walk.circle->object), nullptr) << path << " " << villager.number;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(onThings, 0U);
	EXPECT_GT(round, 0U);
}

/// Each town of a saved game of Land 1 has its belief, its desires and its running counts. The food and the wood that
/// its villagers hold add up to its counts of food and wood carried, and its desires are sorted in full.
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepTheBeliefAndTheDesiresOfTheTowns)
{
	const auto files = ForEachLandOneSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		// The import takes each flag of a desire of the file, and the flag belongs to a town of the file
		EXPECT_EQ(imported.desireFlags.size(), world.OfType(VanillaType::TownDesireFlags).size()) << path;
		for (const auto& flag : imported.desireFlags)
		{
			EXPECT_NE(std::ranges::find(imported.towns, flag.town, &world_import::ImportedTown::number), imported.towns.end())
			    << path;
		}
		for (const auto& town : imported.towns)
		{
			ASSERT_TRUE(town.belief.has_value()) << path << " " << town.number;
			ASSERT_TRUE(town.desire.has_value()) << path << " " << town.number;
			ASSERT_TRUE(town.tally.has_value()) << path << " " << town.number;
			for (const float cap : town.belief->cap)
			{
				EXPECT_GT(cap, 0.0f) << path << " " << town.number;
			}
			for (const auto& sorted : {town.desire->sorted, town.desire->sortedRaw})
			{
				std::vector<uint32_t> indices;
				std::ranges::transform(sorted, std::back_inserter(indices),
				                       [](const world_import::ImportedDesireSort& entry) { return entry.index; });
				std::ranges::sort(indices);
				for (uint32_t i = 0; i < indices.size(); ++i)
				{
					EXPECT_EQ(indices[i], i) << path << " " << town.number;
				}
			}
			float food = 0.0f;
			float wood = 0.0f;
			for (const auto& villager : imported.villagers)
			{
				if (villager.town == town.number)
				{
					food += static_cast<float>(villager.foodHeld);
					wood += static_cast<float>(villager.woodHeld);
				}
			}
			EXPECT_FLOAT_EQ(town.tally->foodCarried, food) << path << " " << town.number;
			EXPECT_FLOAT_EQ(town.tally->woodCarried, wood) << path << " " << town.number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0U);
}

TEST(VanillaWorldImport, ReadsTheSiteOfATempleWithItsPiles)
{
	auto fake = GameAtTurn(100);
	// 2: the site of the temple, with its building 3 and six piles
	fake.Record(VanillaType::CitadelBuildingSite).Ref("siteBuilding", 3).Refs("siteWorkers", {});
	fake.Value("sitePlaces", std::array<glm::vec3, world_import::k_ImportedSitePlaces> {})
	    .Value("siteWorkerCount", uint32_t {0})
	    .Value("siteDesire", 0.75f)
	    .Ref("siteWoodPile", 0)
	    .Refs("templeSitePiles", {4, 0, 5, 0, 0, 6});
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.sites.size(), 1U);
	const auto& site = imported.sites[0];
	EXPECT_TRUE(site.temple);
	EXPECT_EQ(site.building, 3U);
	EXPECT_EQ(site.desire, 0.75f);
	EXPECT_EQ(site.piles, std::vector<uint32_t>({4, 0, 5, 0, 0, 6}));
}

TEST(VanillaWorldImport, ReadsThePlansAndTheBuildingSitesOfTheTowns)
{
	auto fake = GameAtTurn(100);
	// Record 2: the town, which lists its plan and its site
	fake.Record(VanillaType::Town)
	    .Position(0, 0)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::AZTEC))
	    .Value<uint8_t>("player", 0)
	    .Refs("planned", {3})
	    .Refs("buildingSites", {5});
	fake.Record(VanillaType::PlannedAbode)
	    .Position(10, 20)
	    .Value("heading", 1.5f)
	    .Value("scale", 0.25f)
	    .Value("wasBuilt", uint32_t {1})
	    .Value("info", AbodeRowNumber(AbodeInfo::AztecWonder))
	    .Ref("town", 2);
	// Record 4: the building of the site
	fake.Record(VanillaType::Abode).Position(30, 40).Value("info", AbodeRowNumber(AbodeInfo::AztecHut)).Ref("town", 2);
	fake.Record(VanillaType::StandardBuildingSite).Ref("siteBuilding", 4).Refs("siteWorkers", {7, 7});
	std::array<glm::vec3, world_import::k_ImportedSitePlaces> places {};
	places.at(0) = {1.0f, 2.0f, 3.0f};
	places.back() = {4.0f, 5.0f, 6.0f};
	fake.Value("sitePlaces", places).Value("siteWorkerCount", uint32_t {2}).Value("siteDesire", 0.5f).Ref("siteWoodPile", 0);
	// Record 6: a site without its building
	fake.Record(VanillaType::StandardBuildingSite).Ref("siteBuilding", 0);
	const auto imported = world_import::ReadTowns(fake.Make());

	ASSERT_EQ(imported.towns.size(), 1U);
	EXPECT_EQ(imported.towns[0].planned, std::vector<uint32_t> {3});
	EXPECT_EQ(imported.towns[0].sites, std::vector<uint32_t> {5});
	ASSERT_EQ(imported.plans.size(), 1U);
	const auto& plan = imported.plans[0];
	EXPECT_EQ(plan.number, 3U);
	EXPECT_EQ(plan.row, AbodeInfo::AztecWonder);
	EXPECT_EQ(plan.town, 2U);
	EXPECT_EQ(plan.position.x, 10);
	EXPECT_EQ(plan.position.z, 20);
	EXPECT_FLOAT_EQ(plan.heading, 1.5f);
	EXPECT_FLOAT_EQ(plan.scale, 0.25f);
	EXPECT_TRUE(plan.wasBuilt);
	// The site without its building is not taken
	ASSERT_EQ(imported.sites.size(), 1U);
	const auto& site = imported.sites[0];
	EXPECT_EQ(site.number, 5U);
	EXPECT_EQ(site.building, 4U);
	EXPECT_EQ(site.woodPile, 0U);
	EXPECT_EQ(site.workers, (std::vector<uint32_t> {7, 7}));
	EXPECT_EQ(site.workerCount, 2);
	EXPECT_FLOAT_EQ(site.desire, 0.5f);
	EXPECT_EQ(site.places, places);
	EXPECT_EQ(imported.skipped.size(), 1U);
}

// The flag of a desire keeps its town, its desire and its level. The import does not take a flag with a desire outside the
// table.
TEST(VanillaWorldImport, ReadsTheFlagsOfTheDesiresOfTheTowns)
{
	auto fake = GameAtTurn(100);
	fake.Record(VanillaType::Town)
	    .Position(0, 0)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::AZTEC))
	    .Value<uint8_t>("player", 0);
	fake.Record(VanillaType::TownDesireFlags).Ref("flagTown", 2).Value<uint32_t>("flagType", 3).Value("flagLevel", 0.75f);
	fake.Record(VanillaType::TownDesireFlags)
	    .Ref("flagTown", 2)
	    .Value("flagType", static_cast<uint32_t>(world_import::k_ImportedDesires))
	    .Value("flagLevel", 0.5f);
	const auto imported = world_import::ReadTowns(fake.Make());

	ASSERT_EQ(imported.desireFlags.size(), 1U);
	EXPECT_EQ(imported.desireFlags[0].town, 2U);
	EXPECT_EQ(imported.desireFlags[0].desire, 3U);
	EXPECT_EQ(imported.desireFlags[0].level, 0.75f);
	EXPECT_EQ(imported.skipped.size(), 1U);
}

TEST(VanillaWorldImport, SavedGamesOfLandOneKeepThePlansAndTheBuildingSitesOfTheTowns)
{
	size_t plans = 0;
	size_t sites = 0;
	const auto files = ForEachLandOneSave([&plans, &sites](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		size_t listedPlans = 0;
		size_t listedSites = 0;
		for (const auto& town : imported.towns)
		{
			listedPlans += town.planned.size();
			listedSites += town.sites.size();
		}
		// Each town lists all the plans and the sites of the file
		EXPECT_EQ(listedPlans, imported.plans.size()) << path;
		EXPECT_EQ(listedSites, imported.sites.size()) << path;
		for (const auto& plan : imported.plans)
		{
			EXPECT_GT(plan.scale, 0.0f) << path << " " << plan.number;
		}
		// In these files each site mends a building of a town: no builder works at it yet
		for (const auto& site : imported.sites)
		{
			EXPECT_TRUE(std::ranges::any_of(imported.abodes,
			                                [&site](const world_import::ImportedAbode& abode) {
				                                return abode.number == site.building && abode.built == 1.0f &&
				                                       abode.life < 1.0f;
			                                }))
			    << path << " " << site.number;
			EXPECT_EQ(site.workerCount, static_cast<int32_t>(site.workers.size())) << path << " " << site.number;
		}
		plans += imported.plans.size();
		sites += imported.sites.size();
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_EQ(plans, 54U);
	EXPECT_EQ(sites, 55U);
}

// Each still thing and loose thing of the file comes with the row of its table. A rock and a lantern take the rows of
// the static things, a dead tree the row of its tree, and a lump of poo the row of the loose things.
TEST(VanillaWorldImport, StillAndLooseThingsTakeTheRowsOfTheirTables)
{
	auto world = GameAtTurn(10);
	const auto staticRow = world_import::k_FirstMobileStaticRowNumber;
	world.Record(VanillaType::MobileStatic)
	    .Position(100, 200)
	    .Value("info", staticRow - 3)
	    .Value("heading", 1.5f)
	    .Value("scale", 2.0f)
	    .Value("xAngle", -0.25f)
	    .Value("zAngle", 0.125f);
	world.Record(VanillaType::Rock).Position(300, 400).Value("info", staticRow - 7).Value("life", 0.5f);
	world.Record(VanillaType::StreetLantern).Position(5, 6).Value("info", staticRow - 1);
	world.Record(VanillaType::DeadTree)
	    .Position(7, 8)
	    .Value("info", staticRow - 2)
	    .Value("treeInfo", world_import::k_FirstTreeRowNumber - 4)
	    .Value("woodMultiplier", 0.75f);
	world.Record(VanillaType::Feature).Position(9, 10).Value("info", world_import::k_FirstFeatureRowNumber - 11);
	world.Record(VanillaType::AnimatedStatic).Position(11, 12).Value("info", world_import::k_FirstAnimatedStaticRowNumber);
	world.Record(VanillaType::MobileObject).Position(13, 14).Value("info", world_import::k_FirstMobileObjectRowNumber - 2);
	world.Record(VanillaType::Poo).Position(15, 16).Value("info", world_import::k_FirstMobileObjectRowNumber - 5);
	world.Record(VanillaType::Bonfire).Position(21, 22).Value("info", staticRow - 8);
	world.Record(VanillaType::BaseOnly).Position(23, 24).Value("info", staticRow - 6);
	// A feature with a row of another table, and a tree, which is not a still thing of this list
	world.Record(VanillaType::Feature).Position(17, 18).Value("info", 5u);
	world.Record(VanillaType::Tree).Position(19, 20).Value("info", world_import::k_FirstTreeRowNumber);

	const auto imported = world_import::ReadStatics(world.Make());
	ASSERT_EQ(imported.statics.size(), 10U);
	EXPECT_EQ(imported.skipped.size(), 1U);
	const auto& still = imported.statics[0];
	EXPECT_EQ(still.number, 2U);
	EXPECT_EQ(still.kind, world_import::StaticKind::MobileStatic);
	EXPECT_EQ(still.row, 3U);
	EXPECT_EQ(still.position.x, 100);
	EXPECT_EQ(still.position.z, 200);
	EXPECT_EQ(still.heading, 1.5f);
	EXPECT_EQ(still.scale, 2.0f);
	EXPECT_EQ(still.xAngle, -0.25f);
	EXPECT_EQ(still.zAngle, 0.125f);
	EXPECT_EQ(still.life, 1.0f);
	EXPECT_EQ(imported.statics[1].kind, world_import::StaticKind::Rock);
	EXPECT_EQ(imported.statics[1].row, 7U);
	EXPECT_EQ(imported.statics[1].life, 0.5f);
	EXPECT_EQ(imported.statics[2].kind, world_import::StaticKind::StreetLantern);
	EXPECT_EQ(imported.statics[2].row, 1U);
	EXPECT_EQ(imported.statics[3].kind, world_import::StaticKind::DeadTree);
	EXPECT_EQ(imported.statics[3].row, 4U);
	EXPECT_EQ(imported.statics[3].woodMultiplier, 0.75f);
	EXPECT_EQ(imported.statics[4].kind, world_import::StaticKind::Feature);
	EXPECT_EQ(imported.statics[4].row, 11U);
	EXPECT_EQ(imported.statics[5].kind, world_import::StaticKind::AnimatedStatic);
	EXPECT_EQ(imported.statics[5].row, 0U);
	EXPECT_EQ(imported.statics[6].kind, world_import::StaticKind::MobileObject);
	EXPECT_EQ(imported.statics[6].row, 2U);
	EXPECT_EQ(imported.statics[7].kind, world_import::StaticKind::MobileObject);
	EXPECT_EQ(imported.statics[7].row, 5U);
	EXPECT_EQ(imported.statics[8].kind, world_import::StaticKind::Rock);
	EXPECT_EQ(imported.statics[8].row, 8U);
	EXPECT_EQ(imported.statics[9].kind, world_import::StaticKind::BaseOnly);
	EXPECT_EQ(imported.statics[9].row, 6U);
}

// The game object keeps the clock of day and night: the hour that shows, the seconds of a day, the part of the half day
// that changes, the part that is night, and the rate of the clock
TEST(VanillaWorldImport, TheClockOfDayAndNightComesFromTheGameObject)
{
	auto world = GameAtTurn(10);
	EXPECT_FALSE(world_import::ReadDayCycle(world.Make()).has_value());
	world.Value("visualTime", 22.25f)
	    .Value("dayDuration", 1700.0f)
	    .Value("dayChange", 0.07f)
	    .Value("dayNight", 0.083f)
	    .Value("visualTimeScale", 0.0f);
	const auto cycle = world_import::ReadDayCycle(world.Make());
	ASSERT_TRUE(cycle.has_value());
	EXPECT_EQ(cycle->visualTime, 22.25f);
	EXPECT_EQ(cycle->duration, 1700.0f);
	EXPECT_EQ(cycle->change, 0.07f);
	EXPECT_EQ(cycle->night, 0.083f);
	EXPECT_EQ(cycle->scale, 0.0f);
}

/// Each still thing and loose thing of a saved game of Land 1 has a row of its table, and the clock of each file has the
/// game's cycle
TEST(VanillaWorldImport, SavedGamesOfLandOneHaveRowsForTheirStillThings)
{
	size_t statics = 0;
	const auto files = ForEachLandOneSave([&statics](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadStatics(world);
		EXPECT_TRUE(imported.skipped.empty()) << path << " " << (imported.skipped.empty() ? "" : imported.skipped.front());
		statics += imported.statics.size();
		const auto cycle = world_import::ReadDayCycle(world);
		ASSERT_TRUE(cycle.has_value()) << path;
		EXPECT_EQ(cycle->duration, 1700.0f) << path;
		EXPECT_EQ(cycle->night, 0.083f) << path;
		EXPECT_EQ(cycle->change, 0.07f) << path;
		EXPECT_GE(cycle->visualTime, 0.0f) << path;
		EXPECT_LT(cycle->visualTime, 24.0f) << path;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(statics, 0U);
	std::cout << "Read " << statics << " still and loose things" << std::endl;
}

// A flock keeps its place, the place that it comes back to, its reach, its shepherd and its line. An animal keeps its
// walk and its states, and the towns of the file give the town of each animal and each flock.
TEST(VanillaSceneryImport, ReadsTheAnimalsAndTheFlocksOfTheFile)
{
	constexpr uint32_t k_SheepRow = world_import::k_FirstAnimalRowNumber - static_cast<uint32_t>(AnimalInfo::Sheep);
	constexpr uint32_t k_DoveRow = world_import::k_FirstAnimalRowNumber - static_cast<uint32_t>(AnimalInfo::Dove);
	auto world = GameAtTurn(10);
	// 2: the town, 3: the flock, 4 and 5: its sheep, 6: a dove on its own, 7: a living thing with no animal row
	world.Record(VanillaType::Town).Refs("animals", {4}).Refs("flocks", {3});
	world.Record(VanillaType::Flock)
	    .Position(100, 200)
	    .Ref("shepherd", 0)
	    .Ref("heart", 0)
	    .Refs("members", {5, 4})
	    .Value<uint16_t>("domainRadius", 40)
	    .Value<uint16_t>("flockDistance", 12)
	    .Value("domainCentre", MapCoords {.x = 110, .z = 210, .altitude = 0.0f})
	    .Value<uint32_t>("mostMembers", 4)
	    .Value<uint32_t>("turnsOnLeg", 209)
	    .Value("flockHeight", 12.5f)
	    .Value<uint32_t>("keepsTogether", 1)
	    .Value<uint32_t>("calm", 2)
	    .Value<uint32_t>("flockScriptId", 8);
	std::array<float, 12> bank {};
	bank.at(0) = 0.25f;
	bank.at(1) = 0.5f;
	const auto animal = [&world, &bank](uint32_t row, int32_t x, uint32_t flock) {
		world.Record(static_cast<VanillaType>(0x83))
		    .Position(x, 7)
		    .Value("info", row)
		    .Value("life", 0.5f)
		    .Value("scale", 1.25f)
		    .Value<uint16_t>("speed", 300)
		    .Value<uint16_t>("angle", 512)
		    .Value<uint8_t>("moveState", 0x0B)
		    .Value("goal", MapCoords {.x = x + 50, .z = 9, .altitude = 0.0f})
		    .Value("step", std::array<int32_t, 3> {3, 0, 4})
		    .Value("action", std::array<uint8_t, 6> {2, 1, 0, 0, 0, 0})
		    .Value("birthTurn", 77U)
		    .Ref("flock", flock)
		    .Value<uint16_t>("hunger", 30)
		    .Value<uint16_t>("sleep", 40)
		    .Value<uint16_t>("breed", 50)
		    .Value("sleepCell", std::array<uint16_t, 2> {250, 270})
		    .Value("bank", bank);
	};
	animal(k_SheepRow, 1000, 3);
	animal(k_SheepRow, 2000, 3);
	animal(k_DoveRow, 3000, 0);
	animal(5000, 4000, 0);
	const auto imported = world_import::ReadAnimals(world.Make());
	EXPECT_TRUE(imported.skipped.empty());
	ASSERT_EQ(imported.flocks.size(), 1U);
	const auto& flock = imported.flocks.front();
	EXPECT_EQ(flock.number, 3U);
	EXPECT_EQ(flock.place.x, 100);
	EXPECT_EQ(flock.savedPlace.z, 210);
	EXPECT_EQ(flock.domainRadius, 40U);
	EXPECT_EQ(flock.flockDistance, 12U);
	EXPECT_EQ(flock.town, 2U);
	EXPECT_EQ(flock.members, std::vector<uint32_t>({5, 4}));
	EXPECT_EQ(flock.mostMembers, 4U);
	EXPECT_EQ(flock.turnsOnLeg, 209U);
	EXPECT_EQ(flock.height, 12.5f);
	EXPECT_TRUE(flock.keepsTogether);
	EXPECT_EQ(flock.calm, 2);
	EXPECT_EQ(flock.scriptId, 8);
	ASSERT_EQ(imported.animals.size(), 3U);
	const auto& sheep = imported.animals.front();
	EXPECT_EQ(sheep.number, 4U);
	EXPECT_EQ(sheep.row, AnimalInfo::Sheep);
	EXPECT_EQ(sheep.position.x, 1000);
	EXPECT_EQ(sheep.life, 0.5f);
	EXPECT_EQ(sheep.scale, 1.25f);
	EXPECT_EQ(sheep.birthTurn, 77U);
	EXPECT_EQ(sheep.goal.x, 1050);
	EXPECT_EQ(sheep.stepX, 3);
	EXPECT_EQ(sheep.stepZ, 4);
	EXPECT_EQ(sheep.speed, 300U);
	EXPECT_EQ(sheep.angle, 512U);
	EXPECT_EQ(sheep.moveState, 0x0BU);
	EXPECT_EQ(sheep.state, 2U);
	EXPECT_EQ(sheep.finalState, 1U);
	EXPECT_EQ(sheep.flock, 3U);
	EXPECT_EQ(sheep.town, 2U);
	EXPECT_EQ(sheep.hunger, 30);
	EXPECT_EQ(sheep.sleep, 40);
	EXPECT_EQ(sheep.breed, 50);
	EXPECT_EQ(sheep.sleepCell, glm::ivec2(250, 270));
	EXPECT_EQ(sheep.bank.value, 0.25f);
	EXPECT_EQ(sheep.bank.target, 0.5f);
	EXPECT_EQ(imported.animals[1].town, 0U);
	EXPECT_EQ(imported.animals[2].row, AnimalInfo::Dove);
	EXPECT_EQ(imported.animals[2].flock, 0U);
}

// Each float of a glide of the file goes to its value of the zoomer. Nothing reads the fifth float.
TEST(VanillaSceneryImport, AGlideOfTheFileGivesEachValueOfTheZoomer)
{
	std::array<float, 12> floats {1.0f, 2.0f, 3.0f, 4.0f, 99.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
	std::array<uint8_t, world_import::k_GlideBytes> bytes {};
	std::memcpy(bytes.data(), floats.data(), bytes.size());
	const auto glide = world_import::GlideOf(bytes);
	EXPECT_EQ(glide.value, 1.0f);
	EXPECT_EQ(glide.target, 2.0f);
	EXPECT_EQ(glide.targetSpeed, 3.0f);
	EXPECT_EQ(glide.speed, 4.0f);
	EXPECT_EQ(glide.elapsed, 6.0f);
	EXPECT_EQ(glide.duration, 7.0f);
	EXPECT_EQ(glide.start, 8.0f);
	EXPECT_EQ(glide.startSpeed, 9.0f);
	EXPECT_EQ(glide.c2, 10.0f);
	EXPECT_EQ(glide.c3, 11.0f);
	EXPECT_EQ(glide.c4, 12.0f);
}

// A cell of the file is its two halves. Two halves of 0 are no cell.
TEST(VanillaSceneryImport, ACellOfTheFileIsNothingWhenBothHalvesAreZero)
{
	std::array<uint8_t, 4> bytes {0xFA, 0x00, 0x0E, 0x01};
	EXPECT_EQ(world_import::CellOf(bytes), glm::ivec2(250, 270));
	EXPECT_FALSE(world_import::CellOf(std::array<uint8_t, 4> {}).has_value());
}

/// Each animal and each flock of a saved game of Land 1 reads whole. Each member of a flock is a villager, or an animal
/// of the file that has the flock as its flock.
TEST(VanillaSceneryImport, SavedGamesOfLandOneHaveTheirAnimalsInTheirFlocks)
{
	size_t animals = 0;
	size_t flocks = 0;
	size_t villagers = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadAnimals(world);
		EXPECT_TRUE(imported.skipped.empty()) << path << " " << (imported.skipped.empty() ? "" : imported.skipped.front());
		animals += imported.animals.size();
		flocks += imported.flocks.size();
		for (const auto& flock : imported.flocks)
		{
			for (const auto member : flock.members)
			{
				// A villager can also be a member of a flock
				const auto* record = world.Record(member);
				ASSERT_NE(record, nullptr) << path << " flock " << flock.number;
				if (world.Field(*record, "villagerFlags") != nullptr)
				{
					++villagers;
					continue;
				}
				const auto found = std::ranges::find(imported.animals, member, &world_import::ImportedAnimal::number);
				ASSERT_NE(found, imported.animals.end()) << path << " flock " << flock.number;
				EXPECT_EQ(found->flock, flock.number) << path << " animal " << member;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(animals, 0U);
	EXPECT_GT(flocks, 0U);
	std::cout << "Read " << animals << " animals and " << flocks << " flocks, with " << villagers << " villagers in flocks"
	          << std::endl;
}

namespace script_import = openblack::ecs::vanilla_script_import;

// The rows of the highlights count back from their first row, as the rows of the other tables
TEST(VanillaScriptImport, HighlightRowsCountBackFromTheFirstRow)
{
	EXPECT_EQ(script_import::HighlightRowOf(2088), HighlightInfo::Scroll);
	EXPECT_EQ(script_import::HighlightRowOf(2087), HighlightInfo::DidYouKnowSign);
	EXPECT_EQ(script_import::HighlightRowOf(2086), HighlightInfo::Silver);
	EXPECT_EQ(script_import::HighlightRowOf(2085), HighlightInfo::Gold);
	EXPECT_FALSE(script_import::HighlightRowOf(2089).has_value());
	EXPECT_FALSE(script_import::HighlightRowOf(2084).has_value());
}

// A marker keeps its place, a timer its turns, a highlight its row and its values, and an influence its player, its
// reach and the object that it goes with. The heart of a temple belongs to the owner of the temple.
TEST(VanillaScriptImport, ReadsTheThingsOfTheScripts)
{
	auto world = GameAtTurn(10);
	// 2: a player, 3: a temple, 4: its heart
	world.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	world.Record(VanillaType::Citadel).Position(0, 0).Ref("owner", 2).Ref("heart", 4);
	world.Record(VanillaType::CitadelHeart).Position(0, 0);
	// 5: a marker, 6: a timer, 7: a gold scroll with a height, 8: an influence that goes with the marker
	world.Record(VanillaType::ScriptMarker).Position(10, 20);
	world.Record(VanillaType::ScriptTimer).Position(0, 0).Value("timerSetTurn", 300U).Value("timerTurns", 40U);
	world.Record(VanillaType::ScriptHighlight)
	    .Position(30, 40)
	    .Value("info", 2085U)
	    .Value("highlightActive", 1U)
	    .Value("highlightHeightSet", 1U)
	    .Value("highlightHeight", 2.5f)
	    .Value("highlightText", 12U)
	    .Value("highlightCategory", 3U);
	world.Record(VanillaType::InfluenceRing)
	    .Position(50, 60)
	    .Ref("follows", 5)
	    .Ref("player", 2)
	    .Value("radius", 25.0f)
	    .Value("anti", 1U);
	// 9: a marker with no place
	world.Record(VanillaType::ScriptMarker);
	const auto made = world.Make();
	const auto things = script_import::ReadScriptThings(made);
	ASSERT_EQ(things.markers.size(), 1U);
	EXPECT_EQ(things.markers[0].number, 5U);
	EXPECT_EQ(things.markers[0].position.z, 20);
	EXPECT_EQ(things.skipped.size(), 1U);
	ASSERT_EQ(things.timers.size(), 1U);
	EXPECT_EQ(things.timers[0].setTurn, 300U);
	EXPECT_EQ(things.timers[0].turns, 40);
	ASSERT_EQ(things.highlights.size(), 1U);
	const auto& highlight = things.highlights[0];
	EXPECT_EQ(highlight.kind, HighlightInfo::Gold);
	EXPECT_TRUE(highlight.active);
	EXPECT_EQ(highlight.drawHeight, std::optional(2.5f));
	EXPECT_EQ(highlight.text, 12U);
	EXPECT_EQ(highlight.category, 3U);
	ASSERT_EQ(things.rings.size(), 1U);
	EXPECT_EQ(things.rings[0].follows, 5U);
	EXPECT_EQ(things.rings[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(things.rings[0].radius, 25.0f);
	EXPECT_TRUE(things.rings[0].anti);

	EXPECT_EQ(script_import::HeartPlayer(made, 4), PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(script_import::HeartPlayer(made, 5).has_value());
	EXPECT_FALSE(script_import::HeartPlayer(made, 0).has_value());
}

// A temple of the file with no site stands whole
TEST(VanillaScriptImport, ATempleOfTheFileWithNoSiteStandsWhole)
{
	auto fake = GameAtTurn(100);
	// 2: the second player, 3: its temple
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	fake.Record(VanillaType::Citadel).Ref("owner", 2).Ref("heart", 0);
	const auto temples = script_import::ReadTemples(fake.Make());
	ASSERT_EQ(temples.size(), 1U);
	EXPECT_EQ(temples[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(temples[0].citadel, 3U);
	EXPECT_TRUE(temples[0].Whole());
}

// The heart of a temple that goes up owns the site of the temple, and keeps its life, its last hit, its town, its last
// beam and the clocks of its destruction
TEST(VanillaScriptImport, ATempleOfTheFileTakesTheValuesOfItsHeart)
{
	auto fake = GameAtTurn(100);
	// 2: the first player, 3: its temple, 4: the heart, 5: the site, 6: the town
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0);
	fake.Record(VanillaType::Citadel).Ref("owner", 2).Ref("heart", 4);
	fake.Record(VanillaType::CitadelHeart)
	    .Value("built", 0.25f)
	    .Value("life", 0.5f)
	    .Ref("buildingSite", 5)
	    .Value<uint32_t>("heartLastHitTurn", 70)
	    .Ref("heartTown", 6)
	    .Value<uint8_t>("heartDestroying", 1)
	    .Value("heartDestructionClock", 2.5f)
	    .Value("heartDestructionBeamClock", 3.0f)
	    .Ref("heartBeamTarget", 6)
	    .Value<uint32_t>("heartBeamTurn", 80);
	fake.Record(VanillaType::CitadelBuildingSite);
	fake.Record(VanillaType::Town);
	const auto temples = script_import::ReadTemples(fake.Make());
	ASSERT_EQ(temples.size(), 1U);
	const auto& temple = temples[0];
	EXPECT_EQ(temple.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(temple.heart, 4U);
	EXPECT_EQ(temple.site, 5U);
	EXPECT_FALSE(temple.Whole());
	EXPECT_EQ(temple.built, 0.25f);
	EXPECT_EQ(temple.life, 0.5f);
	EXPECT_EQ(temple.lastHitTurn, 70U);
	EXPECT_EQ(temple.town, 6U);
	EXPECT_TRUE(temple.destroying);
	EXPECT_EQ(temple.destructionClock, 2.5f);
	EXPECT_EQ(temple.destructionBeamClock, 3.0f);
	EXPECT_EQ(temple.beamTarget, 6U);
	EXPECT_EQ(temple.beamTurn, 80U);
}

/// Each saved game of Land 1 has the whole temple of the first player
TEST(VanillaScriptImport, SavedGamesOfLandOneHaveTheWholeTempleOfThePlayer)
{
	const auto files = ForEachLandOneSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto temples = script_import::ReadTemples(world);
		ASSERT_EQ(temples.size(), 1U) << path;
		EXPECT_EQ(temples[0].player, PlayerNames::PLAYER_ONE) << path;
		EXPECT_TRUE(temples[0].Whole()) << path;
		EXPECT_FALSE(temples[0].destroying) << path;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0U);
}

TEST(VanillaWorldImport, ReadsThePowerOfEachWonderAndTheTribalPowersOfEachPlayer)
{
	auto fake = GameAtTurn(100);
	// 2: a town of the second player, 3: its wonder, 4: a hut
	fake.Record(VanillaType::Town)
	    .Position(1000, 1000)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::GREEK))
	    .Value<uint8_t>("player", 1);
	fake.Record(VanillaType::Wonder)
	    .Position(1200, 1000)
	    .Value<uint32_t>("info", AbodeRowNumber(AbodeInfo::GreekWonder))
	    .Ref("town", 2)
	    .Value<float>("wonderPower", 3.75f);
	fake.Record(VanillaType::Abode)
	    .Position(1400, 1000)
	    .Value<uint32_t>("info", AbodeRowNumber(AbodeInfo::GreekHut))
	    .Ref("town", 2);
	// 5: the second player. Its Greek power has the wonder's power on top of the usual 1.
	std::vector<uint32_t> power(9, std::bit_cast<uint32_t>(1.0f));
	power.at(static_cast<size_t>(Tribe::GREEK)) = std::bit_cast<uint32_t>(4.75f);
	std::vector<uint32_t> most = power;
	most.at(static_cast<size_t>(Tribe::CELTIC)) = std::bit_cast<uint32_t>(2.5f);
	fake.Record(VanillaType::Player)
	    .Value<uint8_t>("number", 1)
	    .Words("tribalPower", power)
	    .Words("maxTribalPower", most)
	    .Refs("towns", {2});
	// 6: a player with a list of another length is not taken
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 2).Words("tribalPower", {1, 2}).Words("maxTribalPower", {1, 2});

	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.abodes.size(), 2U);
	ASSERT_TRUE(imported.abodes[0].wonderPower.has_value());
	EXPECT_EQ(*imported.abodes[0].wonderPower, 3.75f);
	EXPECT_FALSE(imported.abodes[1].wonderPower.has_value());
	ASSERT_EQ(imported.tribalPowers.size(), 1U);
	const auto& player = imported.tribalPowers[0];
	EXPECT_EQ(player.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(player.power.at(static_cast<size_t>(Tribe::GREEK)), 4.75f);
	EXPECT_EQ(player.power.at(static_cast<size_t>(Tribe::CELTIC)), 1.0f);
	EXPECT_EQ(player.most.at(static_cast<size_t>(Tribe::CELTIC)), 2.5f);
	EXPECT_EQ(player.most.at(static_cast<size_t>(Tribe::TIBETAN)), 1.0f);
}

// An animal that goes round a circle keeps its circle and the height of its step. Each member of a flock keeps its place
// in the line.
TEST(VanillaSceneryImport, ReadsTheCircleOfAnAnimalAndThePlaceInTheLineOfEachMember)
{
	constexpr uint32_t k_SheepRow = world_import::k_FirstAnimalRowNumber - static_cast<uint32_t>(AnimalInfo::Sheep);
	auto world = GameAtTurn(10);
	// 2: the flock, 3: a sheep that goes round a tree, 4: a sheep that steps straight through
	world.Record(VanillaType::Flock)
	    .Position(100, 200)
	    .Ref("shepherd", 0)
	    .Ref("heart", 0)
	    .Refs("members", {3, 4})
	    .Value<uint16_t>("domainRadius", 40)
	    .Value<uint16_t>("flockDistance", 12)
	    .Value("domainCentre", MapCoords {.x = 100, .z = 200, .altitude = 0.0f})
	    .Value<uint32_t>("mostMembers", 2);
	const auto sheep = [&world](uint8_t moveState, uint8_t order) {
		world.Record(static_cast<VanillaType>(0x83))
		    .Position(1000, 7)
		    .Value("info", k_SheepRow)
		    .Value("life", 1.0f)
		    .Value<uint8_t>("moveState", moveState)
		    .Value("step", std::array<uint32_t, 3> {3, std::bit_cast<uint32_t>(2.5f), 4})
		    .Value("action", std::array<uint8_t, 6> {2, 1, 0, 0, 0, 0})
		    .Value("birthTurn", 5U)
		    .Ref("flock", 2)
		    .Value<uint8_t>("flockOrder", order);
		if (moveState >= 0x0C && moveState <= 0x12)
		{
			world.Value<uint8_t>("hugTurnsToObject", 6)
			    .Value<uint32_t>("hugEntryDistance", 9)
			    .Ref("hugObject", 4)
			    .Value<uint32_t>("hugIndex", 2);
		}
	};
	sheep(0x0E, 0);
	sheep(0x0B, 3);
	const auto imported = world_import::ReadAnimals(world.Make());
	ASSERT_EQ(imported.flocks.size(), 1U);
	EXPECT_EQ(imported.flocks[0].memberOrders, std::vector<uint8_t>({0, 3}));
	ASSERT_EQ(imported.animals.size(), 2U);
	const auto& round = imported.animals[0];
	EXPECT_EQ(round.stepAltitude, 2.5f);
	ASSERT_TRUE(round.circle.has_value());
	EXPECT_EQ(round.circle->turnsToObject, 6U);
	EXPECT_EQ(round.circle->entryDistance, 9U);
	EXPECT_EQ(round.circle->object, 4U);
	EXPECT_EQ(round.circle->index, 2);
	EXPECT_FALSE(imported.animals[1].circle.has_value());
}

// The load puts each member before the first member whose place is not lower, so members with the same place come back
// in the opposite order
TEST(VanillaSceneryImport, TheLineOfAFlockAfterALoadFollowsThePlaces)
{
	EXPECT_TRUE(world_import::LineAfterLoad({}).empty());
	EXPECT_EQ(world_import::LineAfterLoad(std::vector<uint8_t> {0, 0, 0}), std::vector<size_t>({2, 1, 0}));
	EXPECT_EQ(world_import::LineAfterLoad(std::vector<uint8_t> {5, 1, 3}), std::vector<size_t>({1, 2, 0}));
	EXPECT_EQ(world_import::LineAfterLoad(std::vector<uint8_t> {2, 0, 2, 1}), std::vector<size_t>({1, 3, 2, 0}));
}
