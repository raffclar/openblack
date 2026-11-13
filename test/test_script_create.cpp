/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "InfoConstants.h"
#include "ScriptHeaders/ScriptCreateRules.h"

using namespace openblack;
using namespace openblack::script::create_rules;
using ScriptType = openblack::script::ObjectType;

TEST(ScriptCreate, OnlyTypesFromAMarkerToAnAnimatedStaticCanBeCreated)
{
	EXPECT_FALSE(IsCreatableType(0));
	EXPECT_TRUE(IsCreatableType(static_cast<int32_t>(ScriptType::Marker)));
	EXPECT_TRUE(IsCreatableType(static_cast<int32_t>(ScriptType::Tree)));
	EXPECT_TRUE(IsCreatableType(static_cast<int32_t>(ScriptType::SpellDispenser)));
	EXPECT_TRUE(IsCreatableType(static_cast<int32_t>(ScriptType::AnimatedStatic)));
	EXPECT_FALSE(IsCreatableType(static_cast<int32_t>(ScriptType::SpecialField)));
	EXPECT_FALSE(IsCreatableType(-1));
}

TEST(ScriptCreate, TheScriptsNumberTheTypesAsTheGameDoes)
{
	// The numbers the challenge scripts use: the leash lesson's palm tree is "create 22 8", the missionaries' ark
	// "create 3 69", the phone box "create 41 15", the storms "create 15 n", the seeds "create 30 n" and the
	// dispenser rewards "create ... 36 126"
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Feature), 3);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::WeatherThing), 15);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Tree), 22);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::OneShotSpell), 30);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::OneShotSpellInHand), 31);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::SpellDispenser), 36);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::AnimatedStatic), 41);
	EXPECT_EQ(static_cast<int32_t>(TreeInfo::Palm), 8);
	EXPECT_EQ(static_cast<int32_t>(FeatureInfo::ArkDryDock), 69);
	EXPECT_EQ(static_cast<int32_t>(AnimatedStaticInfo::PhoneBox), 15);
	EXPECT_EQ(static_cast<int32_t>(AbodeInfo::NorseSpellDispenser), 126);
	EXPECT_EQ(static_cast<int32_t>(SpellSeedType::CreatureSpellStrong), 16);
	EXPECT_EQ(static_cast<int32_t>(SpellSeedType::Water), 9);
}

TEST(ScriptCreate, OnlyMarkersAndSomeMobileStaticsKeepTheScriptsHeight)
{
	// A marker (type 1) keeps the height that the script gave. A mobile static (type 8) keeps it only when it can not
	// move in the physics. Villagers (4), children (5) and the rest stand on the land under the place asked for.
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Marker), 1);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::MobileStatic), 8);
	EXPECT_TRUE(KeepsScriptHeight(ScriptType::Marker));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Villager));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::VillagerChild));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Rock));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Creature));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Ball));
}

TEST(ScriptCreate, AMobileStaticThatCanMoveInThePhysicsGoesOntoTheLand)
{
	const auto keeps = [](MobileStaticInfo row) {
		return KeepsScriptHeight(ScriptType::MobileStatic, static_cast<uint32_t>(row));
	};
	// The singing stones go onto the land, also the ones that the circle makes at no height
	EXPECT_FALSE(keeps(MobileStaticInfo::SingingStone_1));
	EXPECT_FALSE(keeps(MobileStaticInfo::Rock));
	EXPECT_FALSE(keeps(MobileStaticInfo::CeltFenceShort));
	// The holders of the singing stones, the lanterns and the bonfires do not move in the physics
	EXPECT_TRUE(keeps(MobileStaticInfo::SingingStoneBase));
	EXPECT_TRUE(keeps(MobileStaticInfo::StreetLantern));
	EXPECT_TRUE(keeps(MobileStaticInfo::CountryLantern));
	EXPECT_TRUE(keeps(MobileStaticInfo::Bonfire));
	// A rock made by its own type goes onto the land, a holder too
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Rock, static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase)));
}

TEST(ScriptCreate, AMobileStaticOfTheRockKindKeepsTheScriptsHeight)
{
	const auto rock = static_cast<uint32_t>(MobileStaticInfo::Rock);
	// Made as a mobile static, a rock stays at the height that the script gave, and the physics lets it fall
	EXPECT_TRUE(KeepsScriptHeight(ScriptType::MobileStatic, rock, true));
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::MobileStatic, rock, false));
	// Made by the rock type, it goes onto the land under its place
	EXPECT_FALSE(KeepsScriptHeight(ScriptType::Rock, rock, true));
}

TEST(ScriptCreate, ARockOfTheRockKindStartsInThePhysics)
{
	const auto rock = static_cast<uint32_t>(MobileStaticInfo::Rock);
	// A mobile static of the rock kind starts in the physics, but is not put on the land first
	const auto asMobileStatic = PhysicsStartOf(ScriptType::MobileStatic, rock, {.rock = true});
	ASSERT_TRUE(asMobileStatic.has_value());
	EXPECT_FALSE(asMobileStatic->settlesOnLand);
	// Made by the rock type, the physics puts it on the land and turns it to the slope
	const auto asRock = PhysicsStartOf(ScriptType::Rock, rock, {.rock = true});
	ASSERT_TRUE(asRock.has_value());
	EXPECT_TRUE(asRock->settlesOnLand);
	// A row out of the rock kind, such as a singing stone, does not start in the physics
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileStatic, static_cast<uint32_t>(MobileStaticInfo::SingingStone_1)));
	// The rows that the physics can not move never start in it, whatever their kind
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileStatic, static_cast<uint32_t>(MobileStaticInfo::Bonfire), {.rock = true}));
	EXPECT_FALSE(PhysicsStartOf(ScriptType::Rock, static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase), {.rock = true}));
	EXPECT_FALSE(
	    PhysicsStartOf(ScriptType::MobileStatic, static_cast<uint32_t>(MobileStaticInfo::StreetLantern), {.rock = true}));
	EXPECT_FALSE(
	    PhysicsStartOf(ScriptType::MobileStatic, static_cast<uint32_t>(MobileStaticInfo::CountryLantern), {.rock = true}));
	// Other types never do
	EXPECT_FALSE(PhysicsStartOf(ScriptType::Feature, rock, {.rock = true}));
}

TEST(ScriptCreate, AOneShotMiracleGlobeStartsInThePhysicsOnTheLand)
{
	// Every globe starts in the physics, and the physics puts it on the land and turns it to the slope
	for (const auto seed : {0u, 3u, 7u, 8u, 9u})
	{
		const auto start = PhysicsStartOf(ScriptType::OneShotSpell, seed);
		ASSERT_TRUE(start.has_value());
		EXPECT_TRUE(start->settlesOnLand);
	}
	// A globe that goes straight into the hand does not
	EXPECT_FALSE(PhysicsStartOf(ScriptType::OneShotSpellInHand, 3));
}

TEST(ScriptCreate, AMobileObjectStartsInThePhysicsOnTheLandAsTheKindOfItsRowTells)
{
	const auto row = [](MobileObjectInfo info) { return static_cast<uint32_t>(info); };
	const auto plain = [](MobileObjectInfo info) { return ScriptRowKind {.mobileObject = info}; };
	// A plain mobile object starts in the physics, and the physics puts it on the land
	for (const auto info : {MobileObjectInfo::EgyptBarrel, MobileObjectInfo::Cart, MobileObjectInfo::Arrow,
	                        MobileObjectInfo::OldScaffold, MobileObjectInfo::OneOffSpellSeed})
	{
		const auto start = PhysicsStartOf(ScriptType::MobileObject, row(info), plain(info));
		ASSERT_TRUE(start.has_value());
		EXPECT_TRUE(start->settlesOnLand);
	}
	// The kind of the row tells the thing, not the number of the row
	EXPECT_TRUE(PhysicsStartOf(ScriptType::MobileObject, row(MobileObjectInfo::OldScaffold), plain(MobileObjectInfo::Champi))
	                .has_value());
	// A crop and a creed do not start in the physics, also when another row has their kind
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileObject, row(MobileObjectInfo::Crop), plain(MobileObjectInfo::Crop)));
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileObject, row(MobileObjectInfo::Creed), plain(MobileObjectInfo::Creed)));
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileObject, row(MobileObjectInfo::Eyes), plain(MobileObjectInfo::Creed)));
	// The three mushroom rows only stand on the land
	for (const auto info : {MobileObjectInfo::Champi, MobileObjectInfo::MagicMushroom, MobileObjectInfo::Toadstool})
	{
		EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileObject, row(info), plain(info)));
	}
	// A row out of the table makes nothing
	EXPECT_FALSE(PhysicsStartOf(ScriptType::MobileObject, row(MobileObjectInfo::_COUNT), {}));
}

TEST(ScriptCreate, PooAndAnArkStartInThePhysicsOnTheLand)
{
	// The lump of poo is poo whatever kind its row has
	for (const auto kind : {MobileObjectInfo::LumpOfPoo, MobileObjectInfo::Crop, MobileObjectInfo::Creed})
	{
		const auto poo = PhysicsStartOf(ScriptType::Poo, 0, {.mobileObject = kind});
		ASSERT_TRUE(poo.has_value());
		EXPECT_TRUE(poo->settlesOnLand);
	}
	EXPECT_TRUE(PhysicsStartOf(ScriptType::MobileObject, static_cast<uint32_t>(MobileObjectInfo::LumpOfPoo),
	                           {.mobileObject = MobileObjectInfo::Crop})
	                .has_value());
	// The ark is a plain mobile object of its own row
	const auto ark = PhysicsStartOf(ScriptType::Ark, 7, {.mobileObject = MobileObjectInfo::Ark});
	ASSERT_TRUE(ark.has_value());
	EXPECT_TRUE(ark->settlesOnLand);
}

TEST(ScriptCreate, ABallAPileAScaffoldAndADeadTreeStartInThePhysicsOnTheLand)
{
	for (const auto type : {ScriptType::Ball, ScriptType::Store, ScriptType::Scaffold, ScriptType::DeadTree})
	{
		const auto start = PhysicsStartOf(type, 0);
		ASSERT_TRUE(start.has_value());
		EXPECT_TRUE(start->settlesOnLand);
	}
}

TEST(ScriptCreate, ThingsThatCanNotBecomeAPhysicsObjectOrOnlyStandOnTheLandDoNotStartInThePhysics)
{
	for (const auto type :
	     {ScriptType::Marker, ScriptType::Feature, ScriptType::Villager, ScriptType::VillagerChild, ScriptType::Animal,
	      ScriptType::Bird, ScriptType::Reward, ScriptType::Creature, ScriptType::WeatherThing, ScriptType::Timer,
	      ScriptType::Tree, ScriptType::Vortex, ScriptType::Whale, ScriptType::OneShotSpellInHand, ScriptType::PuzzleGame,
	      ScriptType::Totem, ScriptType::SpellDispenser, ScriptType::AnimatedStatic})
	{
		EXPECT_FALSE(PhysicsStartOf(type, 0).has_value()) << static_cast<int>(type);
	}
}

TEST(ScriptCreate, AnglesAreGivenInDegrees)
{
	EXPECT_FLOAT_EQ(AngleFromDegrees(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(AngleFromDegrees(180.0f), 3.14159274f);
	EXPECT_FLOAT_EQ(AngleFromDegrees(-90.0f), -1.57079637f);
}

TEST(ScriptCreate, ATreeFacesWithinOneTurn)
{
	EXPECT_FLOAT_EQ(TreeAngle(1.0f), 1.0f);
	EXPECT_NEAR(TreeAngle(7.0f), 7.0f - 6.28318548f, 1e-6f);
	EXPECT_NEAR(TreeAngle(13.0f), 13.0f - 2.0f * 6.28318548f, 1e-5f);
	// Whole turns are taken off towards zero, so a backwards facing stays backwards
	EXPECT_NEAR(TreeAngle(-7.0f), -7.0f + 6.28318548f, 1e-6f);
	EXPECT_FLOAT_EQ(TreeAngle(-1.0f), -1.0f);
}

TEST(ScriptCreate, ASubtypeMustNumberARow)
{
	EXPECT_TRUE(IsRow(0, 3));
	EXPECT_TRUE(IsRow(2, 3));
	EXPECT_FALSE(IsRow(3, 3));
	EXPECT_FALSE(IsRow(0, 0));
}

TEST(ScriptCreate, ARandomVillagerOfATribeIsOneOfItsSevenKinds)
{
	// The villagers' table: nine tribes of seven kinds, then the kinds of no tribe
	constexpr std::size_t k_Rows = 84;
	EXPECT_EQ(TribeVillagerRow(0, 0, k_Rows), 0u);
	EXPECT_EQ(TribeVillagerRow(0, 6, k_Rows), 6u);
	// The Norse (tribe 7): their housewife to their trader
	EXPECT_EQ(TribeVillagerRow(7, 0, k_Rows), static_cast<uint32_t>(VillagerInfo::NorseHousewifeFemale));
	EXPECT_EQ(TribeVillagerRow(7, 6, k_Rows), static_cast<uint32_t>(VillagerInfo::NorseTraderMale));
	EXPECT_FALSE(TribeVillagerRow(-1, 0, k_Rows).has_value());
	EXPECT_FALSE(TribeVillagerRow(0, 7, k_Rows).has_value());
	EXPECT_FALSE(TribeVillagerRow(12, 0, k_Rows).has_value());
}

TEST(ScriptCreate, AScriptsVillagerIsMadeInNoTown)
{
	// A grown-up a year past growing up, a child ten, and neither in a town until the script attaches it to one
	const auto grownUp = ScriptVillagerOf(false, 13);
	EXPECT_EQ(grownUp.age, 14u);
	EXPECT_FALSE(grownUp.joinsTown);
	const auto child = ScriptVillagerOf(true, 13);
	EXPECT_EQ(child.age, 10u);
	EXPECT_FALSE(child.joinsTown);
}

TEST(ScriptCreate, MiraclesAreNumberedAsTheGameNumbersThem)
{
	EXPECT_EQ(MagicTypeFromScript(0), MagicType::None);
	EXPECT_EQ(MagicTypeFromScript(10), MagicType::Heal);
	EXPECT_EQ(MagicTypeFromScript(11), MagicType::HealPowerUpOne);
	EXPECT_EQ(MagicTypeFromScript(14), MagicType::Food);
	EXPECT_EQ(MagicTypeFromScript(22), MagicType::Water);
	EXPECT_EQ(MagicTypeFromScript(30), MagicType::CreatureSpellStrong);
	EXPECT_EQ(MagicTypeFromScript(34), MagicType::CreatureSpellCompassion);
	EXPECT_EQ(MagicTypeFromScript(41), MagicType::CreatureSpellItchy);
	EXPECT_FALSE(MagicTypeFromScript(42).has_value());
	EXPECT_FALSE(MagicTypeFromScript(-1).has_value());
}

TEST(ScriptCreate, ADispenserIsGivenSecondsAsTurnsOrKeepsItsBuildingsPeriod)
{
	// Ten turns a second, cut down
	EXPECT_EQ(DispenserTurns(30.0f, 300.0f), 300u);
	EXPECT_EQ(DispenserTurns(1.25f, 300.0f), 12u);
	EXPECT_EQ(DispenserTurns(0.05f, 300.0f), 0u);
	// None or fewer: the building's own period, cut down
	EXPECT_EQ(DispenserTurns(0.0f, 300.0f), 300u);
	EXPECT_EQ(DispenserTurns(-5.0f, 250.7f), 250u);
	EXPECT_EQ(DispenserTurns(0.0f, 0.0f), 0u);
}

TEST(ScriptCreate, AWeatherThingBringsASmallStormOfItsKindsWeather)
{
	GWeatherInfo drizzle {};
	drizzle.fadeInOutTime = 77.0f;
	drizzle.lastsFor = 999.0f;
	drizzle.strength = 0.25f;
	drizzle.temperature = 12;
	drizzle.wetness = 40;
	drizzle.snowFall = 0;
	drizzle.overCast = 60;
	drizzle.wind = {5, -3};
	const glm::vec3 centre {1984.0f, 13.9f, 2078.0f};
	const auto storm = WeatherThingStorm(drizzle, centre);
	EXPECT_EQ(storm.centre, centre);
	EXPECT_FLOAT_EQ(storm.innerRadius, 100.0f);
	EXPECT_FLOAT_EQ(storm.outerRadius, 300.0f);
	// Its kind's fading time, life and strength are not used
	EXPECT_FLOAT_EQ(storm.fadeSeconds, 10.0f);
	EXPECT_FLOAT_EQ(storm.lastsFor, 100.0f);
	EXPECT_FLOAT_EQ(storm.strength, 1.0f);
	EXPECT_FLOAT_EQ(storm.cloudHeight, 500.0f);
	EXPECT_FLOAT_EQ(storm.rainSpeed, 1.0f);
	EXPECT_EQ(storm.effect.temperature, 12);
	EXPECT_EQ(storm.effect.rain, 40);
	EXPECT_EQ(storm.effect.snow, 0);
	EXPECT_EQ(storm.effect.overcast, 60);
	EXPECT_EQ(storm.effect.windX, 5);
	EXPECT_EQ(storm.effect.windZ, -3);
}

TEST(ScriptCreate, OnlyACreatureTakesNoNoticeOfReactionsWhenAScriptMakesIt)
{
	EXPECT_TRUE(IgnoresReactions(ScriptType::Creature));
	EXPECT_FALSE(IgnoresReactions(ScriptType::Villager));
	EXPECT_FALSE(IgnoresReactions(ScriptType::Marker));
	EXPECT_FALSE(IgnoresReactions(ScriptType::MobileStatic));
}

TEST(ScriptCreate, AMobileObjectTakesTheRowOfItsType)
{
	// The fourth land's Creed is "create 20 21": the mobile object of row 21
	EXPECT_EQ(MobileObjectRowOf(ScriptType::MobileObject, 21), MobileObjectInfo::Creed);
	EXPECT_EQ(MobileObjectRowOf(ScriptType::MobileObject, 0), MobileObjectInfo::EgyptBarrel);
	// A lump of poo and an ark always take their own rows, whatever the script gives
	EXPECT_EQ(MobileObjectRowOf(ScriptType::Poo, 0), MobileObjectInfo::LumpOfPoo);
	EXPECT_EQ(MobileObjectRowOf(ScriptType::Ark, 7), MobileObjectInfo::Ark);
	// A row past the end of the table and a type that is not a mobile object make nothing
	EXPECT_FALSE(MobileObjectRowOf(ScriptType::MobileObject, static_cast<uint32_t>(MobileObjectInfo::_COUNT)).has_value());
	EXPECT_FALSE(MobileObjectRowOf(ScriptType::MobileStatic, 21).has_value());
	EXPECT_EQ(static_cast<int32_t>(ScriptType::MobileObject), 20);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Poo), 25);
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Ark), 27);
}

TEST(ScriptCreate, AStoreIsAPileOfFoodOrWood)
{
	// The fifth land's crusaders make their pyre with "create 16 1": a store of wood
	EXPECT_EQ(static_cast<int32_t>(ScriptType::Store), 16);
	EXPECT_EQ(StoreResource(0), ResourceType::Food);
	EXPECT_EQ(StoreResource(1), ResourceType::Wood);
	EXPECT_FALSE(StoreResource(2).has_value());
	EXPECT_FALSE(StoreResource(5000).has_value());
}

TEST(ScriptCreate, ACauldronAndDungComeFromTheMobileObjectRules)
{
	// The fifth land's crusaders make their cauldron with "create 20 31" and the brown bear's dung with "create 25 5000"
	EXPECT_EQ(MobileObjectRowOf(ScriptType::MobileObject, 31), MobileObjectInfo::Cauldron);
	EXPECT_EQ(MobileObjectRowOf(ScriptType::Poo, 5000), MobileObjectInfo::LumpOfPoo);
	EXPECT_FALSE(MobileObjectRowOf(ScriptType::Tree, 5).has_value());
}
