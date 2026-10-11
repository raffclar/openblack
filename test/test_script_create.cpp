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
