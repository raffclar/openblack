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
namespace script_import = openblack::ecs::vanilla_script_import;

TEST(VanillaWorldImport, ReadsThePlansAndTheBuildingSitesOfTheTowns)
{
	auto fake = GameAtTurn(100);
	// Record 2: the town, which lists its plan and its site
	fake.Record(VanillaType::Town)
	    .Position(0, 0)
	    .Value<uint32_t>("id", 0)
	    .Value<uint32_t>("tribe", static_cast<uint32_t>(Tribe::AZTEC))
	    .OwnedBy(0)
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
	    .OwnedBy(0);
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

// The game object keeps the influence multipliers and the eight land balances that the land's script gave and that a
// script can change after
TEST(VanillaWorldImport, TheLandValuesComeFromTheGameObject)
{
	auto world = GameAtTurn(10);
	EXPECT_FALSE(world_import::ReadLandValues(world.Make()).has_value());
	world.Value("playerInfluenceMultiplier", 0.5f)
	    .Value("townInfluenceMultiplier", 1.25f)
	    .Value("landBalance", std::array {1.0f, 2.0f, 2.0f, 2.0f, 1.5f, 2.0f, 1.0f, 0.75f});
	const auto land = world_import::ReadLandValues(world.Make());
	ASSERT_TRUE(land.has_value());
	EXPECT_EQ(land->playerInfluenceMultiplier, 0.5f);
	EXPECT_EQ(land->townInfluenceMultiplier, 1.25f);
	EXPECT_EQ(land->landBalance, (std::array {1.0f, 2.0f, 2.0f, 2.0f, 1.5f, 2.0f, 1.0f, 0.75f}));
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
		// The script of Land 1 gives multipliers of 1 and no balance, and no shipped script changes a balance
		const auto land = world_import::ReadLandValues(world);
		ASSERT_TRUE(land.has_value()) << path;
		EXPECT_EQ(land->playerInfluenceMultiplier, 1.0f) << path;
		EXPECT_EQ(land->townInfluenceMultiplier, 1.0f) << path;
		EXPECT_TRUE(std::ranges::all_of(land->landBalance, [](float value) { return value == 1.0f; })) << path;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(statics, 0U);
	std::cout << "Read " << statics << " still and loose things" << '\n' << std::flush;
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
	          << '\n'
	          << std::flush;
}

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
			// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison): the compare must be bit exact
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
			          << totem.heading << '\n'
			          << std::flush;
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
			// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison): the compare must be bit exact
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
				          << path.generic_string() << '\n'
				          << std::flush;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(highlights, 0U);
	std::cout << *files << " Land 1 saves: " << highlights << " highlights, " << active << " active" << '\n' << std::flush;
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

// A load removes each thing of the land that no thing of the file takes, in the order of the land
TEST(VanillaWorldImport, ALandThingGoesWhenNoThingOfTheFileTakesIt)
{
	const std::array land {entt::entity {4}, entt::entity {7}, entt::entity {9}};
	const std::array taken {entt::entity {7}, entt::entity {12}};
	EXPECT_EQ(world_import::LandThingsToRemove(land, taken), (std::vector<entt::entity> {entt::entity {4}, entt::entity {9}}));
	EXPECT_EQ(world_import::LandThingsToRemove(land, {}).size(), land.size());
	EXPECT_TRUE(world_import::LandThingsToRemove({}, taken).empty());
}

// A load removes each temple of the land whose player has no temple in the file. The temples of the other players stay.
TEST(VanillaScriptImport, ALandTempleGoesWhenItsPlayerHasNoTempleInTheFile)
{
	const std::array land {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
	const std::array file {script_import::ImportedTemple {.player = PlayerNames::PLAYER_ONE},
	                       script_import::ImportedTemple {.player = PlayerNames::PLAYER_FOUR}};
	EXPECT_EQ(script_import::LandTemplesToRemove(land, file),
	          (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE}));
	EXPECT_EQ(script_import::LandTemplesToRemove(land, {}).size(), land.size());
	EXPECT_TRUE(script_import::LandTemplesToRemove({}, file).empty());
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
	    .OwnedBy(1);
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
	static constexpr auto k_PotRow = [](PotInfo row) { return world_import::k_FirstPotRowNumber - static_cast<uint32_t>(row); };
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

// A workshop keeps the turns until its scaffold is finished, the count of its scaffolds, the places of its yard, its
// scaffolds in the order of the file and its wood pile
TEST(VanillaWorldImport, ReadsTheValuesOfAWorkshopAndItsWoodPile)
{
	static constexpr auto k_PotRow = [](PotInfo row) { return world_import::k_FirstPotRowNumber - static_cast<uint32_t>(row); };
	auto fake = GameAtTurn(100);
	// 2: the town, 3: its workshop, 4: the wood pile of the workshop
	fake.Record(VanillaType::Town).Position(0, 0).Value<uint32_t>("id", 0).Refs("abodes", {3});
	fake.Record(VanillaType::Workshop)
	    .Position(1000, 2000)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseWorkshop))
	    .Ref("town", 2)
	    .Refs("villagers", {})
	    .Words("resources", {0, 0})
	    .Ref("workshopWoodPile", 4)
	    .Ref("workshopFlag", 0)
	    .Value<int32_t>("workshopMakingTurns", 37)
	    .Value<uint32_t>("workshopCounted", 2)
	    .Value("workshopPlaces", std::array<uint8_t, 3> {2, 0, 1})
	    .Refs("workshopScaffolds", {9, 7});
	fake.Record(VanillaType::MagicWood)
	    .Position(1100, 2000)
	    .Value("info", k_PotRow(PotInfo::MagicWood))
	    .Value<uint32_t>("potAmount", 450)
	    .Value<uint8_t>("potFlags", 0)
	    .Ref("potBuilding", 0)
	    .Value("pileRise", std::array<float, 12> {});
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.abodes.size(), 1U);
	const auto& abode = imported.abodes[0];
	EXPECT_EQ(abode.row, AbodeInfo::NorseWorkshop);
	ASSERT_TRUE(abode.workshop.has_value());
	EXPECT_EQ(abode.workshop->makingTurns, 37);
	EXPECT_EQ(abode.workshop->counted, 2U);
	EXPECT_EQ(abode.workshop->places, (std::array<uint8_t, 3> {2, 0, 1}));
	EXPECT_EQ(abode.workshop->scaffolds, (std::vector<uint32_t> {9, 7}));
	EXPECT_EQ(abode.workshop->woodPile, 4U);
	ASSERT_EQ(abode.piles.size(), 1U);
	EXPECT_EQ(abode.piles[0].number, 4U);
	EXPECT_EQ(abode.piles[0].row, PotInfo::MagicWood);
	EXPECT_EQ(abode.piles[0].amount, 450U);
}

TEST(VanillaWorldImport, TheTribesCountBackFromTheirFirstRow)
{
	EXPECT_EQ(world_import::TribeRowOf(742), Tribe::CELTIC);
	EXPECT_EQ(world_import::TribeRowOf(735), Tribe::NORSE);
	EXPECT_EQ(world_import::TribeRowOf(734), Tribe::TIBETAN);
	EXPECT_EQ(world_import::TribeRowOf(733), std::nullopt);
	EXPECT_EQ(world_import::TribeRowOf(743), std::nullopt);
	// The 355 rows of the 8 tables between the last tribe and the first totem
	EXPECT_EQ(world_import::k_FirstTribeRowNumber - static_cast<uint32_t>(Tribe::_COUNT) - 355,
	          world_import::k_FirstTotemStatueRowNumber);
}
