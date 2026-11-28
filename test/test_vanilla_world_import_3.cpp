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

TEST(VanillaWorldImport, ReadsTheValuesOfAWorshipSite)
{
	auto fake = GameAtTurn(100);
	// 2: the site, 3: its needs display, 4 to 6: its flags, 7: its altar, 8: its food pot
	fake.Record(VanillaType::WorshipSite)
	    .Position(0, 0)
	    .Value<float>("built", 0.25f)
	    .Ref("footpathLink", 40)
	    .Ref("partCitadel", 30)
	    .Value<uint32_t>("siteTribe", world_import::k_FirstTribeRowNumber - static_cast<uint32_t>(Tribe::NORSE))
	    .Ref("siteNeeds", 3)
	    .Ref("siteDance", 31)
	    .Refs("siteTowns", {20, 21})
	    .Ref("siteFoodPot", 8)
	    .Value<uint32_t>("siteWorshipCount", 3)
	    .Refs("siteWorshippers", {50, 51})
	    .Refs("siteIcons", {60, 61, 62})
	    .Words("siteFoodOnTheWay", {std::bit_cast<uint32_t>(-12), 0})
	    .Value<float>("siteBattery", 120.0f)
	    .Value<float>("siteSacrificed", 7.5f)
	    .Value<float>("siteAvailable", 130.0f)
	    .Value<float>("siteUsed", 10.0f)
	    .Value<float>("siteRequested", 14.0f)
	    .Value<float>("siteChantsPerDancer", 0.5f)
	    .Value<uint32_t>("siteInfinite", 1)
	    .Value<uint32_t>("siteFreeMaintenance", 0)
	    .Value<uint8_t>("sitePlace", 5)
	    .Refs("siteGoHome", {})
	    .Ref("siteAltar", 7)
	    .Value<float>("siteStrain", 0.4f);
	fake.Record(VanillaType::ShowNeeds).Ref("needsOwner", 2).Refs("needFlags", {4, 5, 6});
	for (const float need : {0.1f, 0.2f, 0.3f})
	{
		fake.Record(VanillaType::ShowNeedsVisuals).Ref("needFlagOwner", 2).Value<float>("needFlagNeed", need);
	}
	fake.Record(VanillaType::WorshipTotem).Value<float>("built", 1.0f).Value<float>("life", 0.75f).Ref("altarSite", 2);
	fake.Record(VanillaType::PileFood).Value<uint32_t>("potAmount", 90);
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.worshipSites.size(), 1U);
	const auto& site = imported.worshipSites[0];
	EXPECT_EQ(site.number, 2U);
	EXPECT_EQ(site.citadel, 30U);
	EXPECT_EQ(site.tribe, Tribe::NORSE);
	EXPECT_EQ(site.place, 5);
	EXPECT_FLOAT_EQ(site.built, 0.25f);
	EXPECT_EQ(site.towns, (std::vector<uint32_t> {20, 21}));
	EXPECT_EQ(site.dance, 31U);
	EXPECT_EQ(site.foodPot, 8U);
	EXPECT_EQ(site.icons, (std::vector<uint32_t> {60, 61, 62}));
	EXPECT_EQ(site.worshippers, (std::vector<uint32_t> {50, 51}));
	EXPECT_EQ(site.worshipCount, 3U);
	EXPECT_EQ(site.food, 90U);
	EXPECT_EQ(site.foodOnTheWay, -12);
	EXPECT_FLOAT_EQ(site.chants.battery, 120.0f);
	EXPECT_FLOAT_EQ(site.chants.sacrificed, 7.5f);
	EXPECT_FLOAT_EQ(site.chants.available, 130.0f);
	EXPECT_FLOAT_EQ(site.chants.used, 10.0f);
	EXPECT_FLOAT_EQ(site.chants.requested, 14.0f);
	EXPECT_FLOAT_EQ(site.chants.chantsPerDancer, 0.5f);
	EXPECT_FLOAT_EQ(site.chants.strain, 0.4f);
	EXPECT_TRUE(site.chants.infinite);
	EXPECT_FALSE(site.chants.freeMaintenance);
	ASSERT_TRUE(site.needs.has_value());
	EXPECT_EQ(*site.needs, (std::array<float, world_import::k_SiteNeedFlags> {0.1f, 0.2f, 0.3f}));
	ASSERT_TRUE(site.altar.has_value());
	EXPECT_EQ(site.altar->number, 7U);
	EXPECT_FLOAT_EQ(site.altar->built, 1.0f);
	EXPECT_FLOAT_EQ(site.altar->life, 0.75f);
	EXPECT_EQ(site.footpathLink, 40U);
	EXPECT_EQ(site.goHome, 0U);
}

// The spell seeds come right before the animals in the order that the game loads its tables
TEST(VanillaWorldImport, SpellSeedsCountBackFromTheirFirstRow)
{
	EXPECT_EQ(world_import::SpellSeedRowOf(world_import::k_FirstSpellSeedRowNumber), SpellSeedType::Storm);
	EXPECT_EQ(world_import::SpellSeedRowOf(2549), SpellSeedType::BeamExplosion);
	EXPECT_EQ(world_import::SpellSeedRowOf(2548), std::nullopt);
	EXPECT_EQ(world_import::SpellSeedRowOf(world_import::k_FirstSpellSeedRowNumber + 1), std::nullopt);
}

// The dance and the spell icons of a worship site come with their values
TEST(VanillaWorldImport, ReadsTheDanceAndTheIconsOfAWorshipSite)
{
	static constexpr auto k_Seed = [](SpellSeedType seed) {
		return world_import::k_FirstSpellSeedRowNumber - static_cast<uint32_t>(seed);
	};
	auto fake = GameAtTurn(100);
	// 2: the site, 3: its dance, 4 and 5: its icons, 6: the player, 7: its interface, 8: the status of its hand
	fake.Record(VanillaType::WorshipSite)
	    .Ref("partCitadel", 30)
	    .Value<uint32_t>("siteTribe", world_import::k_FirstTribeRowNumber - static_cast<uint32_t>(Tribe::NORSE))
	    .Ref("siteDance", 3)
	    .Refs("siteIcons", {4, 5})
	    .Value<uint8_t>("sitePlace", 2);
	fake.Record(VanillaType::Dance)
	    .Value<float>("danceClock", 37.0f)
	    .Value<uint32_t>("danceLoopLength", 4)
	    .Value<uint32_t>("danceDancers", 6)
	    .Value<float>("danceRate", 1.4f)
	    .Value<float>("danceDancingRate", 1.0f)
	    .Value<float>("danceAngle", 2.5f)
	    .Value<uint32_t>("danceCameraWanted", 1)
	    .Value<uint32_t>("danceCameraMade", 0)
	    .Value<uint32_t>("danceHandTurns", 101)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::CitadelDance_3))
	    .Value<float>("danceSpeed", 0.5f)
	    .Value<uint32_t>("danceState", 0)
	    .Value<uint32_t>("danceDuration", 0)
	    .Value<uint32_t>("danceStartTurn", 80)
	    .Value<uint32_t>("danceFirstDancerCame", 1)
	    .Value<uint32_t>("danceFirstDancerTurn", 82)
	    .Value<uint32_t>("danceOnTheirWay", 3);
	fake.Record(VanillaType::WorshipSpellIcon)
	    .Value<uint32_t>("iconSeed", k_Seed(SpellSeedType::Water))
	    .Value<uint16_t>("iconRemoveTurns", 0)
	    .Ref("iconSite", 2)
	    .Value<uint32_t>("iconChargeStart", 95)
	    .Value<uint16_t>("iconSlot", 11)
	    .Value<uint8_t>("iconCharging", 1)
	    .Value<uint32_t>("iconPowerUp", 1)
	    .Ref("iconCharger", 8)
	    .Value<float>("iconStore", 42.5f)
	    .Refs("iconSeedsOut", {40});
	fake.Record(VanillaType::WorshipSpellIcon)
	    .Value<uint32_t>("iconSeed", k_Seed(SpellSeedType::Food))
	    .Ref("iconSite", 2)
	    .Value<uint16_t>("iconSlot", 10)
	    .Value<uint8_t>("iconCharging", 0)
	    .Value<uint32_t>("iconPowerUp", 0xFFFFFFFFU)
	    .Ref("iconCharger", 0)
	    .Value<float>("iconStore", 0.0f)
	    .Refs("iconSeedsOut", {});
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Refs("interfaces", {7});
	fake.Record(VanillaType::PlayerInterface);
	fake.Record(VanillaType::PlayerInterfaceStatus).Ref("interface", 7);
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.worshipSites.size(), 1U);
	const auto& site = imported.worshipSites[0];
	ASSERT_TRUE(site.danceValues.has_value());
	const auto& dance = *site.danceValues;
	EXPECT_EQ(dance.number, 3U);
	EXPECT_EQ(dance.type, DanceInfo::CitadelDance_3);
	EXPECT_FLOAT_EQ(dance.clock, 37.0f);
	EXPECT_EQ(dance.loopLength, 4U);
	EXPECT_FLOAT_EQ(dance.speed, 0.5f);
	EXPECT_FLOAT_EQ(dance.rate, 1.4f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.0f);
	EXPECT_FLOAT_EQ(dance.angle, 2.5f);
	EXPECT_FALSE(dance.stopped);
	EXPECT_EQ(dance.duration, 0U);
	EXPECT_EQ(dance.startTurn, 80U);
	EXPECT_EQ(dance.firstDancerTurn, 82U);
	EXPECT_EQ(dance.dancers, 6U);
	EXPECT_EQ(dance.onTheirWay, 3U);
	EXPECT_EQ(dance.handTurns, 101U);
	EXPECT_TRUE(dance.cameraWanted);
	EXPECT_FALSE(dance.cameraMade);
	ASSERT_EQ(site.iconValues.size(), 2U);
	const auto& water = site.iconValues[0];
	EXPECT_EQ(water.number, 4U);
	EXPECT_EQ(water.seed, SpellSeedType::Water);
	EXPECT_EQ(water.slot, 11);
	EXPECT_TRUE(water.charging);
	EXPECT_EQ(water.powerUp, 1);
	EXPECT_EQ(water.charger, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(water.chargeStartTurn, 95U);
	EXPECT_FLOAT_EQ(water.store, 42.5f);
	EXPECT_EQ(water.seedsOut, (std::vector<uint32_t> {40}));
	EXPECT_EQ(water.removeTurns, 0U);
	const auto& food = site.iconValues[1];
	EXPECT_EQ(food.seed, SpellSeedType::Food);
	EXPECT_FALSE(food.charging);
	EXPECT_EQ(food.powerUp, -1);
	EXPECT_EQ(food.charger, std::nullopt);
	EXPECT_TRUE(food.seedsOut.empty());
}

// A dance with no first dancer keeps no turn of its first dancer, and a stopped dance stays stopped
TEST(VanillaWorldImport, ADanceWithNoFirstDancerHasNoTurnForIt)
{
	auto fake = GameAtTurn(100);
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::Worship))
	    .Value<uint32_t>("danceState", 1)
	    .Value<uint32_t>("danceFirstDancerCame", 0)
	    .Value<uint32_t>("danceFirstDancerTurn", 55);
	fake.Record(VanillaType::Dance).Value<uint32_t>("danceRow", 25);
	const auto world = fake.Make();
	const auto dance = world_import::ReadDance(world, *world.Record(2));
	ASSERT_TRUE(dance.has_value());
	EXPECT_TRUE(dance->stopped);
	EXPECT_EQ(dance->firstDancerTurn, std::nullopt);
	EXPECT_FALSE(world_import::ReadDance(world, *world.Record(3)).has_value());
}

// A dance and an icon take the values of the file. The hand of a game with one player is the hand of the first god.
TEST(VanillaWorldImport, ADanceAndAnIconTakeTheValuesOfTheFile)
{
	ecs::components::Dance dance;
	dance.dancers = 2;
	world_import::PutDanceValues(dance, {.clock = 12.0f,
	                                     .loopLength = 3,
	                                     .speed = 0.75f,
	                                     .rate = 2.2f,
	                                     .dancingRate = 1.8f,
	                                     .angle = 1.5f,
	                                     .stopped = false,
	                                     .duration = 500,
	                                     .startTurn = 40,
	                                     .firstDancerTurn = 44,
	                                     .dancers = 9,
	                                     .onTheirWay = 4,
	                                     .handTurns = 7,
	                                     .cameraWanted = true,
	                                     .cameraMade = true});
	EXPECT_FLOAT_EQ(dance.clock, 12.0f);
	EXPECT_EQ(dance.loopLength, 3U);
	EXPECT_FLOAT_EQ(dance.speed, 0.75f);
	EXPECT_FLOAT_EQ(dance.rate, 2.2f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.8f);
	EXPECT_FLOAT_EQ(dance.angle, 1.5f);
	EXPECT_EQ(dance.state, ecs::components::Dance::State::Dancing);
	EXPECT_EQ(dance.duration, 500U);
	EXPECT_EQ(dance.startTurn, 40U);
	EXPECT_EQ(dance.firstDancerTurn, 44U);
	// The dancers stay as the dance has them
	EXPECT_EQ(dance.dancers, 2U);
	EXPECT_EQ(dance.onTheirWay, 0U);
	EXPECT_EQ(dance.hands[0].nearTurns, 7U);
	EXPECT_TRUE(dance.hands[0].cameraWanted);
	EXPECT_TRUE(dance.hands[0].cameraMade);
	EXPECT_FALSE(dance.hands[0].cameraAsked);
	EXPECT_EQ(dance.hands[1].nearTurns, 0U);

	ecs::components::WorshipIcon icon {.seed = SpellSeedType::Heal};
	world_import::PutIconValues(icon, {.seed = SpellSeedType::Heal,
	                                   .charging = true,
	                                   .powerUp = 2,
	                                   .charger = PlayerNames::PLAYER_ONE,
	                                   .chargeStartTurn = 60,
	                                   .store = 15.0f});
	EXPECT_TRUE(icon.charging);
	EXPECT_EQ(icon.powerUp, 2);
	EXPECT_EQ(icon.charger, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(icon.chargeStartTurn, 60U);
	EXPECT_FLOAT_EQ(icon.store, 15.0f);
	// An icon of the file that charges for no hand charges for no player
	world_import::PutIconValues(icon, {.seed = SpellSeedType::Heal});
	EXPECT_FALSE(icon.charging);
	EXPECT_EQ(icon.powerUp, -1);
	EXPECT_EQ(icon.charger, PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(icon.store, 0.0f);
}

// Each spell icon of a saved game of Land 1 has a spell seed of the table, and each dance of the saves has a row of
// the table of the dances
TEST(VanillaWorldImport, SavedGamesOfLandOneHaveASpellSeedForEachIconAndARowForEachDance)
{
	std::map<SpellSeedType, size_t> seeds;
	size_t dances = 0;
	const auto files = ForEachLandOneSave([&seeds, &dances](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& record : world.Records())
		{
			if (record.Is(VanillaType::TownCentreSpellIcon) || record.Is(VanillaType::TownSpellIcon) ||
			    record.Is(VanillaType::WorshipSpellIcon))
			{
				const auto number = world.Word(record, "iconSeed").value_or(0);
				const auto seed = world_import::SpellSeedRowOf(number);
				ASSERT_TRUE(seed.has_value()) << path << " " << record.number << " " << number;
				++seeds[*seed];
			}
			if (record.Is(VanillaType::Dance))
			{
				++dances;
				EXPECT_TRUE(world_import::ReadDance(world, record).has_value()) << path << " " << record.number;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	// The saves of Land 1 here have no spell icon and no dance: no save has a worship site
	size_t icons = 0;
	for (const auto& [seed, count] : seeds)
	{
		icons += count;
	}
	std::cout << icons << " spell icons and " << dances << " dances in the saves\n";
}

// A site with a tribe that the table does not have is not taken
TEST(VanillaWorldImport, AWorshipSiteWithNoTribeOfTheTableIsNotTaken)
{
	auto fake = GameAtTurn(100);
	fake.Record(VanillaType::WorshipSite)
	    .Ref("partCitadel", 30)
	    .Value<uint32_t>("siteTribe", world_import::k_FirstTotemStatueRowNumber)
	    .Value<uint8_t>("sitePlace", 0);
	const auto imported = world_import::ReadTowns(fake.Make());
	EXPECT_TRUE(imported.worshipSites.empty());
	EXPECT_EQ(imported.skipped.size(), 1U);
}

// A pile whose building is the workshop is not taken twice
TEST(VanillaWorldImport, TheWoodPileOfAWorkshopComesOnce)
{
	static constexpr auto k_PotRow = [](PotInfo row) { return world_import::k_FirstPotRowNumber - static_cast<uint32_t>(row); };
	auto fake = GameAtTurn(100);
	fake.Record(VanillaType::Town).Position(0, 0).Value<uint32_t>("id", 0).Refs("abodes", {3});
	fake.Record(VanillaType::Workshop)
	    .Position(1000, 2000)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseWorkshop))
	    .Ref("town", 2)
	    .Refs("villagers", {})
	    .Ref("workshopWoodPile", 4)
	    .Refs("workshopScaffolds", {});
	fake.Record(VanillaType::MagicWood)
	    .Position(1100, 2000)
	    .Value("info", k_PotRow(PotInfo::MagicWood))
	    .Value<uint32_t>("potAmount", 10)
	    .Ref("potBuilding", 3)
	    .Value("pileRise", std::array<float, 12> {});
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.abodes.size(), 1U);
	EXPECT_EQ(imported.abodes[0].piles.size(), 1U);
}

TEST(VanillaWorldImport, TheFlagsOfAScaffoldGiveEachValue)
{
	constexpr auto k_None = world_import::ScaffoldFlagsOf(0);
	EXPECT_FALSE(k_None.grows);
	EXPECT_FALSE(k_None.looked);
	EXPECT_EQ(k_None.noOfferTurns, 0U);
	EXPECT_EQ(k_None.place, 0U);
	EXPECT_FALSE(k_None.foundsTown);
	EXPECT_FALSE(k_None.fixedAngle);
	EXPECT_FALSE(k_None.gentle);
	EXPECT_FALSE(k_None.destroysThingsInWay);
	// The phantom grows, 9 turns with no offer, the place 2, a fixed angle and the destroy flag
	constexpr auto k_Some = world_import::ScaffoldFlagsOf(0x0001 | (9 << 2) | (2 << 6) | 0x0200 | 0x0800);
	EXPECT_TRUE(k_Some.grows);
	EXPECT_FALSE(k_Some.looked);
	EXPECT_EQ(k_Some.noOfferTurns, 9U);
	EXPECT_EQ(k_Some.place, 2U);
	EXPECT_FALSE(k_Some.foundsTown);
	EXPECT_TRUE(k_Some.fixedAngle);
	EXPECT_FALSE(k_Some.gentle);
	EXPECT_TRUE(k_Some.destroysThingsInWay);
	// It looked for an offer, the place 1, it founds a town and the hand let it go gently
	constexpr auto k_Other = world_import::ScaffoldFlagsOf(0x0002 | (1 << 6) | 0x0100 | 0x0400);
	EXPECT_FALSE(k_Other.grows);
	EXPECT_TRUE(k_Other.looked);
	EXPECT_EQ(k_Other.place, 1U);
	EXPECT_TRUE(k_Other.foundsTown);
	EXPECT_TRUE(k_Other.gentle);
	EXPECT_FALSE(k_Other.destroysThingsInWay);
	EXPECT_EQ(world_import::k_ScaffoldPutDownTurnAfterLoad, 1U);
}

// A scaffold keeps its place, its size, its town, its site, the buildings that its phantom offers, the player whose hand
// holds it, the values of its phantom, its flags and the building that a script gave it
TEST(VanillaWorldImport, ReadsTheScaffoldsWithTheirValues)
{
	auto fake = GameAtTurn(100);
	// 2: the player with its interface 3, 4: the plan that the phantom shows, 5 and 6: two scaffolds
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Refs("towns", {}).Refs("interfaces", {3});
	fake.Record(VanillaType::PlayerInterfaceStatus);
	fake.Record(VanillaType::PlannedAbode)
	    .Position(500, 600)
	    .Value("info", AbodeRowNumber(AbodeInfo::NorseCreche))
	    .Value("heading", 1.25f)
	    .Value("scale", 0.75f);
	fake.Record(VanillaType::Scaffold)
	    .Position(1000, 2000)
	    .Value("heading", 0.5f)
	    .Value("scale", 1.4f)
	    .Ref("scaffoldSite", 0)
	    .Ref("scaffoldOffer", 4)
	    .Ref("scaffoldNextOffer", 0)
	    .Value<int32_t>("scaffoldChangeMs", 250)
	    .Ref("scaffoldHolder", 3)
	    .Value("scaffoldAngle", 2.5f)
	    .Value("scaffoldScale", 1.5f)
	    .Value<uint16_t>("scaffoldFlags", 0x0001 | (3 << 2))
	    .Ref("scaffoldTown", 0)
	    .Value<uint32_t>("scaffoldScriptBuilding", 0xFFFFFFFFU);
	fake.Record(VanillaType::Scaffold)
	    .Position(3000, 4000)
	    .Value("heading", 0.0f)
	    .Value("scale", 0.7f)
	    .Ref("scaffoldSite", 8)
	    .Ref("scaffoldOffer", 0)
	    .Ref("scaffoldNextOffer", 0)
	    .Value<int32_t>("scaffoldChangeMs", 0)
	    .Ref("scaffoldHolder", 0)
	    .Value("scaffoldAngle", 0.0f)
	    .Value("scaffoldScale", 0.7f)
	    .Value<uint16_t>("scaffoldFlags", 2 << 6)
	    .Ref("scaffoldTown", 7)
	    .Value<uint32_t>("scaffoldScriptBuilding", static_cast<uint32_t>(AbodeNumber::Creche));
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.scaffolds.size(), 2U);
	const auto& held = imported.scaffolds[0];
	EXPECT_EQ(held.number, 5U);
	EXPECT_EQ(held.position.x, 1000);
	EXPECT_EQ(held.position.z, 2000);
	EXPECT_EQ(held.heading, 0.5f);
	EXPECT_EQ(held.scale, 1.4f);
	EXPECT_EQ(held.town, 0U);
	EXPECT_EQ(held.site, 0U);
	ASSERT_TRUE(held.offer.has_value());
	EXPECT_EQ(held.offer->row, AbodeInfo::NorseCreche);
	EXPECT_EQ(held.offer->heading, 1.25f);
	EXPECT_EQ(held.offer->scale, 0.75f);
	EXPECT_FALSE(held.nextOffer.has_value());
	EXPECT_EQ(held.changeMs, 250);
	EXPECT_EQ(held.handPlayer, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(held.phantomAngle, 2.5f);
	EXPECT_EQ(held.phantomScale, 1.5f);
	EXPECT_TRUE(held.flags.grows);
	EXPECT_EQ(held.flags.noOfferTurns, 3U);
	EXPECT_FALSE(held.scriptBuilding.has_value());
	const auto& onSite = imported.scaffolds[1];
	EXPECT_EQ(onSite.town, 7U);
	EXPECT_EQ(onSite.site, 8U);
	EXPECT_FALSE(onSite.offer.has_value());
	EXPECT_FALSE(onSite.handPlayer.has_value());
	EXPECT_EQ(onSite.flags.place, 2U);
	EXPECT_EQ(onSite.scriptBuilding, AbodeNumber::Creche);
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

// Each animal of the saves of Land 1 is a kind that openblack makes, and keeps its state, the state that it goes into
// next and the state of its move: no animal decides again after the import. The follow states of each flock are
// states of openblack too.
TEST(VanillaSceneryImport, SavedGamesOfLandOneHaveNoAnimalThatDecidesAgain)
{
	using openblack::ecs::systems::animal_detail::AnimalStateOfGame;
	size_t count = 0;
	size_t birds = 0;
	size_t grazers = 0;
	size_t staged = 0;
	size_t noMove = 0;
	size_t wandering = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadAnimals(world);
		for (const auto& animal : imported.animals)
		{
			++count;
			const bool bird = animals::birds::IsLandBird(animal.row);
			const bool grazer = animals::grazers::IsGrazer(animal.row);
			birds += bird ? 1 : 0;
			grazers += grazer ? 1 : 0;
			EXPECT_TRUE(bird || grazer) << path << " " << animal.number << " kind " << static_cast<int>(animal.row);
			EXPECT_TRUE(AnimalStateOfGame(animal.state).has_value())
			    << path << " " << animal.number << " state " << static_cast<int>(animal.state);
			EXPECT_TRUE(AnimalStateOfGame(animal.finalState).has_value())
			    << path << " " << animal.number << " next state " << static_cast<int>(animal.finalState);
			// A move state with no stage of openblack is only the move that a script has not set up yet, on an animal
			// that a script holds, or the wander move of a grazer that wanders. openblack reads no stage in these two
			// states: the next move that the animal sets up gives the stage, as the game sets the move state then.
			constexpr uint8_t k_NoMove = 0;
			constexpr uint8_t k_WanderMove = 5;
			constexpr uint8_t k_InScript = 4;
			constexpr uint8_t k_Wander = 32;
			if (animals::MoveStageOfGame(animal.moveState).has_value())
			{
				++staged;
			}
			else if (animal.moveState == k_NoMove)
			{
				++noMove;
				EXPECT_EQ(animal.state, k_InScript) << path << " " << animal.number;
			}
			else
			{
				++wandering;
				EXPECT_EQ(animal.moveState, k_WanderMove) << path << " " << animal.number;
				EXPECT_EQ(animal.state, k_Wander) << path << " " << animal.number;
				EXPECT_TRUE(grazer) << path << " " << animal.number;
			}
		}
		for (const auto& flock : imported.flocks)
		{
			EXPECT_TRUE(AnimalStateOfGame(flock.followState).has_value()) << path << " " << flock.number;
			EXPECT_TRUE(flock.afterMove == 0 || AnimalStateOfGame(flock.afterMove).has_value()) << path << " " << flock.number;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(count, 0U);
	EXPECT_GT(staged, 0U);
	std::cout << "Animals " << count << ", birds " << birds << ", grazers " << grazers << ", with a move stage " << staged
	          << ", held by a script with no move " << noMove << ", wander move " << wandering << '\n'
	          << std::flush;
}

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

// The effect of a spell is taken only for that spell, which steps it
TEST(VanillaParticleImport, TheEffectOfASpellIsTakenOnlyForItsSpell)
{
	auto fake = GameAtTurn(5);
	// 2: the system, 3: the manager of the spell 9
	fake.Record(VanillaType::ParticleSystem).Ref("systemManager", 3);
	fake.Record(VanillaType::ParticleManager)
	    .Value("managerAtomCount", uint32_t {0})
	    .Ref("managerPlayer", 0)
	    .Refs("managerRoots", {})
	    .Value("managerInfo", std::array<float, 15> {0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
	                                                 1.0f, 0.0f, std::bit_cast<float>(uint32_t {1})})
	    .Value("managerAge", 2.5f)
	    .Ref("managerSpell", 9)
	    .Value("managerType", uint32_t {20});
	const auto world = fake.Make();
	EXPECT_FALSE(particle_import::ReadEffect(world, 2, SameNumber).effect.has_value());
	EXPECT_FALSE(particle_import::ReadSpellEffect(world, 2, 8, SameNumber).effect.has_value());
	const auto read = particle_import::ReadSpellEffect(world, 2, 9, SameNumber);
	ASSERT_TRUE(read.effect.has_value()) << read.why;
	EXPECT_EQ(read.effect->type, static_cast<ParticleType>(20));
	EXPECT_EQ(read.effect->state.age, 2.5f);
	EXPECT_EQ(read.effect->state.info.handPosition, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(read.effect->state.info.enabled);
}

// Each atom of a fireball keeps what its rules keep, as openblack keeps it. The sounds of the atom do not play after a
// load, so the import does not make them. A rule that lets out atoms along the path of the parent keeps its point.
TEST(VanillaParticleImport, TheAtomsOfAFireballKeepWhatTheirRulesKeep)
{
	auto fake = GameAtTurn(5);
	// 2: the system, 3: the manager of the spell 20, 4: the collection, 5: the atom, 6: its sprite, 7..12: the data of
	// its rules, 13: its sound, 14: the data of the path rule, 15: the ball
	fake.Record(VanillaType::ParticleSystem).Ref("systemManager", 3);
	fake.Record(VanillaType::ParticleManager)
	    .Value("managerAtomCount", uint32_t {1})
	    .Ref("managerPlayer", 0)
	    .Refs("managerRoots", {4})
	    .Value("managerInfo", std::array<float, 15> {})
	    .Ref("managerSpell", 20)
	    .Value("managerType", uint32_t {43});
	fake.Record(VanillaType::AtomCollection)
	    .Refs("ruleData", {14})
	    .Value("collectionGroup", uint32_t {0})
	    .Value("collectionFlags", uint8_t {0})
	    .Refs("collectionAtoms", {5})
	    .Ref("collectionUnused", 0);
	const std::array<float, 14> draw {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
	fake.Record(VanillaType::AtomCore)
	    .Refs("atomCollections", {})
	    .Refs("atomRuleData", {7, 8, 9, 10, 11, 12})
	    .Refs("atomSounds", {13})
	    .Value("atomBirth", 2.0f)
	    .Value("atomRotation", std::array<float, 12> {})
	    .Value("atomPrevious", draw)
	    .Value("atomCurrent", draw)
	    .Value("atomAnimation", std::array<uint32_t, 3> {})
	    .Ref("atomDrawOffset", 0)
	    .Ref("atomRender", 6);
	fake.Record(VanillaType::SpriteParticle).Value("spriteCreator", uint32_t {3});
	fake.Record(VanillaType::AtomDataFadeOutOnceConditionTrue)
	    .Value("atomDataRule", uint32_t {11})
	    .Value("fadeOutStarted", uint8_t {1})
	    .Value("fadeOutAge", 0.5f)
	    .Value("fadeOutAlpha", 200.0f)
	    .Value("fadeOutScale", 1.25f);
	fake.Record(VanillaType::AtomDataAttachFireBall)
	    .Value("atomDataRule", uint32_t {12})
	    .Value("fireBallNotMade", uint8_t {0})
	    .Value("fireBallTurn", uint32_t {4})
	    .Ref("fireBallObject", 15);
	fake.Record(VanillaType::AtomDataRipple)
	    .Value("atomDataRule", uint32_t {13})
	    .Value("rippleLast", std::array<float, 3> {7.0f, 8.0f, 9.0f});
	fake.Record(VanillaType::AtomDataAddSubCollections)
	    .Value("atomDataRule", uint32_t {14})
	    .Value("subCollectionsAdded", uint8_t {1});
	fake.Record(VanillaType::AtomDataSideSpin)
	    .Value("atomDataRule", uint32_t {15})
	    .Value("sideSpin", -0.5f)
	    .Value("sideSpinNotStarted", uint8_t {0});
	fake.Record(VanillaType::AtomDataRemoveRuleAfterConditionTrue)
	    .Value("atomDataRule", uint32_t {16})
	    .Value("removeStarted", uint8_t {1})
	    .Value("removeAge", 0.25f);
	fake.Record(VanillaType::ParticleSound);
	fake.Record(VanillaType::CollectionDataWillowWisp)
	    .Value("dataRule", uint32_t {17})
	    .Value("wispLast", std::array<float, 3> {1.0f, 2.0f, 3.0f})
	    .Value("wispEmitted", 4.5f)
	    .Value("wispMade", 4.0f)
	    .Value("wispFirst", uint8_t {0});
	fake.Record(VanillaType::MagicFireBall);
	const auto world = fake.Make();

	const auto read = particle_import::ReadSpellEffect(world, 2, 20, SameNumber);
	ASSERT_TRUE(read.effect.has_value()) << read.why;
	ASSERT_EQ(read.effect->state.roots.size(), 1U);
	const auto& collection = read.effect->state.roots.front();
	ASSERT_EQ(collection.pathEmitters.size(), 1U);
	EXPECT_EQ(collection.pathEmitters.front().rule, 17);
	EXPECT_EQ(collection.pathEmitters.front().last, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(collection.pathEmitters.front().emitted, 4.5f);
	EXPECT_EQ(collection.pathEmitters.front().made, 4.0f);
	EXPECT_FALSE(collection.pathEmitters.front().first);
	ASSERT_EQ(collection.atoms.size(), 1U);
	const auto& rules = collection.atoms.front().ruleData;
	ASSERT_EQ(rules.size(), 6U);
	EXPECT_EQ(rules[0].rule, 11);
	EXPECT_EQ(rules[0].className, "AR_FadeOutOnceConditionTrue");
	EXPECT_TRUE(rules[0].data.started);
	EXPECT_EQ(rules[0].data.a, glm::vec4(0.5f, 200.0f, 1.25f, 0.0f));
	EXPECT_EQ(rules[1].className, "AttatchFireBallToAtom");
	EXPECT_TRUE(rules[1].data.started);
	EXPECT_EQ(rules[1].data.object, SameNumber(15));
	// The point of the ring is on the map, across the land
	EXPECT_EQ(rules[2].className, "UpdateRuleGravityWithFloor");
	EXPECT_EQ(rules[2].data.a.x, 7.0f);
	EXPECT_EQ(rules[2].data.a.y, 9.0f);
	EXPECT_EQ(rules[3].className, "AddSubCollectionsToAtom");
	EXPECT_TRUE(rules[3].data.started);
	EXPECT_EQ(rules[4].className, "UR_SideSpin");
	EXPECT_TRUE(rules[4].data.started);
	EXPECT_EQ(rules[4].data.a.x, -0.5f);
	// The file keeps the age of the atom, and openblack keeps the age of the effect: the import adds the birth
	EXPECT_EQ(rules[5].className, "RemoveRuleAfterConditionTrue");
	EXPECT_TRUE(rules[5].data.started);
	EXPECT_EQ(rules[5].data.a.x, 2.25f);
}
