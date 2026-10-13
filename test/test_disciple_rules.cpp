/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "Camera/CameraLens.h"
#include "ECS/DiscipleRules.h"

using namespace openblack;
using namespace openblack::disciple;

namespace
{
using enum VillagerDisciple;

constexpr auto k_Me = PlayerNames::PLAYER_ONE;
constexpr auto k_Them = PlayerNames::PLAYER_TWO;

/// One of my adults held in my hand
HeldFacts MyAdult()
{
	return {.hand = k_Me, .villager = k_Me, .sexuallyActive = true, .male = true};
}

/// A built, whole building of one of my towns
NearFacts MyBuilding(NearKind kind)
{
	return {.kind = kind, .owner = k_Me, .hasTown = true, .heldVillagersTown = true};
}

/// The same of their town
NearFacts TheirBuilding(NearKind kind)
{
	return {.kind = kind, .owner = k_Them, .hasTown = true};
}
} // namespace

TEST(DiscipleRules, InterestFallsAwayToNothingBySixMetres)
{
	// Touching, a thing draws as much as its kind
	EXPECT_NEAR(Interest(0.0f, 1.0f), 1.0f, 1e-3f);
	// Overlapping counts as touching
	EXPECT_FLOAT_EQ(Interest(-2.0f, 1.0f), Interest(0.0f, 1.0f));
	EXPECT_LT(Interest(3.0f, 1.0f), Interest(1.0f, 1.0f));
	// At six metres and beyond, too little to look at
	EXPECT_LT(Interest(6.0f, 1.5f), k_LeastInterest);
	EXPECT_LT(Interest(20.0f, 1.5f), k_LeastInterest);
	// A kind that draws villagers more draws them more at the same gap
	EXPECT_GT(Interest(1.0f, 1.5f), Interest(1.0f, 0.9f));
}

TEST(DiscipleRules, MostInterestingFirstTiesInTheOrderFound)
{
	const std::array<float, 5> interests {0.5f, 0.9f, 0.0001f, 0.9f, 0.25f};
	EXPECT_EQ(OrderByInterest(interests), (std::vector<size_t> {1, 3, 0, 4}));
}

TEST(DiscipleRules, FieldsMakeFarmersOrBuilders)
{
	auto field = MyBuilding(NearKind::Field);
	EXPECT_EQ(Offered(field, MyAdult()), Farmer);
	field.built = false;
	EXPECT_EQ(Offered(field, MyAdult()), Builder);
	// Anyone's field farms, but only the player's own people
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Field), MyAdult()), Farmer);
	EXPECT_EQ(Offered(MyBuilding(NearKind::Field), {.hand = k_Me, .villager = k_Them}), None);
}

TEST(DiscipleRules, TreesForestsAndFishFarms)
{
	EXPECT_EQ(Offered({.kind = NearKind::Tree}, MyAdult()), Forester);
	EXPECT_EQ(Offered({.kind = NearKind::Forest}, MyAdult()), Forester);
	EXPECT_EQ(Offered({.kind = NearKind::FishFarm}, MyAdult()), Fisherman);
	EXPECT_EQ(Offered({.kind = NearKind::Tree}, {.hand = k_Me, .villager = std::nullopt}), None);
}

TEST(DiscipleRules, OwnBuildingsAreBuiltUpAndOthersPreachedTo)
{
	auto house = MyBuilding(NearKind::Building);
	EXPECT_EQ(Offered(house, MyAdult()), None);
	house.repaired = false;
	EXPECT_EQ(Offered(house, MyAdult()), Builder);
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Building), MyAdult()), Missionary);
	// A neutral town is another player's as much as an enemy's
	EXPECT_EQ(Offered({.kind = NearKind::Building, .owner = PlayerNames::NEUTRAL, .hasTown = true}, MyAdult()), Missionary);
	// A building of no town, or of a town no one can live in, offers nothing
	EXPECT_EQ(Offered({.kind = NearKind::Building, .owner = PlayerNames::NEUTRAL}, MyAdult()), None);
	auto empty = TheirBuilding(NearKind::Building);
	empty.townHabitable = false;
	EXPECT_EQ(Offered(empty, MyAdult()), None);
}

TEST(DiscipleRules, StoragePitsTradeWorkshopsCraft)
{
	EXPECT_EQ(Offered(TheirBuilding(NearKind::StoragePit), MyAdult()), Trader);
	EXPECT_EQ(Offered(MyBuilding(NearKind::StoragePit), MyAdult()), None);
	EXPECT_EQ(Offered(MyBuilding(NearKind::Workshop), MyAdult()), Craftsman);
	EXPECT_EQ(Offered(TheirBuilding(NearKind::Workshop), MyAdult()), Missionary);
	auto unfinished = MyBuilding(NearKind::Workshop);
	unfinished.built = false;
	EXPECT_EQ(Offered(unfinished, MyAdult()), Builder);
}

TEST(DiscipleRules, HousesWithRoomTakeInOthers)
{
	auto house = MyBuilding(NearKind::House);
	house.roomForAdults = true;
	EXPECT_EQ(Offered(house, MyAdult()), ChangeHouse);
	house.heldVillagersHome = true;
	EXPECT_EQ(Offered(house, MyAdult()), None);
	house.heldVillagersHome = false;
	house.roomForAdults = false;
	EXPECT_EQ(Offered(house, MyAdult()), None);
	// Even another player's villager moves into one of the player's houses
	house.roomForAdults = true;
	EXPECT_EQ(Offered(house, {.hand = k_Me, .villager = k_Them}), ChangeHouse);
}

TEST(DiscipleRules, AnotherTownsCentreTakesInWhenThereIsRoom)
{
	auto centre = MyBuilding(NearKind::TownCentre);
	EXPECT_EQ(Offered(centre, MyAdult()), None);
	centre.heldVillagersTown = false;
	EXPECT_EQ(Offered(centre, MyAdult()), None);
	centre.townHasRoom = true;
	EXPECT_EQ(Offered(centre, MyAdult()), ChangeHouse);
	// Another player's centre is preached to
	EXPECT_EQ(Offered(TheirBuilding(NearKind::TownCentre), MyAdult()), Missionary);
}

TEST(DiscipleRules, OwnWorshipSiteMakesWorshippers)
{
	auto site = MyBuilding(NearKind::WorshipSite);
	EXPECT_EQ(Offered(site, MyAdult()), None);
	site.heldVillagersWorshipSite = true;
	EXPECT_EQ(Offered(site, MyAdult()), Worship);
	site.built = false;
	EXPECT_EQ(Offered(site, MyAdult()), Builder);
}

TEST(DiscipleRules, BreedersByOneOfTheOtherSex)
{
	NearFacts woman {.kind = NearKind::Villager,
	                 .owner = k_Me,
	                 .villagerFunctional = true,
	                 .villagerSexuallyActive = true,
	                 .villagerMale = false};
	EXPECT_EQ(Offered(woman, MyAdult()), Breeder);
	auto man = woman;
	man.villagerMale = true;
	EXPECT_EQ(Offered(man, MyAdult()), None);
	auto old = woman;
	old.villagerSexuallyActive = false;
	EXPECT_EQ(Offered(old, MyAdult()), None);
	auto theirs = woman;
	theirs.owner = k_Them;
	EXPECT_EQ(Offered(theirs, MyAdult()), None);
}

TEST(DiscipleRules, FirstOfferTakenAndOldWorkKeptWithNone)
{
	const std::array<NearFacts, 3> ordered {MyBuilding(NearKind::Building), NearFacts {.kind = NearKind::Tree},
	                                        MyBuilding(NearKind::Field)};
	const auto choice = ChooseWhileHeld(ordered, MyAdult(), None);
	EXPECT_EQ(choice.type, Forester);
	EXPECT_EQ(choice.from, 1u);
	const auto nothing = ChooseWhileHeld(std::span(ordered).first(1), MyAdult(), Builder);
	EXPECT_EQ(nothing.type, Builder);
	EXPECT_FALSE(nothing.from.has_value());
}

TEST(DiscipleRules, ChildrenOnlyWorship)
{
	auto site = MyBuilding(NearKind::WorshipSite);
	site.heldVillagersWorshipSite = true;
	const std::array<NearFacts, 2> ordered {MyBuilding(NearKind::Field), site};
	auto child = MyAdult();
	child.child = true;
	EXPECT_EQ(ChooseWhileHeld(ordered, child, None).type, Worship);
	EXPECT_EQ(ChooseWhileHeld(std::span(ordered).first(1), child, None).type, None);
	// An adult a script holds takes up nothing, not even worship
	auto held = MyAdult();
	held.otherwiseRefused = true;
	EXPECT_EQ(ChooseWhileHeld(ordered, held, None).type, None);
	// An adult free to choose takes the field
	EXPECT_EQ(ChooseWhileHeld(ordered, MyAdult(), None).type, Farmer);
}

TEST(DiscipleRules, MissionariesPickedUpStopPreaching)
{
	EXPECT_EQ(AtPickUp(Missionary), None);
	EXPECT_EQ(AtPickUp(Farmer), Farmer);
}

TEST(DiscipleRules, KindsAsTheTableHasThem)
{
	EXPECT_TRUE(KindOf(Farmer).glows);
	EXPECT_TRUE(KindOf(Farmer).movesIntoTown);
	EXPECT_EQ(KindOf(Forester).landingState, VillagerStates::ForesterArrivesAtForest);
	EXPECT_EQ(KindOf(Missionary).landingState, VillagerStates::MissionaryDisciple);
	EXPECT_FALSE(KindOf(Missionary).waitsWhenIdle);
	EXPECT_FALSE(KindOf(Worship).glows);
	EXPECT_FALSE(KindOf(Breeder).leadsFollowers);
	EXPECT_EQ(KindOf(Builder).desire, TownDesireInfo::ToBuild);
	EXPECT_FALSE(KindOf(Craftsman).desire.has_value());
}

TEST(DiscipleRules, SymbolsAndSamples)
{
	EXPECT_EQ(SymbolCell(Missionary), 32u);
	EXPECT_EQ(SymbolCell(Fisherman), 38u);
	EXPECT_EQ(SymbolCell(ChangeHouse), 40u);
	EXPECT_EQ(SymbolCell(Trader), SymbolCell(Protection));
	EXPECT_FALSE(SymbolCell(Worship).has_value());
	EXPECT_EQ(MadeSample(Farmer, 0.0f, 0.0f), 6u);
	EXPECT_EQ(MadeSample(Missionary, 0.0f, 0.0f), 13u);
	EXPECT_EQ(MadeSample(ChangeHouse, 0.9f, 0.2f), 18u);
	EXPECT_EQ(MadeSample(ChangeHouse, 0.0f, 0.7f), 17u);
	EXPECT_EQ(MadeSample(ChangeHouse, -0.9f, 0.2f), 16u);
	EXPECT_EQ(MadeSample(ChangeHouse, -0.9f, 0.7f), 15u);
	EXPECT_FALSE(MadeSample(Worship, 0.0f, 0.0f).has_value());
}

/// The symbol's height on the screen, in pixels, through the camera's view on a screen of that size
static float SymbolPixelsOnScreen(float distance, int32_t width, int32_t height)
{
	constexpr float k_Lens = 70.0f * std::numbers::pi_v<float> / 180.0f;
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	const float halfView = distance * std::tan(camera_lens::VerticalFieldOfView(k_Lens, aspect) * 0.5f);
	return SymbolSize(distance, width, height) / halfView * (static_cast<float>(height) * 0.5f);
}

TEST(DiscipleRules, TheSymbolIsTheGamesSizeOnItsDefaultScreen)
{
	// 16 pixels over half of 800, halved
	EXPECT_FLOAT_EQ(SymbolSize(50.0f, 800, 600), 50.0f * (16.0f / 400.0f) * 0.5f);
	EXPECT_FLOAT_EQ(SymbolSize(0.0f, 800, 600), 0.0001f);
}

TEST(DiscipleRules, TheSymbolTakesTheSameShareOfTheScreensFourByThreeMiddleOnEveryScreen)
{
	// Its share of the height of the 4:3 middle, as on the default screen
	const float reference = SymbolPixelsOnScreen(30.0f, 800, 600) / 600.0f;
	constexpr std::array<std::array<int32_t, 2>, 6> k_Screens {
	    {{1280, 1024}, {1600, 1200}, {1920, 1080}, {2560, 1440}, {3440, 1440}, {5120, 1440}}};
	for (const auto& [width, height] : k_Screens)
	{
		const float middleHeight = std::min(static_cast<float>(height), static_cast<float>(width) * 0.75f);
		for (const float distance : {5.0f, 30.0f, 400.0f})
		{
			EXPECT_NEAR(SymbolPixelsOnScreen(distance, width, height) / middleHeight, reference, 1e-5f)
			    << width << "x" << height << " at " << distance;
		}
	}
	// Twice the pixels on a 4:3 screen twice the default's size, where the game drew it the same 16 pixels
	EXPECT_NEAR(SymbolPixelsOnScreen(30.0f, 1600, 1200), 2.0f * SymbolPixelsOnScreen(30.0f, 800, 600), 1e-3f);
}

TEST(DiscipleRules, AlignmentFollowsWhatTheTownWants)
{
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.8f, 0.2f, 0.6f, 0.05f), 0.05f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.1f, 0.2f, 0.6f, 0.05f), -0.05f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(0.4f, 0.2f, 0.6f, 0.05f), 0.0f);
	EXPECT_FLOAT_EQ(AlignmentForMaking(std::nullopt, 0.2f, 0.6f, 0.05f), 0.0f);
	// Leaning good, more good counts for less and evil for more
	EXPECT_FLOAT_EQ(DampedAlignment(0.5f, 0.1f), 0.075f);
	EXPECT_FLOAT_EQ(DampedAlignment(0.5f, -0.1f), -0.125f);
	EXPECT_FLOAT_EQ(DampedAlignment(-0.5f, -0.1f), -0.075f);
}

TEST(DiscipleRules, IdleDisciplesWaitJustBeyondTheCentre)
{
	const auto least = WaitStepFromCentre(1.0f, 3.0f, [](float) { return 0.0f; });
	EXPECT_FLOAT_EQ(least.radians, 1.0f - (std::numbers::pi_v<float> / 4.0f));
	EXPECT_FLOAT_EQ(least.metres, 3.0f);
	const auto most = WaitStepFromCentre(1.0f, 3.0f, [](float limit) { return limit; });
	EXPECT_FLOAT_EQ(most.radians, 1.0f + (std::numbers::pi_v<float> / 4.0f));
	EXPECT_FLOAT_EQ(most.metres, 7.0f);
}

TEST(DiscipleRules, AScriptNamesAWorkByItsNumberOrNoneOutsideTheKinds)
{
	EXPECT_EQ(FromScript(0), None);
	EXPECT_EQ(FromScript(4), Builder);
	EXPECT_EQ(FromScript(12), FromVortex);
	EXPECT_FALSE(FromScript(13).has_value());
	EXPECT_FALSE(FromScript(-1).has_value());
}
