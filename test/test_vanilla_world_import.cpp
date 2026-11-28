/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaWorldImportFixture.h"

using namespace openblack;
using namespace openblack::gamesave;
using namespace vanilla_import_test;

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
	EXPECT_TRUE(world_import::IsImportedBuilding(AbodeInfo::NorseWorkshop));
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
	    .OwnedBy(0)
	    .Value<uint32_t>("cannotBuildWorshipSite", 1)
	    .Value<uint32_t>("uninhabitable", 0)
	    .Refs("workshops", {4})
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
	EXPECT_EQ(town.workshops, (std::vector<uint32_t> {4}));

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
		ASSERT_TRUE(read.world.has_value()) << entry.path() << " " << (read.error ? read.error->reason : std::string {});
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
		    .OwnedBy(0);
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
	    .OwnedBy(0);
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
		    .OwnedBy(id == 2 ? 1 : 0);
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

// A town keeps the number of the player that it had before it last changed hands. The owner of the town is the player
// that its owner reference gives.
TEST(VanillaWorldImport, TheOwnerOfATownIsThePlayerOfItsOwnerReference)
{
	auto fake = GameAtTurn(100);
	// 2: a town that went from the first player to the neutral player, 3: the neutral player
	fake.Record(VanillaType::Town)
	    .Position(1000, 1000)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
	    .Value<uint8_t>("player", 0)
	    .Ref("owner", 3);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", static_cast<uint8_t>(PlayerNames::NEUTRAL));
	// 4: a town with no owner is not taken
	fake.Record(VanillaType::Town)
	    .Position(2000, 1000)
	    .Value<uint32_t>("id", 1)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::NORSE))
	    .Value<uint8_t>("player", 0);

	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.towns.size(), 1U);
	EXPECT_EQ(imported.towns[0].owner, PlayerNames::NEUTRAL);
	ASSERT_EQ(imported.skipped.size(), 1U);
}

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
			if (world_import::StartStates(villager) != villager.states)
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
	std::cout << "Kept " << kept << " villagers, " << walking << " walking and " << working << " working" << '\n' << std::flush;
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

// The site of a temple keeps only the piles of the file: a place with no pile in the file has no pile after the load
TEST(VanillaWorldImport, TheSiteOfATempleKeepsOnlyThePilesOfTheFile)
{
	EXPECT_EQ(world_import::SitePileAfterLoad(0, false), world_import::SitePileLoad::Remove);
	EXPECT_EQ(world_import::SitePileAfterLoad(0, true), world_import::SitePileLoad::Remove);
	EXPECT_EQ(world_import::SitePileAfterLoad(4, true), world_import::SitePileLoad::TakeValues);
	EXPECT_EQ(world_import::SitePileAfterLoad(4, false), world_import::SitePileLoad::Keep);
}
