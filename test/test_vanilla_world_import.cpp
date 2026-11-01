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
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <VanillaWorld.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/VanillaFootpathImport.h"
#include "ECS/VanillaParticleImport.h"
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
	constexpr auto k_Inspect = static_cast<uint8_t>(VillagerStates::InspectObjectReaction);
	EXPECT_EQ(world_import::StartStates({k_Inspect, 0, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	EXPECT_EQ(world_import::StartStates({1, k_Inspect, 0}, 0x01), (std::array<uint8_t, 3> {k_Decide, k_Invalid, k_Invalid}));
	// A walk on a footpath keeps its states: the footpaths of the file give its values
	EXPECT_EQ(world_import::StartStates({k_OnPath, k_Chillout, 0}, 0x0B), (std::array<uint8_t, 3> {k_OnPath, k_Chillout, 0}));
	EXPECT_TRUE(world_import::CanKeepStateValues(k_OnPath));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::Fishing)));
	EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::FarmerPlantsCrop)));
	EXPECT_FALSE(world_import::CanKeepStateValues(k_Inspect));
	// The states of a shepherd, the script states, the dead state and a disciple with nothing to do keep their values
	for (const auto state : {VillagerStates::ShepherdLookForFlock, VillagerStates::ShepherdGotoFlock,
	                         VillagerStates::ShepherdCheckAnimalForSlaughter, VillagerStates::InScript,
	                         VillagerStates::ScriptWanderAroundPosition, VillagerStates::ScriptPlayAnim,
	                         VillagerStates::ScriptInCrowd, VillagerStates::Dead, VillagerStates::DiscipleNothingToDo})
	{
		EXPECT_TRUE(world_import::CanKeepStateValues(static_cast<uint8_t>(state))) << static_cast<int>(state);
	}
	// The states before and after death that keep the reason are not kept yet
	EXPECT_FALSE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::SetDying)));
	EXPECT_FALSE(world_import::CanKeepStateValues(static_cast<uint8_t>(VillagerStates::Dying)));
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
	    .Value<uint32_t>("followState", 45)
	    .Value<uint32_t>("afterMove", 43)
	    .Value<uint32_t>("followMode", 3)
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
	EXPECT_EQ(flock.followState, 45U);
	EXPECT_EQ(flock.afterMove, 43U);
	EXPECT_EQ(flock.followMode, 3);
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

// Each totem of a Land 1 save keeps every value of the file, and stands on a town centre that the import makes
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepEveryValueOfTheirTotems)
{
	uint32_t totems = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		std::vector<const VanillaRecord*> records;
		for (const auto& record : world.Records())
		{
			if (record.Is(VanillaType::TotemStatue))
			{
				records.push_back(&record);
			}
		}
		ASSERT_EQ(imported.totems.size(), records.size()) << path;
		for (size_t i = 0; i < records.size(); ++i)
		{
			const auto& record = *records[i];
			const auto& totem = imported.totems[i];
			EXPECT_EQ(totem.number, record.number) << path;
			EXPECT_EQ(totem.townCentre, world.Ref(record, "totemTownCentre")) << path;
			const auto place = world.Raw(record, "position");
			ASSERT_EQ(place.size(), sizeof(totem.position)) << path;
			EXPECT_EQ(std::memcmp(place.data(), &totem.position, sizeof(totem.position)), 0) << path;
			EXPECT_EQ(totem.heading, world.Float(record, "heading")) << path;
			EXPECT_EQ(totem.scale, world.Float(record, "scale")) << path;
			EXPECT_EQ(totem.life, world.Float(record, "life")) << path;
			EXPECT_EQ(totem.share, world.Float(record, "totemShare")) << path;
			EXPECT_EQ(totem.heldShare, world.Float(record, "totemHeldShare")) << path;
			EXPECT_EQ(totem.word84, world.Word(record, "totemWord84")) << path;
			const auto* centre = world.Record(totem.townCentre);
			ASSERT_NE(centre, nullptr) << path;
			EXPECT_TRUE(centre->Is(VanillaType::TownCentre)) << path;
			// The totem has the scale of its town centre, as the totem that the town centre makes has
			EXPECT_EQ(totem.scale, world.Float(*centre, "scale")) << path;
			EXPECT_NE(std::ranges::find(imported.abodes, totem.townCentre, &world_import::ImportedAbode::number),
			          imported.abodes.end())
			    << path;
			std::cout << path.parent_path().filename().string() << " totem " << totem.number << " share " << totem.share
			          << " held " << totem.heldShare << " word " << totem.word84 << " life " << totem.life << " heading "
			          << totem.heading << std::endl;
			++totems;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(totems, 0U);
}

// Each highlight of a Land 1 save keeps every value of the file: its row, its place, the active value, the height,
// the text and the category. The import gives these values to the highlight before its creation step.
TEST(VanillaScriptImport, SavedGamesOfLandOneKeepEveryValueOfTheirHighlights)
{
	uint32_t highlights = 0;
	uint32_t active = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto things = script_import::ReadScriptThings(world);
		std::vector<const VanillaRecord*> records;
		for (const auto& record : world.Records())
		{
			if (record.Is(VanillaType::ScriptHighlight))
			{
				records.push_back(&record);
			}
		}
		ASSERT_EQ(things.highlights.size(), records.size()) << path;
		for (size_t i = 0; i < records.size(); ++i)
		{
			const auto& record = *records[i];
			const auto& highlight = things.highlights[i];
			EXPECT_EQ(highlight.number, record.number) << path;
			EXPECT_EQ(highlight.kind, script_import::HighlightRowOf(world.Word(record, "info").value_or(0))) << path;
			const auto place = world.Raw(record, "position");
			ASSERT_EQ(place.size(), sizeof(highlight.position)) << path;
			EXPECT_EQ(std::memcmp(place.data(), &highlight.position, sizeof(highlight.position)), 0) << path;
			EXPECT_EQ(highlight.active, world.Word(record, "highlightActive").value_or(0) != 0) << path;
			EXPECT_EQ(highlight.drawHeight.has_value(), world.Word(record, "highlightHeightSet").value_or(0) != 0) << path;
			if (highlight.drawHeight.has_value())
			{
				EXPECT_EQ(highlight.drawHeight, world.Float(record, "highlightHeight")) << path;
			}
			EXPECT_EQ(highlight.text, world.Word(record, "highlightText")) << path;
			EXPECT_EQ(highlight.category, world.Word(record, "highlightCategory")) << path;
			++highlights;
			if (highlight.active)
			{
				++active;
				std::cout << "Active highlight " << highlight.number << " of row " << static_cast<int>(highlight.kind) << " in "
				          << path.generic_string() << std::endl;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(highlights, 0U);
	std::cout << *files << " Land 1 saves: " << highlights << " highlights, " << active << " active" << std::endl;
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

// A pile of a storage pit keeps its row, its amount, its poison and its rise. A pile of another building is not a pile
// of the storage pit.
TEST(VanillaWorldImport, ReadsThePilesOfAStoragePit)
{
	constexpr auto k_PotRow = [](PotInfo row) { return world_import::k_FirstPotRowNumber - static_cast<uint32_t>(row); };
	auto fake = GameAtTurn(100);
	// 2: the town, 3: its storage pit, 4: the food pile of the pit, 5: a wood pile of the pit, 6: a pile of building 7
	fake.Record(VanillaType::Town).Position(0, 0).Value<uint32_t>("id", 0).Refs("abodes", {3});
	fake.Record(VanillaType::StoragePit)
	    .Position(1000, 2000)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseStoragePit))
	    .Ref("town", 2)
	    .Refs("villagers", {})
	    .Words("resources", {300, 5000});
	std::array<float, 12> rise {};
	for (size_t i = 0; i < rise.size(); ++i)
	{
		rise.at(i) = static_cast<float>(i) + 0.5f;
	}
	const auto pile = [&fake, &rise](VanillaType type, PotInfo row, uint32_t building, uint32_t amount, uint8_t flags) {
		fake.Record(type)
		    .Position(1000, 2000)
		    .Value("info", k_PotRow(row))
		    .Value<uint32_t>("potAmount", amount)
		    .Value<uint8_t>("potFlags", flags)
		    .Ref("potBuilding", building)
		    .Value("pileRise", rise);
	};
	pile(VanillaType::PileFood, PotInfo::StoragePitFoodPile, 3, 300, 1);
	pile(VanillaType::PileResource, PotInfo::WoodPile_2, 3, 5000, 0);
	pile(VanillaType::PileResource, PotInfo::WoodPile_1, 7, 70, 0);
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.abodes.size(), 1U);
	const auto& piles = imported.abodes[0].piles;
	ASSERT_EQ(piles.size(), 2U);
	EXPECT_EQ(piles[0].number, 4U);
	EXPECT_EQ(piles[0].row, PotInfo::StoragePitFoodPile);
	EXPECT_EQ(piles[0].amount, 300U);
	EXPECT_TRUE(piles[0].poisoned);
	EXPECT_EQ(piles[1].row, PotInfo::WoodPile_2);
	EXPECT_EQ(piles[1].amount, 5000U);
	EXPECT_FALSE(piles[1].poisoned);
	EXPECT_EQ(piles[1].rise.offset, 0.5f);
	EXPECT_EQ(piles[1].rise.target, 1.5f);
	EXPECT_EQ(piles[1].rise.currentSpeed, 3.5f);
	EXPECT_EQ(piles[1].rise.time, 5.5f);
	EXPECT_EQ(piles[1].rise.duration, 6.5f);
	EXPECT_EQ(piles[1].rise.start, 7.5f);
	EXPECT_EQ(piles[1].rise.speed, 8.5f);
	EXPECT_EQ(piles[1].rise.acceleration, 9.5f);
	EXPECT_EQ(piles[1].rise.jerk, 10.5f);
	EXPECT_EQ(piles[1].rise.snap, 11.5f);
}

// A building site takes the values of its wood pile, and the site of a temple the values of each of its piles, nothing
// for a pile that has gone
TEST(VanillaWorldImport, ReadsThePilesOfTheBuildingSites)
{
	constexpr uint32_t k_MagicWoodRow = world_import::k_FirstPotRowNumber - static_cast<uint32_t>(PotInfo::MagicWood);
	auto fake = GameAtTurn(100);
	// 2: a site with its wood pile 4, 3: the site of a temple with its piles 5 and none
	fake.Record(VanillaType::StandardBuildingSite)
	    .Ref("siteBuilding", 6)
	    .Refs("siteWorkers", {})
	    .Value("sitePlaces", std::array<glm::vec3, world_import::k_ImportedSitePlaces> {})
	    .Ref("siteWoodPile", 4);
	fake.Record(VanillaType::CitadelBuildingSite)
	    .Ref("siteBuilding", 7)
	    .Refs("siteWorkers", {})
	    .Value("sitePlaces", std::array<glm::vec3, world_import::k_ImportedSitePlaces> {})
	    .Ref("siteWoodPile", 0)
	    .Refs("templeSitePiles", {5, 0});
	for (const uint32_t amount : {120U, 40U})
	{
		fake.Record(VanillaType::MagicWood)
		    .Position(static_cast<int32_t>(amount) * 1000, 2000)
		    .Value("info", k_MagicWoodRow)
		    .Value("heading", 1.5f)
		    .Value<uint32_t>("potAmount", amount)
		    .Value<uint8_t>("potFlags", 0)
		    .Ref("potBuilding", 0)
		    .Value("pileRise", std::array<float, 12> {});
	}
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.sites.size(), 2U);
	const auto& site = imported.sites[0];
	ASSERT_TRUE(site.woodPileValues.has_value());
	EXPECT_EQ(site.woodPileValues->number, 4U);
	EXPECT_EQ(site.woodPileValues->row, PotInfo::MagicWood);
	EXPECT_EQ(site.woodPileValues->amount, 120U);
	EXPECT_EQ(site.woodPileValues->position.x, 120000);
	EXPECT_EQ(site.woodPileValues->heading, 1.5f);
	const auto& temple = imported.sites[1];
	EXPECT_FALSE(temple.woodPileValues.has_value());
	ASSERT_EQ(temple.pileValues.size(), 2U);
	ASSERT_TRUE(temple.pileValues[0].has_value());
	EXPECT_EQ(temple.pileValues[0]->amount, 40U);
	EXPECT_FALSE(temple.pileValues[1].has_value());
}

// Each storage pit of a saved game of Land 1 has one pile of food and five piles of wood, and its piles hold the food
// and the wood of the storage pit
TEST(VanillaWorldImport, SavedGamesOfLandOneKeepThePilesOfTheirStoragePits)
{
	size_t pits = 0;
	const auto files = ForEachLandOneSave([&pits](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& abode : world_import::ReadTowns(world).abodes)
		{
			if (abode.piles.empty())
			{
				continue;
			}
			++pits;
			ASSERT_EQ(abode.piles.size(), 6U) << path << " " << abode.number;
			uint32_t food = 0;
			uint32_t wood = 0;
			for (const auto& pile : abode.piles)
			{
				(pile.row == PotInfo::StoragePitFoodPile ? food : wood) += pile.amount;
				EXPECT_FALSE(pile.poisoned) << path << " " << pile.number;
				EXPECT_EQ(pile.rise.duration, 1.0f) << path << " " << pile.number;
			}
			EXPECT_EQ(food, abode.food) << path << " " << abode.number;
			EXPECT_EQ(wood, abode.wood) << path << " " << abode.number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(pits, 0U);
}

// Every flock of a saved game of Land 1 keeps the follow values of a new flock or of a flock of birds whose leader
// wanders, and no flock keeps a thing
TEST(VanillaSceneryImport, SavedGamesOfLandOneKeepTheFollowValuesOfTheirFlocks)
{
	size_t birds = 0;
	const auto files = ForEachLandOneSave([&birds](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& flock : world_import::ReadAnimals(world).flocks)
		{
			const auto* record = world.Record(flock.number);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(world.Ref(*record, "flockThing"), std::optional<uint32_t> {0}) << path << " " << flock.number;
			if (flock.followState == 45)
			{
				++birds;
				EXPECT_EQ(flock.afterMove, 43U) << path << " " << flock.number;
				EXPECT_EQ(flock.followMode, 3) << path << " " << flock.number;
				continue;
			}
			EXPECT_EQ(flock.followState, 43U) << path << " " << flock.number;
			EXPECT_EQ(flock.followMode, 0) << path << " " << flock.number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(birds, 0U);
}

namespace
{
namespace particle_import = openblack::ecs::vanilla_particle_import;

/// Every object of a saved game becomes an entity of the same number, so that a test sees which object a value holds
entt::entity SameNumber(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

/// A value of a record as the file keeps it, from its bytes
template <typename T>
T FileValue(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	T value {};
	const auto bytes = world.Raw(record, name);
	EXPECT_GE(bytes.size(), sizeof(T)) << record.number << " " << name;
	if (bytes.size() >= sizeof(T))
	{
		std::memcpy(&value, bytes.data(), sizeof(T));
	}
	return value;
}

glm::vec3 FilePoint(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	const auto point = FileValue<std::array<float, 3>>(world, record, name);
	return {point[0], point[1], point[2]};
}

/// The values of the file that the import took, with how many of each group it checked
struct Checked
{
	uint32_t containers {0};
	uint32_t effects {0};
	uint32_t collections {0};
	uint32_t atoms {0};
	uint32_t emitters {0};
};

void CheckAtom(const VanillaWorld& world, const VanillaRecord& record, const particles::RestoredAtom& atom, Checked& checked);

/// The collection of the import agrees with each value of its record
void CheckCollection(const VanillaWorld& world, uint32_t number, const particles::RestoredCollection& collection,
                     Checked& checked)
{
	const auto* record = world.Record(number);
	ASSERT_NE(record, nullptr);
	ASSERT_TRUE(record->Is(VanillaType::AtomCollection));
	++checked.collections;
	EXPECT_EQ(collection.group, static_cast<int>(FileValue<uint32_t>(world, *record, "collectionGroup")));
	EXPECT_EQ(collection.birth, FileValue<float>(world, *record, "collectionBirth"));
	EXPECT_EQ(collection.alpha, static_cast<float>(FileValue<uint8_t>(world, *record, "collectionAlpha")));
	const auto flags = FileValue<uint8_t>(world, *record, "collectionFlags");
	EXPECT_EQ(collection.hierarchy, (flags & 1U) == 0);
	EXPECT_EQ(collection.interpolated, (flags & 2U) != 0);
	EXPECT_EQ(collection.emitting, (flags & 4U) != 0);
	// A collection is in a hierarchy when the group of an atom over it is a hierarchy, as the file's hierarchy value says
	if (const auto* parent = world.Record(world.Ref(*record, "collectionParent").value_or(0)); parent != nullptr)
	{
		const auto* above = world.Record(world.Ref(*parent, "atomCollection").value_or(0));
		ASSERT_NE(above, nullptr);
		const bool aboveHierarchy = FileValue<uint32_t>(world, *above, "collectionHierarchy") != 0 ||
		                            (FileValue<uint8_t>(world, *above, "collectionFlags") & 1U) == 0;
		EXPECT_EQ(collection.hierarchy, aboveHierarchy) << number;
	}
	const auto rules = world.Fields(*record, "rule");
	ASSERT_EQ(collection.rules.size(), rules.size());
	for (size_t i = 0; i < rules.size(); ++i)
	{
		uint32_t rule = 0;
		std::memcpy(&rule, world.Bytes().data() + rules[i]->at, sizeof(rule));
		EXPECT_EQ(collection.rules[i], static_cast<int>(rule));
	}
	const auto data = world.Refs(*record, "ruleData");
	ASSERT_EQ(collection.emitters.size(), data.size());
	for (size_t i = 0; i < data.size(); ++i)
	{
		const auto* emitter = world.Record(data[i]);
		ASSERT_NE(emitter, nullptr);
		++checked.emitters;
		const auto& clock = collection.emitters[i];
		EXPECT_EQ(clock.rule, static_cast<int>(FileValue<uint32_t>(world, *emitter, "dataRule")));
		EXPECT_EQ(clock.clock.next, FileValue<float>(world, *emitter, "emitterNext"));
		EXPECT_EQ(clock.clock.emitted, static_cast<int>(FileValue<uint32_t>(world, *emitter, "emitterEmitted")));
		EXPECT_EQ(clock.clock.started, FileValue<uint8_t>(world, *emitter, "emitterNotStarted") == 0);
		// An emitter of the collection is one of its rules
		EXPECT_NE(std::ranges::find(collection.rules, clock.rule), collection.rules.end());
	}
	// The file keeps the newest atom first, the import the oldest first
	const auto atoms = world.Refs(*record, "collectionAtoms");
	ASSERT_EQ(collection.atoms.size(), atoms.size());
	for (size_t i = 0; i < atoms.size(); ++i)
	{
		const auto* atom = world.Record(atoms[atoms.size() - 1 - i]);
		ASSERT_NE(atom, nullptr);
		CheckAtom(world, *atom, collection.atoms[i], checked);
		if (i > 0)
		{
			EXPECT_LE(collection.atoms[i - 1].birth, collection.atoms[i].birth) << "the oldest atom first";
		}
	}
}

void CheckDrawState(std::span<const uint8_t> bytes, const particles::DrawState& state, float frame)
{
	ASSERT_EQ(bytes.size(), 56U);
	std::array<float, 13> floats {};
	std::memcpy(floats.data(), bytes.data(), sizeof(floats));
	EXPECT_EQ(state.position, glm::vec3(floats[9], floats[10], floats[11]));
	EXPECT_EQ(state.scale, floats[12]);
	EXPECT_EQ(state.alpha, static_cast<float>(bytes[52]));
	EXPECT_EQ(state.frame, frame);
	// The file keeps the rotation times the scale
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			EXPECT_FLOAT_EQ(state.rotation[row][column] * state.scale, floats[static_cast<size_t>(row * 3 + column)]);
		}
	}
}

void CheckAtom(const VanillaWorld& world, const VanillaRecord& record, const particles::RestoredAtom& atom, Checked& checked)
{
	ASSERT_TRUE(record.Is(VanillaType::AtomCore));
	++checked.atoms;
	EXPECT_EQ(atom.position, FilePoint(world, record, "atomPosition"));
	EXPECT_EQ(atom.velocity, FilePoint(world, record, "atomVelocity"));
	EXPECT_EQ(atom.birth, FileValue<float>(world, record, "atomBirth"));
	const auto rotation = FileValue<std::array<float, 12>>(world, record, "atomRotation");
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			EXPECT_EQ(atom.rotation[row][column], rotation[static_cast<size_t>(row * 3 + column)]);
		}
	}
	EXPECT_EQ(atom.baseScale, FileValue<float>(world, record, "atomBaseScale"));
	EXPECT_EQ(atom.ruleScale, FileValue<float>(world, record, "atomRuleScale"));
	EXPECT_EQ(atom.stretch, FileValue<float>(world, record, "atomStretch"));
	const auto colour = FileValue<std::array<uint8_t, 4>>(world, record, "atomColour");
	EXPECT_EQ(atom.rgba, (std::array<uint8_t, 4> {colour[2], colour[1], colour[0], colour[3]}));
	const auto specular = FileValue<uint32_t>(world, record, "atomSpecular");
	EXPECT_EQ(atom.specular, (std::array<uint8_t, 3> {static_cast<uint8_t>(specular >> 16U),
	                                                  static_cast<uint8_t>(specular >> 8U), static_cast<uint8_t>(specular)}));
	const auto flags = FileValue<uint8_t>(world, record, "atomFlags");
	EXPECT_EQ(atom.drawOnFirstUpdate, (flags & 1U) != 0);
	EXPECT_EQ(atom.steps, (flags >> 1U) & 3U);
	EXPECT_EQ(atom.deflected, (flags & 8U) != 0);
	EXPECT_EQ(atom.visible, (flags & 16U) != 0);
	CheckDrawState(world.Raw(record, "atomPrevious"), atom.previous, FileValue<float>(world, record, "atomPreviousFrame"));
	CheckDrawState(world.Raw(record, "atomCurrent"), atom.current, FileValue<float>(world, record, "atomFrame"));
	const auto animation = world.Raw(record, "atomAnimation");
	ASSERT_EQ(animation.size(), 12U);
	EXPECT_EQ(atom.frameRate, FileValue<float>(world, record, "atomAnimation"));
	EXPECT_EQ(atom.playAnim, animation[8] != 0);
	EXPECT_EQ(atom.gravity, FileValue<float>(world, record, "atomGravity"));
	EXPECT_EQ(atom.random, FileValue<uint8_t>(world, record, "atomRandom"));
	const auto* render = world.Record(world.Ref(record, "atomRender").value_or(0));
	ASSERT_NE(render, nullptr);
	switch (atom.look)
	{
	case particles::RestoredLook::Sprite:
		ASSERT_TRUE(render->Is(VanillaType::SpriteParticle));
		EXPECT_EQ(atom.creator, static_cast<int>(FileValue<uint32_t>(world, *render, "spriteCreator")));
		// The sprite keeps the number of the particle file of its effect with its creator
		EXPECT_NE(FileValue<uint32_t>(world, *render, "spriteCreatorFile"), 0U);
		break;
	case particles::RestoredLook::GoldenShower:
	case particles::RestoredLook::ObjectRef:
		EXPECT_TRUE(render->Is(VanillaType::GoldenShowerParticle) || render->Is(VanillaType::ObjectRefParticle));
		EXPECT_EQ(atom.object, SameNumber(world.Ref(*render, "objectRef").value_or(0)));
		break;
	case particles::RestoredLook::Point:
		EXPECT_TRUE(render->Is(VanillaType::PointParticle));
		break;
	}
	const auto collections = world.Refs(record, "atomCollections");
	ASSERT_EQ(atom.collections.size(), collections.size());
	for (size_t i = 0; i < collections.size(); ++i)
	{
		CheckCollection(world, collections[collections.size() - 1 - i], atom.collections[i], checked);
	}
}

size_t CountAtoms(const particles::RestoredCollection& collection)
{
	size_t count = collection.atoms.size();
	for (const auto& atom : collection.atoms)
	{
		for (const auto& sub : atom.collections)
		{
			count += CountAtoms(sub);
		}
	}
	return count;
}

/// The effect of the import agrees with each value of its particle system
void CheckEffect(const VanillaWorld& world, uint32_t system, const particle_import::ImportedEffect& effect, Checked& checked)
{
	const auto* interface = world.Record(system);
	ASSERT_NE(interface, nullptr);
	const auto* manager = world.Record(world.Ref(*interface, "systemManager").value_or(0));
	ASSERT_NE(manager, nullptr);
	++checked.effects;
	const auto& state = effect.state;
	EXPECT_EQ(static_cast<uint32_t>(effect.type), FileValue<uint32_t>(world, *manager, "managerType"));
	EXPECT_EQ(effect.synced, FileValue<uint8_t>(world, *manager, "managerSynced") != 0);
	EXPECT_EQ(state.atomCount, FileValue<uint32_t>(world, *manager, "managerAtomCount"));
	EXPECT_EQ(state.origin, FilePoint(world, *manager, "managerOrigin"));
	EXPECT_EQ(state.globalAlpha, static_cast<float>(FileValue<uint8_t>(world, *manager, "managerAlpha")));
	EXPECT_EQ(state.age, FileValue<float>(world, *manager, "managerAge"));
	EXPECT_EQ(state.closing, (FileValue<uint32_t>(world, *manager, "managerFlags") & 1U) != 0);
	EXPECT_EQ(state.direction, FilePoint(world, *manager, "managerDirection"));
	EXPECT_EQ(state.magnitude, FileValue<float>(world, *manager, "managerMagnitude"));
	EXPECT_EQ(state.closeAge, FileValue<float>(world, *manager, "managerCloseAge"));
	EXPECT_EQ(state.firstStepTwice, FileValue<uint8_t>(world, *manager, "managerStepTwice") != 0);
	// The process info: three words that are 0 in every save, the hand, the camera, the direction, the power, the spin
	// and the enabled byte
	const auto info = FileValue<std::array<float, 15>>(world, *manager, "managerInfo");
	const auto infoWords = FileValue<std::array<uint32_t, 15>>(world, *manager, "managerInfo");
	EXPECT_EQ(infoWords[0], 0U);
	EXPECT_EQ(infoWords[1], 0U);
	EXPECT_EQ(infoWords[2], 0U);
	EXPECT_EQ(state.info.handPosition, glm::vec3(info[3], info[4], info[5]));
	EXPECT_EQ(state.info.cameraForward, glm::vec3(info[6], info[7], info[8]));
	EXPECT_EQ(state.info.direction, glm::vec3(info[9], info[10], info[11]));
	EXPECT_EQ(state.info.power, info[12]);
	EXPECT_EQ(state.info.spin, info[13]);
	EXPECT_EQ(state.info.enabled, (infoWords[14] & 0xFFU) != 0);
	// The flags keep a copy of whether the miracle is still cast
	EXPECT_EQ(state.info.enabled, (FileValue<uint32_t>(world, *manager, "managerFlags") & 2U) != 0);
	EXPECT_EQ(state.targets.size(), FileValue<uint32_t>(world, *manager, "targetObjectCount"));
	EXPECT_EQ(state.targetPositions.size(), FileValue<uint32_t>(world, *manager, "targetPointCount"));
	if (const auto player = world.Ref(*manager, "managerPlayer").value_or(0); player != 0)
	{
		EXPECT_EQ(state.player, static_cast<int>(world.Byte(*world.Record(player), "number").value_or(0xFF)));
	}
	else
	{
		EXPECT_EQ(state.player, -1);
	}
	const auto roots = world.Refs(*manager, "managerRoots");
	ASSERT_EQ(state.roots.size(), roots.size());
	size_t atoms = 0;
	for (size_t i = 0; i < roots.size(); ++i)
	{
		CheckCollection(world, roots[roots.size() - 1 - i], state.roots[i], checked);
		atoms += CountAtoms(state.roots[i]);
	}
	// The effect counts every atom of its tree
	EXPECT_EQ(state.atomCount, atoms);
}

} // namespace

TEST(VanillaParticleImport, ReadsAContainerAndItsWholeEffect)
{
	auto fake = GameAtTurn(5);
	for (size_t list = 0; list < 44; ++list)
	{
		fake.Refs("lists",
		          list == particle_import::k_ParticleContainerList ? std::vector<uint32_t> {2} : std::vector<uint32_t> {});
	}
	// 2: the container, 3: its system, 4: the manager, 5: the first collection, 6: its atom, 7: the sprite of the atom,
	// 8: the data of the emitter, 9: the thing the effect follows
	fake.Record(VanillaType::ParticleContainer)
	    .Position(100, 200)
	    .Value("containerSingleSort", uint8_t {1})
	    .Ref("containerFollows", 9)
	    .Ref("containerSystem", 3)
	    .Value("containerTurns", int32_t {-10})
	    .Value("containerScale", 1.5f);
	fake.Record(VanillaType::ParticleSystem).Ref("systemManager", 4);
	fake.Record(VanillaType::ParticleManager)
	    .Value("managerAtomCount", uint32_t {1})
	    .Ref("managerPlayer", 0)
	    .Refs("managerRoots", {5})
	    .Value("managerInfo", std::array<float, 15> {0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	                                                 1.0f, 0.0f, std::bit_cast<float>(uint32_t {1})})
	    .Value("managerOrigin", std::array<float, 3> {10.0f, 20.0f, 30.0f})
	    .Value("managerAlpha", uint8_t {200})
	    .Value("managerAge", 6.5f)
	    .Value("managerFlags", uint32_t {3})
	    .Value("managerDirection", std::array<float, 3> {0.0f, 0.0f, 1.0f})
	    .Value("managerMagnitude", 2.0f)
	    .Value("managerCloseAge", 0.5f)
	    .Ref("managerSpell", 0)
	    .Value("managerSynced", uint8_t {1})
	    .Value("managerStepTwice", uint8_t {0})
	    .Value("managerType", uint32_t {116})
	    .Value("targetObjectCount", uint32_t {1})
	    .Ref("targetObject", 9)
	    .Value("targetPointCount", uint32_t {0});
	fake.Record(VanillaType::AtomCollection)
	    .Value("ruleCount", uint32_t {1})
	    .Value("ruleFile", uint32_t {116})
	    .Value("rule", uint32_t {4})
	    .Refs("ruleData", {8})
	    .Value("collectionBirth", 0.25f)
	    .Ref("collectionManager", 4)
	    .Value("collectionGroup", uint32_t {2})
	    .Value("collectionFlags", uint8_t {0xCB})
	    .Ref("collectionParent", 0)
	    .Refs("collectionAtoms", {6})
	    .Ref("collectionUnused", 0)
	    .Value("collectionHierarchy", uint32_t {0})
	    .Value("collectionAlpha", uint8_t {128});
	std::array<float, 14> draw {2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 4.0f, 5.0f, 6.0f, 2.0f, 0.0f};
	std::memcpy(&draw[13], std::array<uint8_t, 4> {77, 0xCD, 0xCD, 0xCD}.data(), 4);
	fake.Record(VanillaType::AtomCore)
	    .Refs("atomCollections", {})
	    .Refs("atomRuleData", {})
	    .Refs("atomSounds", {})
	    .Value("atomVelocity", std::array<float, 3> {0.0f, 1.0f, 0.0f})
	    .Value("atomBirth", 6.0f)
	    .Value("atomRotation", std::array<float, 12> {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f})
	    .Value("atomBaseScale", 0.5f)
	    .Value("atomRuleScale", 4.0f)
	    .Value("atomStretch", 1.0f)
	    .Value("atomPosition", std::array<float, 3> {4.0f, 5.0f, 6.0f})
	    .Value("atomColour", std::array<uint8_t, 4> {30, 20, 10, 140})
	    .Value("atomSpecular", uint32_t {0x010203})
	    .Value("atomFlags", uint16_t {0xC0D4})
	    .Value("atomPrevious", draw)
	    .Value("atomCurrent", draw)
	    .Value("atomPreviousFrame", 3.0f)
	    .Value("atomFrame", 4.0f)
	    .Value("atomAnimation", std::array<uint32_t, 3> {std::bit_cast<uint32_t>(13.5f), 9, 0x0101})
	    .Value("atomGravity", 1.0f)
	    .Ref("atomCollection", 5)
	    .Ref("atomDrawOffset", 0)
	    .Ref("atomRender", 7)
	    .Value("atomRandom", uint8_t {99});
	fake.Record(VanillaType::SpriteParticle).Value("spriteCreatorFile", uint32_t {116}).Value("spriteCreator", uint32_t {14});
	fake.Record(VanillaType::EmitterRuleData)
	    .Value("dataRuleFile", uint32_t {116})
	    .Value("dataRule", uint32_t {4})
	    .Value("emitterNext", 6.6f)
	    .Value("emitterEmitted", uint32_t {42})
	    .Value("emitterNotStarted", uint8_t {0});
	fake.Record(VanillaType::Bonfire);
	const auto world = fake.Make();

	const auto read = particle_import::ReadContainers(world, SameNumber);
	EXPECT_TRUE(read.skipped.empty());
	ASSERT_EQ(read.containers.size(), 1U);
	const auto& container = read.containers.front();
	EXPECT_EQ(container.number, 2U);
	EXPECT_EQ(container.position.x, 100);
	EXPECT_TRUE(container.singleSort);
	EXPECT_EQ(container.follows, 9U);
	EXPECT_EQ(container.turns, -10);
	EXPECT_EQ(container.scale, 1.5f);
	Checked checked;
	CheckEffect(world, 3, container.effect, checked);
	EXPECT_EQ(checked.collections, 1U);
	EXPECT_EQ(checked.atoms, 1U);
	EXPECT_EQ(checked.emitters, 1U);
	const auto& state = container.effect.state;
	EXPECT_EQ(state.targets, std::vector<entt::entity> {SameNumber(9)});
	const auto& collection = state.roots.front();
	EXPECT_FALSE(collection.hierarchy);
	EXPECT_TRUE(collection.interpolated);
	EXPECT_FALSE(collection.emitting);
	const auto& atom = collection.atoms.front();
	EXPECT_EQ(atom.look, particles::RestoredLook::Sprite);
	EXPECT_EQ(atom.rgba, (std::array<uint8_t, 4> {10, 20, 30, 140}));
	EXPECT_EQ(atom.steps, 2);
	EXPECT_TRUE(atom.visible);
	EXPECT_EQ(atom.current.rotation, glm::mat3(1.0f));
	EXPECT_EQ(atom.current.alpha, 77.0f);
}

TEST(VanillaParticleImport, AContainerOfAMiracleIsNotTaken)
{
	auto fake = GameAtTurn(5);
	fake.Record(VanillaType::ParticleContainer).Ref("containerSystem", 3).Value("containerTurns", int32_t {-1});
	fake.Record(VanillaType::ParticleSystem).Ref("systemManager", 4);
	fake.Record(VanillaType::ParticleManager).Ref("managerSpell", 2).Refs("managerRoots", {});
	const auto read = particle_import::ReadContainer(fake.Make(), 2, SameNumber);
	EXPECT_FALSE(read.container.has_value());
	EXPECT_NE(read.why.find("miracle"), std::string::npos);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneKeepEveryValueOfTheirParticleContainers)
{
	Checked checked;
	const auto files = ForEachLandOneSave([&checked](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadContainers(world, SameNumber);
		EXPECT_TRUE(read.skipped.empty()) << path << " " << (read.skipped.empty() ? "" : read.skipped.front());
		const auto list = world.Refs(*world.Fields(world.Game(), "lists").at(particle_import::k_ParticleContainerList));
		ASSERT_EQ(read.containers.size(), list.size()) << path;
		for (size_t i = 0; i < list.size(); ++i)
		{
			// The import takes the oldest container first, the file keeps the newest first
			const auto& container = read.containers[i];
			const auto* record = world.Record(list[list.size() - 1 - i]);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(container.number, record->number);
			++checked.containers;
			EXPECT_EQ(container.turns, FileValue<int32_t>(world, *record, "containerTurns"));
			EXPECT_EQ(container.scale, FileValue<float>(world, *record, "containerScale"));
			EXPECT_EQ(container.singleSort, FileValue<uint8_t>(world, *record, "containerSingleSort") == 1);
			EXPECT_EQ(container.follows, world.Ref(*record, "containerFollows").value_or(0));
			CheckEffect(world, world.Ref(*record, "containerSystem").value_or(0), container.effect, checked);
			std::cout << path.generic_string() << " container " << container.number << ": type "
			          << static_cast<uint32_t>(container.effect.type) << ", turns " << container.turns << " (file "
			          << FileValue<int32_t>(world, *record, "containerTurns") << "), scale " << container.scale << " (file "
			          << FileValue<float>(world, *record, "containerScale") << "), follows " << container.follows << ", age "
			          << container.effect.state.age << ", atoms " << container.effect.state.atomCount << "\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << "checked " << checked.containers << " containers, " << checked.effects << " effects, " << checked.collections
	          << " collections, " << checked.atoms << " atoms, " << checked.emitters << " emitters\n";
	EXPECT_GT(checked.containers, 0U);
	EXPECT_GT(checked.atoms, 0U);
	EXPECT_GT(checked.emitters, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneGiveEachTempleHeartItsContainers)
{
	uint32_t beams = 0;
	const auto files = ForEachLandOneSave([&beams](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadContainers(world, SameNumber);
		for (const auto& temple : ecs::vanilla_script_import::ReadTemples(world))
		{
			for (const auto number : {temple.beams, temple.destructionBeams, temple.destructionGlow})
			{
				if (number == 0)
				{
					continue;
				}
				// Each container of a heart is a container of the game list, so the import starts its effect
				EXPECT_TRUE(std::ranges::any_of(read.containers, [number](const auto& c) { return c.number == number; }))
				    << path << " " << number;
				++beams;
				std::cout << path.generic_string() << " heart " << temple.heart << " holds container " << number << "\n";
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(beams, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneKeepEveryValueOfTheirVorticesThatDrawThingsIn)
{
	uint32_t vortices = 0;
	Checked checked;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadVorticesIn(world, SameNumber);
		EXPECT_TRUE(read.skipped.empty()) << path;
		for (const auto& vortex : read.vortices)
		{
			const auto* record = world.Record(vortex.number);
			ASSERT_NE(record, nullptr);
			++vortices;
			EXPECT_EQ(vortex.centre, FilePoint(world, *record, "vortexCentre"));
			EXPECT_EQ(static_cast<uint32_t>(vortex.state), FileValue<uint32_t>(world, *record, "vortexState"));
			// A vortex that draws things in is of the first kind
			EXPECT_EQ(FileValue<uint32_t>(world, *record, "vortexType"), static_cast<uint32_t>(VortexType::In));
			EXPECT_EQ(vortex.altitude, FileValue<float>(world, *record, "vortexAltitude"));
			EXPECT_EQ(vortex.groundAverage, FileValue<float>(world, *record, "vortexGroundAverage"));
			EXPECT_EQ(vortex.groundHeights.size(), FileValue<uint32_t>(world, *record, "vortexGroundCount"));
			// The five words that only the save and the load use are 0
			const auto* words = world.Field(*record, "vortexWords");
			ASSERT_NE(words, nullptr);
			EXPECT_EQ(words->count, 5U);
			const auto bytes = world.Raw(*record, "vortexWords");
			EXPECT_TRUE(std::ranges::all_of(bytes, [](uint8_t b) { return b == 0; })) << path;
			ASSERT_TRUE(vortex.mover.has_value());
			CheckEffect(world, world.Ref(*record, "vortexMover").value_or(0), *vortex.mover, checked);
			std::cout << path.generic_string() << " vortex " << vortex.number << ": state "
			          << static_cast<uint32_t>(vortex.state) << ", altitude " << vortex.altitude << ", mover type "
			          << static_cast<uint32_t>(vortex.mover->type) << ", crossing bytes "
			          << (vortex.crossing.has_value() ? vortex.crossing->size() : 0) << "\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(vortices, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneHaveEachAtomOfTheFileInAnEffectThatTheImportStarts)
{
	size_t atoms = 0;
	const auto files = ForEachLandOneSave([&atoms](const std::filesystem::path& path, const VanillaWorld& world) {
		size_t imported = 0;
		const auto count = [&imported](const particles::RestoredEffect& state) {
			for (const auto& root : state.roots)
			{
				imported += CountAtoms(root);
			}
		};
		for (const auto& container : particle_import::ReadContainers(world, SameNumber).containers)
		{
			count(container.effect.state);
		}
		for (const auto& vortex : particle_import::ReadVorticesIn(world, SameNumber).vortices)
		{
			if (vortex.mover.has_value())
			{
				count(vortex.mover->state);
			}
		}
		EXPECT_EQ(imported, world.OfType(VanillaType::AtomCore).size()) << path;
		atoms += imported;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(atoms, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneHaveEveryContainerAndVortexThatTheScriptsHold)
{
	uint32_t held = 0;
	const auto files = ForEachLandOneSave([&held](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto table = ecs::vanilla_script_import::ReadScriptTable(world);
		ASSERT_TRUE(table.has_value());
		const auto containers = particle_import::ReadContainers(world, SameNumber);
		const auto vortices = particle_import::ReadVorticesIn(world, SameNumber);
		for (const auto& place : table->places)
		{
			const auto* record = place.number != 0 ? world.Record(place.number) : nullptr;
			if (record == nullptr)
			{
				continue;
			}
			if (record->Is(VanillaType::ParticleContainer))
			{
				++held;
				EXPECT_TRUE(
				    std::ranges::any_of(containers.containers, [&place](const auto& c) { return c.number == place.number; }))
				    << path << " " << place.number;
			}
			else if (record->Is(VanillaType::LandscapeVortexIn))
			{
				++held;
				EXPECT_TRUE(
				    std::ranges::any_of(vortices.vortices, [&place](const auto& v) { return v.number == place.number; }))
				    << path << " " << place.number;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(held, 0U);
}

namespace footpath_import = openblack::ecs::vanilla_footpath_import;

// Each footpath of a Land 1 save keeps every value of the file: its nodes in order, the place, the flags and the
// walkers of each node, and the word that lets walkers take it. Each link of a thing keeps its footpaths in order.
TEST(VanillaFootpathImport, SavedGamesOfLandOneKeepEveryValueOfTheirFootpaths)
{
	uint32_t paths = 0;
	uint32_t nodes = 0;
	uint32_t links = 0;
	std::map<uint32_t, uint32_t> holderTypes;
	std::map<uint8_t, uint32_t> flags;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = footpath_import::ReadFootpaths(world);
		EXPECT_TRUE(imported.skipped.empty()) << path << " " << (imported.skipped.empty() ? "" : imported.skipped.front());
		EXPECT_EQ(imported.finders, 0U) << path;
		// Every footpath and every node of the file is in a footpath of the list, once
		std::set<uint32_t> listed;
		std::set<uint32_t> listedNodes;
		for (const auto& footpath : imported.paths)
		{
			EXPECT_TRUE(listed.insert(footpath.number).second) << path;
			const auto* record = world.Record(footpath.number);
			ASSERT_NE(record, nullptr) << path;
			EXPECT_EQ(footpath.walkable, world.Word(*record, "walkable")) << path;
			const auto refs = world.Refs(*record, "nodes");
			ASSERT_EQ(footpath.nodes.size(), refs.size()) << path;
			for (size_t i = 0; i < refs.size(); ++i)
			{
				const auto& node = footpath.nodes[i];
				EXPECT_EQ(node.number, refs[i]) << path;
				EXPECT_TRUE(listedNodes.insert(node.number).second) << path;
				const auto& nodeRecord = *world.Record(node.number);
				const auto place = world.Raw(nodeRecord, "pos");
				ASSERT_EQ(place.size(), sizeof(node.coords)) << path;
				EXPECT_EQ(std::memcmp(place.data(), &node.coords, sizeof(node.coords)), 0) << path;
				EXPECT_EQ(node.flags, world.Byte(nodeRecord, "nodeFlags")) << path;
				const auto followers = world.Refs(nodeRecord, "followers");
				EXPECT_TRUE(std::ranges::equal(node.followers, followers)) << path;
				// openblack keeps every flag of the file but the first, which the game sets on each node it makes
				EXPECT_EQ(node.flags & ~footpath_import::k_NodeKeptFlags, 0) << path;
				EXPECT_NE(node.flags & footpath_import::k_NodeMade, 0) << path;
				const auto made = footpath_import::NodeOf(node, 3.5f);
				EXPECT_EQ(made.place, glm::ivec2(node.coords.x, node.coords.z)) << path;
				EXPECT_EQ(made.planned, (node.flags & footpath_import::k_NodePlanned) != 0) << path;
				EXPECT_EQ(made.hidden, (node.flags & footpath_import::k_NodeHidden) != 0) << path;
				EXPECT_EQ(made.marker, (node.flags & footpath_import::k_NodeMarker) != 0) << path;
				EXPECT_EQ(made.position.y, node.coords.altitude + 3.5f) << path;
				++flags[node.flags];
				++nodes;
			}
			++paths;
		}
		uint32_t footpathRecords = 0;
		uint32_t nodeRecords = 0;
		uint32_t linkRecords = 0;
		for (const auto& record : world.Records())
		{
			footpathRecords += record.Is(VanillaType::Footpath) ? 1 : 0;
			nodeRecords += record.Is(VanillaType::FootpathNode) ? 1 : 0;
			linkRecords += record.Is(VanillaType::FootpathLink) ? 1 : 0;
		}
		EXPECT_EQ(listed.size(), footpathRecords) << path;
		EXPECT_EQ(listedNodes.size(), nodeRecords) << path;
		// Every link of the file is the link of one thing, and its footpaths are footpaths of the list
		std::set<uint32_t> linked;
		for (const auto& link : imported.links)
		{
			EXPECT_TRUE(linked.insert(link.number).second) << path;
			EXPECT_TRUE(std::ranges::equal(link.footpaths, world.Refs(*world.Record(link.number), "footpaths"))) << path;
			for (const auto footpath : link.footpaths)
			{
				EXPECT_TRUE(listed.contains(footpath)) << path << " link " << link.number;
			}
			++holderTypes[world.Record(link.holder)->type];
			++links;
		}
		EXPECT_EQ(linked.size(), linkRecords) << path;
		std::cout << path.parent_path().filename().string() << " footpaths " << imported.paths.size() << " links "
		          << imported.links.size() << std::endl;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(paths, 0U);
	std::cout << "Footpaths " << paths << ", nodes " << nodes << ", links " << links << std::endl;
	for (const auto& [type, count] : holderTypes)
	{
		std::cout << "Links on type 0x" << std::hex << type << std::dec << ": " << count << std::endl;
	}
	for (const auto& [value, count] : flags)
	{
		std::cout << "Node flags 0x" << std::hex << static_cast<int>(value) << std::dec << ": " << count << std::endl;
	}
}

// Each villager of a Land 1 save that walks a footpath keeps its walk: the footpath, the node that it walks to, the way
// and the footpath after it are footpaths and nodes of the file. The walkers of each node are the villagers that walk
// to it.
TEST(VanillaFootpathImport, SavedGamesOfLandOneKeepTheFootpathWalksOfTheirVillagers)
{
	constexpr auto k_OnPath = static_cast<uint8_t>(VillagerStates::MoveOnPath);
	uint32_t walks = 0;
	uint32_t kept = 0;
	uint32_t withNext = 0;
	uint32_t towardsStart = 0;
	uint32_t noNode = 0;
	uint32_t stale = 0;
	std::map<uint8_t, uint32_t> finals;
	std::map<uint8_t, uint32_t> moveStates;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto footpaths = footpath_import::ReadFootpaths(world);
		const auto imported = world_import::ReadTowns(world);
		std::map<uint32_t, const footpath_import::ImportedFootpath*> byNumber;
		for (const auto& footpath : footpaths.paths)
		{
			byNumber.emplace(footpath.number, &footpath);
		}
		std::map<uint32_t, std::set<uint32_t>> walkersOf;
		for (const auto& villager : imported.villagers)
		{
			if (villager.states[0] != k_OnPath)
			{
				continue;
			}
			++walks;
			const auto& walk = villager.footpathWalk;
			// The values of the state are the footpath and the node of the living thing
			ASSERT_EQ(villager.topValues.size(), 2U) << path;
			EXPECT_EQ(villager.topValues[0].object.value_or(0), walk.footpath) << path << " " << villager.number;
			EXPECT_EQ(villager.topValues[1].object.value_or(0), walk.node) << path << " " << villager.number;
			const auto footpath = byNumber.find(walk.footpath);
			ASSERT_NE(footpath, byNumber.end()) << path << " " << villager.number;
			if (walk.node == 0)
			{
				++noNode;
			}
			else
			{
				EXPECT_TRUE(footpath_import::NodeIndexOf(*footpath->second, walk.node).has_value())
				    << path << " " << villager.number;
				walkersOf[walk.node].insert(villager.number);
			}
			if (walk.next != 0)
			{
				EXPECT_TRUE(byNumber.contains(walk.next)) << path << " " << villager.number;
				++withNext;
			}
			towardsStart += walk.towardsStart ? 1 : 0;
			++finals[villager.states[1]];
			++moveStates[villager.moveState];
			kept += world_import::StartStates(villager.states, villager.moveState) == villager.states ? 1 : 0;
		}
		// The walkers of each node: a walker that walks to the node, or a walker that went on and that the game drops
		// from the list when it next looks at the list
		for (const auto& footpath : footpaths.paths)
		{
			for (const auto& node : footpath.nodes)
			{
				const auto& walkers = walkersOf[node.number];
				for (const auto follower : node.followers)
				{
					stale += walkers.contains(follower) ? 0 : 1;
				}
				for (const auto walker : walkers)
				{
					EXPECT_NE(std::ranges::find(node.followers, walker), node.followers.end()) << path << " " << walker;
				}
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(walks, 0U);
	std::cout << "Walks " << walks << ", kept " << kept << ", with a next footpath " << withNext << ", towards the start "
	          << towardsStart << ", no node " << noNode << ", walkers of nodes that walk on " << stale << std::endl;
	for (const auto& [state, count] : finals)
	{
		std::cout << "Final state " << static_cast<int>(state) << ": " << count << std::endl;
	}
	for (const auto& [state, count] : moveStates)
	{
		std::cout << "Move state 0x" << std::hex << static_cast<int>(state) << std::dec << ": " << count << std::endl;
	}
}

namespace
{
world_import::ImportedStateValue BytesValue(std::vector<uint8_t> bytes)
{
	return {.object = std::nullopt, .bytes = std::move(bytes)};
}

world_import::ImportedStateValue ObjectValue(uint32_t number)
{
	return {.object = number, .bytes = {}};
}

template <typename T>
T BytesAs(const world_import::ImportedStateValue& value)
{
	T result {};
	std::memcpy(&result, value.bytes.data(), sizeof(T));
	return result;
}
} // namespace

TEST(VanillaWorldImport, AScriptStateKeepsThePlaceTheNumberAndThePairOfItsScript)
{
	const std::vector values = {BytesValue({0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x02, 0x00}),
	                            BytesValue({0x00, 0x00, 0x20, 0x41}), BytesValue({0x8D, 0x01, 0x00, 0x00}),
	                            BytesValue({0x03, 0x00, 0x00, 0x00})};
	const auto script = world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::InScript), values);
	ASSERT_TRUE(script.has_value());
	EXPECT_EQ(script->x, 0x10000);
	EXPECT_EQ(script->z, 0x28000);
	EXPECT_EQ(script->number, 10.0f);
	EXPECT_EQ(script->first, 397U);
	EXPECT_EQ(script->second, 3U);
	for (const auto state :
	     {VillagerStates::ScriptWanderAroundPosition, VillagerStates::ScriptPlayAnim, VillagerStates::ScriptInCrowd})
	{
		EXPECT_TRUE(world_import::ScriptValuesOf(static_cast<uint8_t>(state), values).has_value());
	}
	// Another state, or values of other sizes, give nothing
	EXPECT_FALSE(world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::Dead), values).has_value());
	const std::vector three(values.begin(), values.begin() + 3);
	EXPECT_FALSE(world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::InScript), three).has_value());
}

TEST(VanillaWorldImport, APlaceInMetresGivesTheMapUnitsAgain)
{
	// The game makes a place of a script from metres, so each place of a file is the map units of a float
	for (const int32_t units : {0, 1, 65536, 0x28000, 8090858, map_coords::ToFixed(4999.37f), map_coords::ToFixed(-3.3f)})
	{
		EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(units)), units) << units;
	}
}

TEST(VanillaWorldImport, AShepherdStateKeepsItsCountdownAndTheStateAfterItsWait)
{
	const std::vector values = {BytesValue({0xFF, 0xFF}), BytesValue({0x5F, 0x00})};
	const auto shepherd =
	    world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::ShepherdCheckAnimalForSlaughter), values);
	ASSERT_TRUE(shepherd.has_value());
	EXPECT_EQ(shepherd->countdown, -1);
	EXPECT_EQ(shepherd->pending, static_cast<uint16_t>(VillagerStates::ShepherdDecideWhatToDoWithFlock));
	EXPECT_TRUE(world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::ShepherdLookForFlock), values));
	EXPECT_FALSE(world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::InScript), values));
}

TEST(VanillaWorldImport, TheDeadStateKeepsTheReasonForTheDeath)
{
	constexpr auto k_Dead = static_cast<uint8_t>(VillagerStates::Dead);
	EXPECT_EQ(world_import::DeathReasonOf(k_Dead, {BytesValue({0x09})}), DeathReason::OldAge);
	EXPECT_EQ(world_import::DeathReasonOf(k_Dead, {BytesValue({0x00})}), DeathReason::None);
	// A reason that the game does not have, or another state, gives nothing
	EXPECT_FALSE(world_import::DeathReasonOf(k_Dead, {BytesValue({0x0A})}).has_value());
	EXPECT_FALSE(world_import::DeathReasonOf(static_cast<uint8_t>(VillagerStates::Dying), {BytesValue({0x09})}).has_value());
	// The pose on its back is pose 2 in bits 4 and 5 of the living status
	EXPECT_TRUE(world_import::LiesOnBack(0x0029));
	EXPECT_FALSE(world_import::LiesOnBack(0x0039));
	EXPECT_FALSE(world_import::LiesOnBack(0x0019));
	EXPECT_FALSE(world_import::LiesOnBack(0x0000));
}

TEST(VanillaWorldImport, ATradingDiscipleWithNothingToDoKeepsItsResourceAndItsTown)
{
	constexpr auto k_Idle = static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo);
	constexpr auto k_Trader = static_cast<uint8_t>(VillagerDisciple::Trader);
	const std::vector values = {BytesValue({0x01, 0x00, 0x00, 0x00}), ObjectValue(42)};
	const auto trade = world_import::TradeOf(k_Idle, k_Trader, values);
	ASSERT_TRUE(trade.has_value());
	EXPECT_EQ(trade->resource, ResourceType::Wood);
	EXPECT_EQ(trade->town, 42U);
	// Another disciple keeps no values, and another state gives nothing
	EXPECT_FALSE(world_import::TradeOf(k_Idle, static_cast<uint8_t>(VillagerDisciple::Fisherman), {}).has_value());
	EXPECT_FALSE(world_import::TradeOf(static_cast<uint8_t>(VillagerStates::InScript), k_Trader, values).has_value());
}

TEST(VanillaWorldImport, TheVillagerFlagsGiveADiscipleOrAFollower)
{
	const auto disciple = world_import::DiscipleOf(3, 0x1200);
	ASSERT_TRUE(disciple.has_value());
	EXPECT_EQ(disciple->type, VillagerDisciple::Fisherman);
	EXPECT_FALSE(disciple->follower);
	const auto follower = world_import::DiscipleOf(0, 0x0400);
	ASSERT_TRUE(follower.has_value());
	EXPECT_EQ(follower->type, VillagerDisciple::None);
	EXPECT_TRUE(follower->follower);
	// With neither flag the villager is not a disciple, and a type that the game does not have gives nothing
	EXPECT_FALSE(world_import::DiscipleOf(3, 0x1000).has_value());
	EXPECT_FALSE(world_import::DiscipleOf(13, 0x0200).has_value());
}

// No villager of a Land 1 save decides again after the import: each keeps its states, and the values of its shepherd,
// script, dead and disciple states are the bytes of the file
TEST(VanillaWorldImport, SavedGamesOfLandOneHaveNoVillagerThatDecidesAgain)
{
	uint32_t villagers = 0;
	uint32_t decideAgain = 0;
	uint32_t shepherds = 0;
	uint32_t scripts = 0;
	uint32_t dead = 0;
	uint32_t idleDisciples = 0;
	uint32_t disciples = 0;
	uint32_t followers = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		for (const auto& villager : imported.villagers)
		{
			++villagers;
			const auto* record = world.Record(villager.number);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(villager.discipleType, world.Byte(*record, "discipleType").value_or(0xFF));
			EXPECT_EQ(villager.villagerFlags, world.Half(*record, "villagerFlags").value_or(0xFFFF));
			EXPECT_EQ(villager.status, world.Half(*record, "status").value_or(0xFFFF));
			if (const auto disciple = world_import::DiscipleOf(villager.discipleType, villager.villagerFlags);
			    disciple.has_value())
			{
				EXPECT_EQ(static_cast<uint8_t>(disciple->type), villager.discipleType) << path << " " << villager.number;
				disciples += disciple->follower ? 0 : 1;
				followers += disciple->follower ? 1 : 0;
			}
			else
			{
				EXPECT_EQ(villager.discipleType, 0) << path << " " << villager.number;
			}
			if (world_import::StartStates(villager.states, villager.moveState) != villager.states)
			{
				++decideAgain;
				ADD_FAILURE() << path << " " << villager.number << " decides again in state "
				              << static_cast<int>(villager.states[0]) << "/" << static_cast<int>(villager.states[1]);
				continue;
			}
			const std::array<std::pair<uint8_t, const std::vector<world_import::ImportedStateValue>*>, 2> states = {
			    std::pair {villager.states[0], &villager.topValues}, std::pair {villager.states[1], &villager.finalValues}};
			for (const auto& [state, values] : states)
			{
				if (const auto script = world_import::ScriptValuesOf(state, *values); script.has_value())
				{
					++scripts;
					const auto place = BytesAs<std::array<int32_t, 2>>((*values)[0]);
					EXPECT_EQ(script->x, place[0]);
					EXPECT_EQ(script->z, place[1]);
					EXPECT_EQ(std::bit_cast<uint32_t>(script->number), BytesAs<uint32_t>((*values)[1]));
					EXPECT_EQ(script->first, BytesAs<uint32_t>((*values)[2]));
					EXPECT_EQ(script->second, BytesAs<uint32_t>((*values)[3]));
					EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(script->x)), script->x);
					EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(script->z)), script->z);
				}
				if (const auto shepherd = world_import::ShepherdValuesOf(state, *values); shepherd.has_value())
				{
					++shepherds;
					EXPECT_EQ(shepherd->countdown, BytesAs<int16_t>((*values)[0]));
					EXPECT_EQ(shepherd->pending, BytesAs<uint16_t>((*values)[1]));
				}
				if (const auto reason = world_import::DeathReasonOf(state, *values); reason.has_value())
				{
					++dead;
					EXPECT_EQ(static_cast<uint8_t>(*reason), (*values)[0].bytes[0]);
				}
				if (state == static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo))
				{
					++idleDisciples;
					EXPECT_TRUE(values->empty() || world_import::TradeOf(state, villager.discipleType, *values).has_value());
				}
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(villagers, 0U);
	EXPECT_EQ(decideAgain, 0U);
	EXPECT_GT(shepherds, 0U);
	EXPECT_GT(scripts, 0U);
	EXPECT_GT(dead, 0U);
	EXPECT_GT(idleDisciples, 0U);
	EXPECT_GT(disciples, 0U);
	std::cout << "Villagers " << villagers << ", shepherd values " << shepherds << ", script values " << scripts << ", dead "
	          << dead << ", idle disciples " << idleDisciples << ", disciples " << disciples << ", followers " << followers
	          << std::endl;
}
